#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>
#include <utility>
#include <memory>

#include "messages.hpp"

namespace channeling {
  std::atomic_int ChannelFactory::id {ATOMIC_FLAG_INIT};

  Channel::Channel(Hub::Hub * const hub, const std::string& config) :
    _active(ATOMIC_FLAG_INIT),
    _thread(nullptr),
    _pipeRunning(ATOMIC_FLAG_INIT),
    _fd(-1),
    _config(config),
    _name(_config["name"]),
    _direction(_config["direction"]),
    _hub(hub),
    _id(ChannelFactory::nextId())
  {
    std::cout << _name << " : " << _id << std::endl;
    _hub->addChannel(this);
  }

  std::string const& Channel::name() const {
    return _name;
  }

  Channel& operator>> (const message_ptr msg, Channel& channel) {
    if (channel.direction() == channeling::ChannelDirection::Input)
      throw std::logic_error("Can't write data to input channel " + channel.name());
    if (msg->type() == messaging::MessageType::Text) {
      const auto message = messaging::TextMessage::fromMessage(msg);
      std::cerr << "[DEBUG] Incoming message " << message->data() << std::endl;
      std::async(std::launch::async, [&channel, msg = std::move(msg)]()
      {
        channel.incoming(std::move(msg));
      });
    }
    return channel;
  }

  void Channel::startPolling() {
    if (_fd < 0)
      throw std::runtime_error(ERR_FD);
    _pipeRunning = true;
    _thread = std::make_unique<std::thread>(std::thread(&Channel::pollThread, this));
  }

  void Channel::stopPolling() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
  }


  Channel * ChannelFactory::create(const std::string& classname, Hub::Hub * const hub, const std::string& config) {
    std::map<std::string, ChannelCreator *>::iterator i;
    i = get_table().find(classname);

    if (i != get_table().end())
      return i->second->create(hub, config);
    else
      return (Channel *) NULL;
  }

  void ChannelFactory::registerClass(const std::string& classname, ChannelCreator* creator) {
    get_table()[classname] = creator;
  }

  std::map<std::string, ChannelCreator *>& ChannelFactory::get_table() {
    static std::map<std::string, ChannelCreator *> table;
    return table;
  }

  // have the creator's constructor do the registration
  ChannelCreator::ChannelCreator(const std::string& classname) {
    ChannelFactory::registerClass(classname, this);
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

  void Channel::pollThread() {
    // Form descriptor
    const int readFd = _fd;
    fd_set readset;
    int err = 0;
    // Implement the receiver loop
    while (_pipeRunning) {
      // Initialize time out struct for select()
      struct timeval tv;
      tv.tv_sec = _config.get("poll_time", "5");
      tv.tv_usec = 0;
      // Initialize the set
      FD_ZERO(&readset);
      FD_SET(readFd, &readset);
      // Now, check for readability
      err = select(readFd + 1, &readset, NULL, NULL, &tv);
      std::cerr << "[DEBUG] Selected fd " << err << std::endl;
      if (err > 0 && FD_ISSET(readFd, &readset)) {
        // Clear flags
        FD_CLR(readFd, &readset);
        // Check available size
        int bytes;
        net::ioctl(readFd, FIONREAD, &bytes);
        auto line = new char[bytes + 1];
        line[bytes] = 0;
        // Do a simple read on data
        err = net::read(readFd, line, bytes);
        if (err != bytes)
          throw std::runtime_error(ERR_SOCK_READ);
        // Dump read data
        if (direction() == ChannelDirection::Input || direction() == ChannelDirection::Bidirectional)
          _hub->newMessage(parse(line));
        delete[] line;
      }
    }
  }


  int Channel::connect(const std::string& hostname, const uint32_t port) const {
    int sockfd, n;
    struct net::hostent* server;

    char buffer[256];

    /* IPv4 resolution */
    server = net::gethostbyname2(hostname.c_str(), AF_INET);
    if (server != NULL) {
      std::cerr << "[DEBUG] Connecting through IPv4" << std::endl;
      struct net::sockaddr_in serv_addr;
      sockfd = net::socket(AF_INET, net::SOCK_STREAM, 0);
      if (sockfd < 0)
        throw channeling::activate_error(ERR_SOCK_CREATE);

      net::bzero((char *) &serv_addr, sizeof(struct net::sockaddr_in));
      serv_addr.sin_family = AF_INET;
      net::bcopy((char *) server->h_addr,
                 (char *) &serv_addr.sin_addr.s_addr,
                 server->h_length);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using net::htons;
#endif
        serv_addr.sin_port = htons(port);
      }
      if (net::connect(sockfd, (struct net::sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw channeling::activate_error(ERR_CONNECTION);
    } else {
      /* IPv6 resolution */
      std::cerr << "[DEBUG] Connecting through IPv6" << std::endl;
      struct net::sockaddr_in6 serv_addr;
      struct servent* sp;
      server = net::gethostbyname2(hostname.c_str(), AF_INET6);
      if (server == NULL)
        throw channeling::activate_error(ERR_HOST_NOT_FOUND);

      sockfd = net::socket(AF_INET6, net::SOCK_STREAM, 0);
      if (sockfd < 0)
        throw channeling::activate_error(ERR_SOCK_CREATE);

      net::bzero((char *) &serv_addr, sizeof(struct net::sockaddr_in6));
      serv_addr.sin6_family = AF_INET6;
      net::bcopy((char *) server->h_addr,
                 (char *) &serv_addr.sin6_addr,
                 server->h_length);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using net::htons;
#endif
        serv_addr.sin6_port = htons(port);
      }
      if (net::connect(sockfd, (struct net::sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw channeling::activate_error(ERR_CONNECTION);
    }

    return sockfd;
  }

  int Channel::send(const uint32_t fd, const std::string& msg) const {
    const int n = net::write(fd, msg.c_str(), msg.length());
    if (n < 0)
      throw std::runtime_error(ERR_SOCK_WRITE);

    return n;
  }

  int Channel::disconnect(const uint32_t fd) const {
    if (_fd > 0)
      net::close(fd);
    return 0;
  }

}
