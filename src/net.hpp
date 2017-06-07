#pragma once
#include <memory>
#include <stdexcept>

/** This namespace contains network-related inline functions intended to embed
    into clients */
namespace networking {
  namespace os {
    extern "C" {
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
      #include <sys/ioctl.h>
#include <netdb.h>
    }
  }

  /**
   * Generic network error
   */
  class network_error: public std::runtime_error {
  public:
    network_error(std::string const& message) :
      std::runtime_error(message)
    {};
  };

  /**
   * Name resolution error
   */
  class hostname_error: public network_error {
  public:
    hostname_error(std::string const& message) :
      network_error(message)
    {};
  };

  /**
   * Perform the hostname resolution
   *
   * \param hostname Name of host to resolve
   * \returns std::pair<hostent*,bool> hostent is for use in other networking,
   * bool is \c true if ipv6 is used
   * \throws hostname_error if it's impossible to resolve host
   */
  static inline std::pair<os::hostent *, bool> resolve_host(const std::string& hostname) {
    bool ipv6 = false;
    auto server = os::gethostbyname2(hostname.c_str(), AF_INET);
    if (!server) {
      server = os::gethostbyname2(hostname.c_str(), AF_INET6);
      ipv6 = true;
    }
    if (!server)
      throw hostname_error("Can't resolve hostname");
    return std::make_pair(server, ipv6);
  }

  /**
   * Open tcp connection to server
   *
   * \param server Server address as servername.domain:port
   */
  static inline int tcp_connect(const std::string& host) {
    int fd = -1;
    int port = -1;
    std::string url;
    try {
      port = std::stoi(host.substr(host.find(':', 0) + 1, host.length()));
      url = host.substr(0, host.find(':', 0));
    } catch (std::out_of_range& ex) {
      throw network_error("Please provide server address in host:port format");
    } catch (std::invalid_argument& ex) {
      throw network_error("Please provide server address in host:port format");
    }

    os::hostent* server;
    bool ipv6 = false;
    std::tie(server, ipv6) = resolve_host(url);

    if (!ipv6) {
      struct os::sockaddr_in serv_addr;
      fd = socket(AF_INET, os::SOCK_STREAM, 0);
      if (fd < 0)
        throw network_error("Can't create socket");

      os::memset(&serv_addr, 0, sizeof(struct os::sockaddr_in));
      serv_addr.sin_family = AF_INET;
      os::memcpy((char *) server->h_addr,
                 (char *) &serv_addr.sin_addr.s_addr,
                 server->h_length);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using os::htons;
#endif
        serv_addr.sin_port = htons(port);
      }
      if (connect(fd, (struct os::sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw network_error("Can't connect to server");
    } else {
      struct os::sockaddr_in6 serv_addr;

      fd = socket(AF_INET6, os::SOCK_STREAM, 0);
      if (fd < 0)
        throw network_error("Can't create ipv6 socket");
      os::memset(&serv_addr, 0, sizeof(struct os::sockaddr_in6));
      serv_addr.sin6_family = AF_INET6;
      os::memcpy((char *) server->h_addr,
                 (char *) &serv_addr.sin6_addr,
                 server->h_length);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using os::htons;
#endif
        serv_addr.sin6_port = htons(port);
      }
      if (os::connect(fd, (struct os::sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw network_error("Can't connect to server using ipv6");
    }
    return fd;
  }
}
