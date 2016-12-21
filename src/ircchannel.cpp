#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include "logging.hpp"
#include <stdexcept>
#include <utility>
#include <typeinfo>
#include <regex>
#include <memory>
#include <iomanip>

namespace ircChannel {
  IrcChannel::IrcChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _server(_config["server"]),
    _port(_config["port"]),
    _channel(_config["channel"]),
    _ping_time(std::chrono::high_resolution_clock::now()),
    _last_pong_time(std::chrono::high_resolution_clock::now()),
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
      _fd = connect(_server, _port);
      // Wait for server
      int retval = -1;
      while (retval < 0) {
        retval = sys::fcntl(_fd, F_GETFD);
        std::this_thread::sleep_for(std::chrono::milliseconds (100));
      }
      registerConnection();
      startPolling();
      _active = true;
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
      snprintf(message, irc_message_max, "PRIVMSG #%s :[%s]: %s\r\n", _channel.c_str(), textmsg->user()->name().c_str(), textmsg->data().c_str());
      DEBUG << "#irc " << _name << " " << textmsg->data() << " inside ";
      send(message);
    } else if (msg->type() == messaging::MessageType::Action) {
      const auto actionmsg = messaging::ActionMessage::fromMessage(msg);
      snprintf(message, irc_message_max, "PRIVMSG #%s :\001ACTION [%s]: %s\001\r\n", _channel.c_str(), actionmsg->user()->name().c_str(), actionmsg->data().c_str());
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

    std::regex msgRe ("^:(\\S+)!(\\S+)\\s+PRIVMSG\\s+#(\\S+)\\s+:(.*)\r\n$");
    std::regex joinRe("^:(\\S+)!(\\S+)\\s+JOIN\\s+:#(\\S+)$");
    std::regex quitRe("^:(\\S+)!(\\S+)\\s+QUIT\\s+:#(\\S+)$");
    std::regex pingRe("PING\\s+:(.*)\r\n$");
    std::regex pongRe("PONG\\s+(.*)\r\n$");

    std::smatch msgMatches;
    std::string name = "irc";
    std::string text = toParse;
    if (std::regex_search(toParse, msgMatches, pongRe)) {
      DEBUG << "#irc: " << name << "Server pong reply: '" << msgMatches[1];
      pong();
    }
    if (std::regex_search(toParse, msgMatches, pingRe)) {
      const std::string pong = "PONG " + msgMatches[1].str();
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

  void IrcChannel::registerConnection() {
    DEBUG << "Registering IRC connection";

    const std::string nick = _config.get("nickname", "chatsyncbot");
    const std::string mode = _config.get("mode", "*");
    const std::string hostname = _config.get("hostname", "chatsynchost");
    const std::string servername = _config.get("servername", "chatsyncserver");
    const std::string realname = _config.get("realname", "Chat Sync");
    const std::string servicePassword = _config.get("servicepassword", "");
    const auto passline = "PASS *\r\n";
    const auto nickline = "NICK " + nick + "\r\n";
    const auto userline = "USER " + nick + " " + hostname + " " + servername + " :" + realname + "\r\n";
    const auto loginline = "PRIVMSG nickserv :id " + servicePassword + "\r\n";
    const auto joinline = "JOIN #" + _channel + "  \r\n";

    send(passline);
    send(nickline);
    send(userline);
    std::this_thread::sleep_for(std::chrono::milliseconds (1000));
    if (servicePassword.length() > 0) {
      std::async(std::launch::async, [this, loginline]() {
        send(_fd, loginline);
        std::this_thread::sleep_for(std::chrono::milliseconds (1000));
        send(loginline);
      });
    }

    send(joinline);
    std::this_thread::sleep_for(std::chrono::milliseconds (500));
    DEBUG << "#irc: Entered channel";
  }

  void IrcChannel::ping() {
    _ping_time = std::chrono::high_resolution_clock::now();
    DEBUG << "#irc: Sending ping";
    send("PING " + _server + "\r\n");
  }

  void IrcChannel::pong() const {
    const auto& pong_time = std::chrono::high_resolution_clock::now();
    {
      std::unique_lock<std::mutex> lock(_pong_time_mutex);
      _last_pong_time = pong_time;
    }
    _connection_issue = false;
    std::chrono::duration<double> diff = pong_time - _ping_time;
    const auto& pong_time_t = std::chrono::system_clock::to_time_t(pong_time);
    DEBUG << "#irc: last pong at: '" <<
      std::put_time(std::localtime(&pong_time_t), "%F %T") <<
      " and it took " << diff.count() << "s";
  }

  void IrcChannel::checkTimeout() {
    const auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = timestamp - _ping_time;
    if (diff > max_timeout) {
      ping();
      std::this_thread::sleep_for(std::chrono::milliseconds (150));
    }
  }

  void IrcChannel::tick() {
    const auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> last_pong;
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
        _connection_issue = false;
        disconnect(_fd);
      }
    }
  }
}
