#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(Hub::Hub* hub, const std::string&& config):
	Channeling::Channel(hub, std::move(config)),
	_server(_config["server"]),
	_port(_config["port"]),
	_channel(_config["channel"])
    {
    }

    std::future<void> IrcChannel::activate() {
	return std::async(std::launch::async, [this]() {
		if (_direction == Channeling::ChannelDirection::Input) {
		    _fd = connect();
		    startPolling();
		    registerConnection();
		};
	    });}

    IrcChannel::~IrcChannel() {
	disconnect();
	stopPolling();
    }

    void IrcChannel::incoming(const messaging::message_ptr&& msg) {
        std::cerr << "[DEBUG] #irc" << _name << " " << msg->data() << " inside " << _name << std::endl;
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
#include <arpa/inet.h>	   
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
	    throw Channeling::activate_error(ERR_SOCK_CREATE);
	server = net::gethostbyname(_server.c_str());

	if (server == NULL)
	    throw Channeling::activate_error(ERR_HOST_NOT_FOUND);

	net::bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	net::bcopy((char *)server->h_addr,
	      (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	serv_addr.sin_port = net::htons(_port);
	if (net::connect(sockfd,(struct net::sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
	    throw Channeling::activate_error(ERR_CONNECTION);
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
