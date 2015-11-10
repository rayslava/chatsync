#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include <stdexcept>
#include <utility>
#include <typeinfo>
#include <regex>
#include <memory>

namespace ircChannel {
  IrcChannel::IrcChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _server(_config["server"]),
    _port(_config["port"]),
    _channel(_config["channel"]),
    _ping_time(std::chrono::high_resolution_clock::now())
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
    disconnect();
    stopPolling();
  }

  void IrcChannel::incoming(const messaging::message_ptr&& msg) {
    char message[irc_message_max];

    checkTimeout();
    if (msg->type() == messaging::MessageType::Text) {
      const auto textmsg = messaging::TextMessage::fromMessage(msg);
      snprintf(message, irc_message_max, "PRIVMSG #%s :[%s]: %s\r\n", _channel.c_str(), textmsg->user()->name().c_str(), textmsg->data().c_str());
      std::cerr << "[DEBUG] #irc " << _name << " " << textmsg->data() << " inside " << std::endl;
      send(message);
    } else if (msg->type() == messaging::MessageType::Action) {
      const auto actionmsg = messaging::ActionMessage::fromMessage(msg);
      snprintf(message, irc_message_max, "PRIVMSG #%s :\001ACTION[%s]: %s\001\r\n", _channel.c_str(), actionmsg->user()->name().c_str(), actionmsg->data().c_str());
      std::cerr << "[DEBUG] #irc " << _name << " performes an action: " << actionmsg->data() << std::endl;
      send(message);
    } else {
      throw std::runtime_error("Unknown message type");
    }

  }

  const messaging::message_ptr IrcChannel::parse(const char* line) const
  {
    // :rayslava!~v.barinov@212.44.150.238 PRIVMSG #chatsync :ololo
    const std::string toParse(line);
    std::cerr << "[DEBUG] Parsing irc line:" << toParse << std::endl;

    std::regex msgRe (":(\\S+)!(\\S+)\\s+PRIVMSG\\s+#(\\S+)\\s+:(.*)\r\n$");
    std::regex joinRe(":(\\S+)!(\\S+)\\s+JOIN\\s+:#(\\S+)$");
    std::regex quitRe(":(\\S+)!(\\S+)\\s+QUIT\\s+:#(\\S+)$");
    std::regex pingRe("PING\\s+:(.*)\r\n$");
    std::regex pongRe("PONG\\s+(.*)\r\n$");

    std::smatch msgMatches;
    std::string name = "irc";
    std::string text = toParse;
    if (std::regex_search(toParse, msgMatches, pongRe)) {
      const auto ping_end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> diff = ping_end - _ping_time;
      std::cerr << "[DEBUG] #irc: " << name << "Server pong reply: '" << msgMatches[1].str() << "' in " << diff.count() << "s" << std::endl;
    }
    if (std::regex_search(toParse, msgMatches, pingRe)) {
      const std::string pong = "PONG " + msgMatches[1].str();
      std::cerr << "[DEBUG] #irc: sending " << pong << std::endl;
      send(pong);
    };
    if (std::regex_search(toParse, msgMatches, joinRe)) {
      const auto name = msgMatches[1].str();
      const auto host = msgMatches[2].str();
      const auto chan = msgMatches[3].str();
      std::cerr << "[DEBUG] #irc: user " << name << " has joined " << chan << " from " << host << std::endl;
    };
    if (std::regex_search(toParse, msgMatches, quitRe)) {
      const auto name = msgMatches[1].str();
      const auto host = msgMatches[2].str();
      const auto msg = msgMatches[3].str();
      std::cerr << "[DEBUG] #irc: user " << name << " has left because of " << msg << std::endl;
    };
    if (std::regex_search(toParse, msgMatches, msgRe)) {
      name = msgMatches[1].str();
      text = msgMatches[4].str();
      std::regex actionRe("\001ACTION (.*)\001");
      if (std::regex_search(text, msgMatches, actionRe)) {
        text = msgMatches[1].str();
        std::cerr << "[DEBUG] #irc:" << name << "[ACTION]: " << text << std::endl;

        const auto msg = std::make_shared<const messaging::ActionMessage>(_id,
                                                                          std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                          text.c_str());
        return msg;
      }
      std::cerr << "[DEBUG] #irc:" << name << ": " << text << std::endl;
      const auto msg = std::make_shared<const messaging::TextMessage>(_id,
                                                                      std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                      text.c_str());
      return msg;
    };
    return nullptr;
  }

  void IrcChannel::registerConnection() {
    std::cerr << "[DEBUG] Registering IRC connection" << std::endl;

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
    send("PRIVMSG #" + _channel + " :Hello there\r\n");
  }

  void IrcChannel::ping() {
    _ping_time = std::chrono::high_resolution_clock::now();
    std::cerr << "[DEBUG] #irc: Sending ping" << std::endl;
    send("PING " + _server + "\r\n");
  }

  void IrcChannel::checkTimeout() {
    const auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = timestamp - _ping_time;
    if (diff > max_timeout) {
      ping();
      std::this_thread::sleep_for(std::chrono::milliseconds (150));
    }
  }
}
