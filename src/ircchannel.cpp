#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include "logging.hpp"
#include <stdexcept>
#include <string>
#include <utility>
#include <typeinfo>
#include <regex>
#include <memory>
#include <iomanip>

namespace ircChannel {
  const channeling::ChannelCreatorImpl<IrcChannel> IrcChannel::creator("irc");

  IrcChannel::IrcChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _server(_config["server"]),
    _port(_config["port"]),
    _channel(_config["channel"]),
    _ping_time(std::chrono::system_clock::now()),
    _last_pong_time(std::chrono::system_clock::now()),
    _connection_issue(ATOMIC_FLAG_INIT)
  {}

  namespace sys {
    extern "C" {
#include <unistd.h>
#include <fcntl.h>
    }
  }

  std::future<void> IrcChannel::activate() {
    return std::async(std::launch::async, [this]() {
      if (_active)
        return;
      try {
        _fd = connect(_server, _port);
      } catch (std::exception& ex) {
        throw channeling::activate_error(_name, ex.what());
      }
      // Wait for server
      int retval = -1;
      while (retval < 0) {
        retval = sys::fcntl(_fd, F_GETFD);
        std::this_thread::sleep_for(std::chrono::milliseconds (10));
      }
      _active = true;
      registerConnection();
      startPolling();
    });
  }

  IrcChannel::~IrcChannel() {
    stopPolling();
    disconnect();
  }

  void IrcChannel::incoming(const messaging::message_ptr&& msg) {
    char message[irc_message_max];

    checkTimeout();
    if (msg->type() == messaging::MessageType::Text) {
      const auto textmsg = messaging::TextMessage::fromMessage(msg);
      const unsigned int max_msg_len = irc_message_max - 1 -
                                       strlen("PRIVMSG # :[]: \r\n") -
                                       textmsg->user()->name().length() -
                                       _channel.length();
      std::string text = textmsg->data();
      if (text.length() < max_msg_len) {
        snprintf(message, irc_message_max - 1, "PRIVMSG #%s :[%s]: %.*s\r\n",
                 _channel.c_str(), textmsg->user()->name().c_str(),
                 max_msg_len,
                 text.c_str());
        DEBUG << "#irc " << _name << " " << textmsg->data() << " inside ";
        send(message);
      } else {
        /**
         * If message does not fit \ref irc_message_max we have to split message
         * into several smaller ones.
         */
        while (text.length()) {
          /* Find nearest space char */
          auto last_space_pos = text.crend() - max_msg_len;
          const auto space = std::find_if(last_space_pos, text.crend(), ::isspace);
          size_t part_len = 0;
          if (space != text.crend()) {
            /* Found space to split the line */
            part_len = std::distance(space, text.crend());
          } else {
            part_len = max_msg_len;
          }
          std::string part = text.substr(0, part_len);

          snprintf(message, irc_message_max - 1, "PRIVMSG #%s :[%s]: %.*s\r\n",
                   _channel.c_str(), textmsg->user()->name().c_str(),
                   max_msg_len,
                   part.c_str());
          DEBUG << "#irc " << _name << " sending part| " << part;
          send(message);
          text.erase(0, part_len);
        }
      }
    } else if (msg->type() == messaging::MessageType::Action) {
      const auto actionmsg = messaging::ActionMessage::fromMessage(msg);
      const int max_msg_len = irc_message_max - 1 -
                              strlen("PRIVMSG #%s :\001ACTION [%s]: %.*s\001\r\n") -
                              actionmsg->user()->name().length() -
                              _channel.length();
      snprintf(message, irc_message_max - 1, "PRIVMSG #%s :\001ACTION [%s]: %.*s\001\r\n",
               _channel.c_str(), actionmsg->user()->name().c_str(),
               max_msg_len,
               actionmsg->data().c_str());
      DEBUG << "#irc " << _name << " performes an action: " << actionmsg->data();
      send(message);
    } else {
      throw std::runtime_error("Unknown message type");
    }
  }

  const messaging::message_ptr IrcChannel::parse(const char* line) const {
    // :rayslava!~v.barinov@212.44.150.238 PRIVMSG #chatsync :ololo
    const std::string toParse(line);
    std::vector<std::string> lines;
    std::string::size_type pos = 0, lastpos = 0;
    while ((pos = toParse.find("\r\n", lastpos, 2)) != std::string::npos) {
      lines.push_back(toParse.substr(lastpos, pos - lastpos + 2));
      lastpos = pos + 2;
    };

    const auto lastitem = std::move(lines.back());
    lines.pop_back();

    for (const auto& item : lines)
      _hub->newMessage(parseImpl(item));

    return parseImpl(lastitem);
  }

