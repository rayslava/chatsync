#include "net.hpp"

/** This namespace contains network-related inline functions intended to embed
    into clients */
namespace networking {
  namespace os {
    extern "C" {
#include <netinet/in.h>
#include <arpa/inet.h>
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
   * Perform the hostname resolution
   *
   * \param hostname Name of host to resolve
   * \returns std::pair<hostent*,bool> hostent is for use in other networking,
   * bool is \c true if ipv6 is used
   * \throws hostname_error if it's impossible to resolve host
   */
  static inline std::pair<os::hostent *, bool> resolve_host(const std::string& hostname) {
    bool ipv6 = false;
    DEBUG << "Trying to resolve " << hostname;
    auto server = os::gethostbyname2(hostname.c_str(), AF_INET);
    if (!server) {
      server = os::gethostbyname2(hostname.c_str(), AF_INET6);
      ipv6 = true;
    }
    if (!server)
      throw hostname_error("Can't resolve hostname '" + hostname + "'");
    return std::make_pair(server, ipv6);
  }

  int tcp_connect(const std::string& host) {
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
    TRACE << "Opening tcp connect to " << url;
    std::tie(server, ipv6) = resolve_host(url);
    TRACE << "Host resolved to " << server << " ipv6:" << ipv6;

    if (!ipv6) {
      struct os::sockaddr_in serv_addr;
      fd = socket(AF_INET, os::SOCK_STREAM, 0);
      if (fd < 0)
        throw network_error("Can't create socket");

      os::memset(&serv_addr, 0, sizeof(struct os::sockaddr_in));
      serv_addr.sin_family = AF_INET;
      os::memcpy((char *) &serv_addr.sin_addr.s_addr,
                 (char *) server->h_addr,
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
      os::memcpy((char *) &serv_addr.sin6_addr,
                 (char *) server->h_addr,
                 server->h_length);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using os::htons;
#endif
        serv_addr.sin6_port = htons(port);
        os::inet_pton(AF_INET6, url.c_str(), &serv_addr.sin6_addr);
      }
      if (os::connect(fd, (struct os::sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw network_error("Can't connect to server using ipv6");
    }
    return fd;
  }

#ifdef TLS_SUPPORT
  TLSConnection::TLSConnection(int tcp_fd) {
    _session.set_credentials(_credentials);
    _session.set_priority ("NORMAL", NULL);
    _session.set_transport_ptr((gnutls_transport_ptr_t) (ptrdiff_t) tcp_fd);
  }

  TLSConnection::~TLSConnection() {
    _session.bye(GNUTLS_SHUT_RDWR);
  }

  int TLSConnection::handshake() {
    return _session.handshake();
  }

  ssize_t TLSConnection::pending_bytes () {
    return _session.check_pending();
  }

  ssize_t TLSConnection::recv(void* buffer, size_t count) {
    int ret = _session.recv(buffer, count);

    if (ret == 0)
      throw networking::tls_error(ret, "Peer has closed the TLS connection");
    if (ret < 0)
      throw networking::tls_error(ret, gnutls_strerror(ret));
    return ret;
  }

  ssize_t TLSConnection::send(const void * const buffer, size_t count) {
    return _session.send(buffer, count);
  }

  std::unique_ptr<TLSConnection> tls_connect(const std::string& host) {
    TRACE << "Opening tls connect to " << host;
    int tcp_fd = tcp_connect(host);
    auto connection = std::make_unique<TLSConnection>(tcp_fd);

    // Perform the TLS handshake
    int ret = connection->handshake();

    if (ret < 0) {
      throw tls_error(ret, "Handshake failed");
    } else {
      DEBUG << "TLS handshake completed";
    }
    return connection;
  }

  std::unique_ptr<TLSConnection> tls_connect(int fd) {
    auto connection = std::make_unique<TLSConnection>(fd);

    // Perform the TLS handshake
    int ret = connection->handshake();

    if (ret < 0) {
      throw tls_error(ret, "Handshake failed");
    } else {
      DEBUG << "TLS handshake completed";
    }
    return connection;
  }
#endif
}
