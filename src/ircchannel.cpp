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
                _fd = connect();
                startPolling();
                registerConnection();
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
            snprintf(message, 256, "PRIVMSG #chatsync :[%s]: %s\r\n", textmsg->user()->name().c_str(), textmsg->data().c_str());
            std::cerr << "[DEBUG] #irc" << _name << " " << textmsg->data() << " inside " << _name << std::endl;
            sendMessage(message);
        }
    }

    const messaging::message_ptr IrcChannel::parse(const char* line) const
    {
        // :rayslava!~v.barinov@212.44.150.238 PRIVMSG #chatsync :ololo
        const std::string toParse(line);
        std::cerr << "[DEBUG] Parsing irc line:" << toParse << std::endl;
	std::regex msgRe("^:(\\S+)!~(\\S+)\\s+PRIVMSG\\s+#(\\S+)\\s+:(.*)\r\n$");
	std::smatch msgMatches;
        std::string name = "irc";
        std::string text = "parse error";
        if (std::regex_match(toParse, msgMatches, msgRe)) {
            name = msgMatches[1].str();
            text = msgMatches[4].str();
            std::cerr << "[DEBUG] #irc:" << name << ": " << text << std::endl;
        }

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
	const auto passline = "PASS *\r\n";
	const auto nickline = "NICK " + nick + "\r\n";
	const auto userline = "USER " + nick + " " + hostname + " " + servername + ":"  + realname + "\r\n";
	const auto joinline = "JOIN " + _channel + "  \r\n";

	sendMessage(passline);
	sendMessage(nickline);
	sendMessage(userline);
	std::this_thread::sleep_for(std::chrono::milliseconds (500));

	sendMessage(joinline);
	std::this_thread::sleep_for(std::chrono::milliseconds (500));
	sendMessage("PRIVMSG #chatsync :Hello there\r\n");
	return 0;
    }
    /* OS interaction code begins here */
    namespace net {
	extern "C" {
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stropts.h>
	}
    }
    int IrcChannel::connect() {

	int sockfd, n;
	struct net::sockaddr_in serv_addr;
	struct net::hostent *server;

	char buffer[256];

	std::cerr << "[DEBUG] Joining" << _channel << std::endl;
	sockfd = net::socket(AF_INET, net::SOCK_STREAM, 0);
	if (sockfd < 0)
	    throw channeling::activate_error(ERR_SOCK_CREATE);
	server = net::gethostbyname(_server.c_str());

	if (server == NULL)
	    throw channeling::activate_error(ERR_HOST_NOT_FOUND);

	::bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	::bcopy((char *)server->h_addr,
	      (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	{
	  /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
	  using net::htons;
#endif
	  serv_addr.sin_port = htons(_port);
	}
	if (net::connect(sockfd,(struct net::sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
	    throw channeling::activate_error(ERR_CONNECTION);
	return sockfd;
    }

    int IrcChannel::sendMessage(const std::string &msg) {
	const int n = net::write(_fd,msg.c_str(),msg.length());

	if (n < 0)
	    throw std::runtime_error(ERR_SOCK_WRITE);

	return n;
    }

    int IrcChannel::disconnect() {
	if (_fd > 0)
	    net::close(_fd);
	return 0;
    }

}
