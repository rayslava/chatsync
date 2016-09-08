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
    _reconnect_timeout(5000),
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
    const auto message = messaging::TextMessage::fromMessage(msg);
    std::cerr << "[DEBUG] Incoming message " << message->data() << std::endl;
    std::async(std::launch::async, [&channel, msg = std::move(msg)]()
    {
      channel.incoming(std::move(msg));
    });
    return channel;
  }

  void Channel::startPolling() {
    if (_fd < 0) {
      std::cerr << "[DEBUG] Channel " << _name << " fd < 0, reconnecting" << std::endl;
      reconnect();
      return;
    }
    _pipeRunning = true;
    _thread = std::make_unique<std::thread>(std::thread(&Channel::pollThread, this));
  }

  void Channel::stopPolling() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
  }

  void Channel::reconnect() {
    if (!_pipeRunning) {
      std::cerr << "[DEBUG] Channel " << _name << " trying to reconnect after stop." << std::endl;
      return;
    }

    const unsigned int timeout = _config.get("reconnect_timeout", "5000");
    const unsigned int max_repeats = _config.get("max_reconnects", "3");
    if (_reconnect_timeout.count() > timeout * max_repeats)
      throw connection_error(_name, "Maximum number of reconnections reached");
    stopPolling();
    disconnect();

    std::cerr << "[DEBUG] Channel " << name() << ": file descriptor closed" << std::endl;

    activate().wait();
    if (_pipeRunning) {
      // Successfully reconnected
      const unsigned int timeout = _config.get("reconnect_timeout", "5000");
      _reconnect_timeout = std::chrono::milliseconds(timeout);
      return;
    }

    _reconnect_timeout *= 2;

    std::async(std::launch::async, [this]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(_reconnect_timeout));
      reconnect();
    });
  }

  Channel * ChannelFactory::create(const std::string& classname, Hub::Hub * const hub, const std::string& config) {
    std::map<std::string, ChannelCreator *>::iterator i;
    i = get_table().find(classname);

    if (i != get_table().end())
      return i->second->create(hub, config);
    else
      return (Channel *) NULL;
  }

  std::map<std::string, ChannelCreator *>& ChannelFactory::get_table() {
    static std::map<std::string, ChannelCreator *> table;
    return table;
  }

  void ChannelFactory::registerClass(const std::string& classname, ChannelCreator* creator) {
    get_table()[classname] = creator;
  }

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
    const int readFd = _fd;
    fd_set readset;
    int err = 0;
    const unsigned int select_timeout = _config.get("poll_time", "5");
    // Implement the receiver loop
    while (_pipeRunning) {
      // Initialize time out struct for select()
      struct timeval tv;
      tv.tv_sec = select_timeout;
      tv.tv_usec = 0;
      // Initialize the set
      FD_ZERO(&readset);
      FD_SET(readFd, &readset);
      // Now, check for readability
      errno = 0;
      err = select(readFd + 1, &readset, NULL, NULL, &tv);
      if (err < 0 || net::fcntl(readFd, F_GETFD) == -1 || errno == EBADF) {
        std::cerr << "[DEBUG] select() failed. Reconnecting channel " << name() << std::endl;
        reconnect();
      }
      if (err > 0 && FD_ISSET(readFd, &readset)) {
        // Clear flags
        FD_CLR(readFd, &readset);
        // Check available size
        int bytes;
        err = net::ioctl(readFd, FIONREAD, &bytes);
        if (err < 0 || bytes == 0) {
          std::cerr << "[DEBUG] ioctl() failed. Reconnecting channel " << name() << std::endl;
          reconnect();
          break;
        }
        auto line = new char[bytes + 1];
        line[bytes] = 0;
        // Do a simple read on data
        err = net::read(readFd, line, bytes);
        if (err != bytes)
          throw std::runtime_error(ERR_SOCK_READ);
        // Parse received data
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
        throw channeling::activate_error(_name, ERR_SOCK_CREATE);

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
        throw channeling::activate_error(_name, ERR_CONNECTION);
    } else {
      /* IPv6 resolution */
      std::cerr << "[DEBUG] Connecting through IPv6" << std::endl;
      struct net::sockaddr_in6 serv_addr;
      struct servent* sp;
      server = net::gethostbyname2(hostname.c_str(), AF_INET6);
      if (server == NULL)
        throw channeling::activate_error(_name, ERR_HOST_NOT_FOUND);

      sockfd = net::socket(AF_INET6, net::SOCK_STREAM, 0);
      if (sockfd < 0)
        throw channeling::activate_error(_name, ERR_SOCK_CREATE);

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
        throw channeling::activate_error(_name, ERR_CONNECTION);
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