  const messaging::message_ptr IrcChannel::parseImpl(const std::string& toParse) const
  {
    DEBUG << "Parsing irc line:" << toParse;

    const static std::regex msgRe ("^:(\\S+)!(\\S+)\\s+PRIVMSG\\s+#(\\S+)\\s+:(.*)\r\n$");
    const static std::regex joinRe("^:(\\S+)!(\\S+)\\s+JOIN\\s+:#(\\S+)$");
    const static std::regex quitRe("^:(\\S+)!(\\S+)\\s+QUIT\\s+:#(\\S+)$");
    const static std::regex pingRe("PING\\s+:(.*)\r\n$");
    const static std::regex pongRe("PONG\\s+(.*)\r\n$");
    const static std::regex errNotReg(".*You have not registered.*\r\n$");

    std::smatch msgMatches;
    std::string name = "irc";
    std::string text = toParse;
    if (std::regex_search(toParse, msgMatches, errNotReg)) {
      WARNING << "#irc: " << name << "Server says that connection not registered : '";
      registerConnection();
      return nullptr;
    }
    if (std::regex_search(toParse, msgMatches, pongRe)) {
      DEBUG << "#irc: " << name << "Server pong reply: '" << msgMatches[1];
      pong();
    }
    if (std::regex_search(toParse, msgMatches, pingRe)) {
      const std::string pong = "PONG " + msgMatches[1].str() + "\r\n";
      DEBUG << "#irc: sending " << pong;
      send(pong);
    };
    if (std::regex_search(toParse, msgMatches, joinRe)) {
      const auto name = msgMatches[1].str();
      const auto host = msgMatches[2].str();
      const auto chan = msgMatches[3].str();
      DEBUG << "#irc: user " << name << " has joined " << chan << " from " << host;
    };
    if (std::regex_search(toParse, msgMatches, quitRe)) {
      const auto name = msgMatches[1].str();
      const auto host = msgMatches[2].str();
      const auto msg = msgMatches[3].str();
      DEBUG << "#irc: user " << name << " has left because of " << msg;
    };
    if (std::regex_search(toParse, msgMatches, msgRe)) {
      name = msgMatches[1].str();
      text = msgMatches[4].str();
      std::regex actionRe("\001ACTION (.*)\001");
      if (std::regex_search(text, msgMatches, actionRe)) {
        text = msgMatches[1].str();
        DEBUG << "#irc:" << name << "[ACTION]: " << text;
        const auto msg = std::make_shared<const messaging::ActionMessage>(_id,
                                                                          std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                          text.c_str());
        return msg;
      }
      DEBUG << "#irc:" << name << ": " << text;
      const auto msg = std::make_shared<const messaging::TextMessage>(_id,
                                                                      std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                      text.c_str());
      return msg;
    };
    return nullptr;
  }

  static inline std::string getstring(int fd) {
    char c{0};
    std::string result{};
    while (c != '\n') {
      int res = sys::read(fd, &c, 1);
      if (res > 0)
        result.push_back(c);
      else if (res == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds (50));
      else
        throw std::runtime_error("Socket closed");
    }
    return result;
  }

  void IrcChannel::registerConnection() const {
    DEBUG << "Registering IRC connection";

    const std::string nick = _config.get("nickname", "chatsyncbot");
    const std::string login = _config.get("login", nick.c_str());
    const std::string mode = _config.get("mode", "0");
    const std::string realname = _config.get("realname", "Chat Sync");
    const std::string servicePassword = _config.get("password", "");
    const bool auth = (servicePassword.length() > 0);

    const auto nickline = "NICK " + nick + "\r\n";
    const auto userline = "USER " + nick + " " + mode + " * :" + realname + "\r\n";
    const auto loginline = "PRIVMSG nickserv :id " + servicePassword + "\r\n";
    const auto passline = "PASS " + (auth ?
                                     (login + ":" + servicePassword) : "*") + "\r\n";
    const auto joinline = "JOIN #" + _channel + "  \r\n";
    const auto regline = passline + nickline + userline;

    static char buf[1024];
    std::string toParse;
    int result = 0;
    std::smatch msgMatches;

    const static std::regex welcome("\\s+001\\s+");
    const static std::regex identreq("NOTICE.*?hostname.*?\r\n");

    while (!std::regex_search(toParse, msgMatches, identreq)) {
      toParse = getstring(_fd);
      TRACE << toParse;
    }

    DEBUG << "Ident requested, registering";
    send(regline);

    // Wait for the welcome message
    while (!std::regex_search(toParse, msgMatches, welcome)) {
      toParse = getstring(_fd);
      TRACE << toParse;
    }

    if (auth)
      send(loginline);

    send(joinline);
    DEBUG << "#irc: Registered connection";
  }

  void IrcChannel::ping() {
    _ping_time = std::chrono::system_clock::now();
    DEBUG << "#irc: Sending ping";
    send("PING " + _server + "\r\n");
  }

  void IrcChannel::pong() const {
    const auto& pong_time = std::chrono::system_clock::now();
    {
      std::unique_lock<std::mutex> lock(_pong_time_mutex);
      _last_pong_time = pong_time;
    }
    _connection_issue = false;
    std::chrono::duration<double> diff = pong_time - _ping_time;
    DEBUG << "#irc: last pong took " << diff.count() << "s";
  }

  void IrcChannel::checkTimeout() {
    const auto timestamp = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = timestamp - _ping_time;
    if (diff > max_timeout) {
      ping();
      std::this_thread::sleep_for(std::chrono::milliseconds (150));
    }
  }

  void IrcChannel::tick() {
    const auto timestamp = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> last_pong;
    {
      std::unique_lock<std::mutex> lock(_pong_time_mutex);
      last_pong = _last_pong_time;
    }
    std::chrono::duration<double> diff = timestamp - last_pong;
    if (diff > max_timeout * 5) {
      if (!_connection_issue) {
        /* Something happened to our connection */
        DEBUG << "#irc: Got connection issue";
        _connection_issue = true;
        ping();
      } else {
        /* Drop the descriptor to initiate reconnect() */
        DEBUG << "#irc: Connection failure detected. Closing _fd.";
        disconnect(_fd);
        _fd = -1;
      }
    }
  }
}
