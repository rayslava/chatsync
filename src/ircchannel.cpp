#include "ircchannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub,
			   const std::string& ircServer, const uint32_t port, const std::string& channel):
	Channeling::Channel(name, direction, hub)
    {

	if (direction == Channeling::ChannelDirection::Input) {
	    _fd = join(ircServer, port, channel);
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
#include <fcntl.h>           /* Definition of AT_* constants */
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stropts.h>

    int IrcChannel::join(const std::string &ircServer, const uint32_t port, const std::string& channel) {

	int sockfd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[256];

	std::cout << "Joining" << channel << std::endl;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	    throw std::runtime_error(ERR_SOCK_CREATE);
	server = gethostbyname(ircServer.c_str());

	if (server == NULL)
	    throw std::runtime_error(ERR_HOST_NOT_FOUND);

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
	      (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	serv_addr.sin_port = htons(port);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
	    throw std::runtime_error(ERR_CONNECTION);
	return sockfd;
    }

    int IrcChannel::sendMessage(const std::string &msg) {
	const int n = write(_fd,msg.c_str(),msg.length());

	if (n < 0)
	    throw std::runtime_error(ERR_SOCK_WRITE);

	return n;
    }

    int IrcChannel::disconnect() {
	if (_fd > 0)
	    close(_fd);
	return 0;
    }

}
