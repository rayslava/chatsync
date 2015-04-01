#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include <stdexcept>
#include <utility>
#include <typeinfo>
#include <regex>
#include <memory>

namespace ircChannel {
    IrcChannel::IrcChannel(Hub::Hub* hub, const std::string& config):
	channeling::Channel(hub, config),
	_server(_config["server"]),
	_port(_config["port"]),
	_channel(_config["channel"])
    {
    }

    std::future<void> IrcChannel::activate() {
	return std::async(std::launch::async, [this]() {
		if (_active)
                    return;
                _fd = connect(_server, _port);
                startPolling();
		std::async(std::launch::async, [this]() {
		    std::this_thread::sleep_for(std::chrono::milliseconds (2000));
		    registerConnection();
		  });
                _active = true;
	    });}

    IrcChannel::~IrcChannel() {
	disconnect();
	stopPolling();
    }

    void IrcChannel::incoming(const messaging::message_ptr&& msg) {
	char message[irc_message_max];

        if (msg->type() == messaging::MessageType::Text) {
            const auto textmsg = messaging::TextMessage::fromMessage(msg);
            snprintf(message, irc_message_max, "PRIVMSG #%s :[%s]: %s\r\n", _channel.c_str(), textmsg->user()->name().c_str(), textmsg->data().c_str());
            std::cerr << "[DEBUG] #irc" << _name << " " << textmsg->data() << " inside " << _name << std::endl;
            send(message);
        }
    }

    const messaging::message_ptr IrcChannel::parse(const char* line) const
    {
        // :rayslava!~v.barinov@212.44.150.238 PRIVMSG #chatsync :ololo
        const std::string toParse(line);
        std::cerr << "[DEBUG] Parsing irc line:" << toParse << std::endl;

	std::regex msgRe(":(\\S+)!(\\S+)\\s+PRIVMSG\\s+#(\\S+)\\s+:(.*)\r\n$");
	std::regex pingRe("PING\\s+:(.*)\r\n$");

	std::smatch msgMatches;
        std::string name = "irc";
        std::string text = toParse;
        if (std::regex_search(toParse, msgMatches, msgRe)) {
            name = msgMatches[1].str();
            text = msgMatches[4].str();
            std::cerr << "[DEBUG] #irc:" << name << ": " << text << std::endl;
        };
	if (std::regex_search(toParse, msgMatches, pingRe)) {
            const std::string pong = "PONG " + msgMatches[1].str();
            std::cerr << "[DEBUG] #irc: sending " << pong << std::endl;
            send(pong);
        };

        const auto msg = std::make_shared<const messaging::TextMessage>(_id,
	    std::move(std::make_shared<const messaging::User>(messaging::User(name.c_str()))),
	    text.c_str());
        return msg;
    }


    int IrcChannel::registerConnection() {
	const std::string nick = _config.get("nickname", "chatsyncbot");
	const std::string mode = _config.get("mode", "*");
	const std::string hostname = _config.get("hostname", "chatsynchost");
	const std::string servername = _config.get("servername", "chatsyncserver");
	const std::string realname = _config.get("realname", "Chat Sync");
	const std::string servicePassword = _config.get("servicepassword", "");
	const auto passline = "PASS *\r\n";
	const auto nickline = "NICK " + nick + "\r\n";
	const auto userline = "USER " + nick + " " + hostname + " " + servername + " :"  + realname + "\r\n";
	const auto loginline = "PRIVMSG nickserv :id " + servicePassword + "\r\n";
	const auto joinline = "JOIN #" + _channel + "  \r\n";

	send(passline);
	send(nickline);
	send(userline);
	std::this_thread::sleep_for(std::chrono::milliseconds (1000));
	if (servicePassword.length() > 0) {
	  std::async(std::launch::async, [this,loginline]() {
	      send(_fd, loginline);
	      std::this_thread::sleep_for(std::chrono::milliseconds (1000));
	      send(loginline);
	    });
	}
	send(joinline);
	std::this_thread::sleep_for(std::chrono::milliseconds (500));
	send("PRIVMSG #" + _channel + " :Hello there\r\n");
	return 0;
    }

}
