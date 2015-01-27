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

    void IrcChannel::activate() {
        if (_direction == Channeling::ChannelDirection::Input) {
            _fd = connect();
            startPolling();
        };
    }

    IrcChannel::~IrcChannel() {
	disconnect();
	stopPolling();
    }

    void IrcChannel::parse(const std::string &l) {
        std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
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
	}
    }
    int IrcChannel::connect() {

	int sockfd, n;
	struct net::sockaddr_in serv_addr;
	struct net::hostent *server;

	char buffer[256];

	std::cout << "Joining" << _channel << std::endl;
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
