#include "net.hpp"
#include <memory>
#include <mutex>
#include <future>
#include <algorithm>

#ifdef PROXY_SUPPORT
#include "http.hpp"
#endif

/** This namespace contains network-related inline functions intended to embed
    into clients */
namespace networking {
  namespace os {
    extern "C" {
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#ifndef __clang__
#include <string.h>
    }
#else
    }
#include <cstring>
#endif
  }

  static inline void * _memset(void* s, int c, size_t n) {
#ifndef __clang__
    return os::memset(s, c, n);
#else
    return memset(s, c, n);
#endif
  }

  static inline void * _memcpy(void* dest, const void* src, size_t n) {
#ifndef __clang__
    return os::memcpy(dest, src, n);
#else
    return memcpy(dest, src, n);
#endif
  }

#ifndef STATIC
  /**
   * Perform the hostname resolution asyncronously
   *
   * \param hostname Name of host to resolve
   * \returns std::tuple with h_addr, its size and bool for ipv6
   * bool is \c true if ipv6 is used
   * \throws hostname_error if it's impossible to resolve host
   */
  static inline std::tuple<std::unique_ptr<char[]>, int, bool>
  resolve_host(const std::string& hostname) {
    struct os::hostent he;
    struct os::hostent* hp = &he;

    const std::size_t buflen = 1024;
    auto buf = std::make_unique<char[]>(buflen);
    int herr, hres;

    bool ipv6 = false;
    DEBUG << "Trying to resolve " << hostname;
    hres = os::gethostbyname2_r(hostname.c_str(), AF_INET,
                                &he, buf.get(), buflen, &hp, &herr);
    if (hres) {
      hres = os::gethostbyname2_r(hostname.c_str(), AF_INET6,
                                  &he, buf.get(), buflen, &hp, &herr);
      ipv6 = true;
    }
    if (hres)
      throw hostname_error("Can't resolve hostname '" + hostname + "'");
    auto result = std::make_unique<char[]>(he.h_length);
    _memcpy(result.get(), he.h_addr, he.h_length);
    return std::make_tuple(std::move(result), he.h_length, ipv6);
  }
#else
  /**
   * Perform the hostname resolution syncronously with mutex
   *
   * Only available during dynamic linkage due to glibc bug 10652
   * https://sourceware.org/bugzilla/show_bug.cgi?id=10652
   *
   * \param hostname Name of host to resolve
   * \returns std::tuple with h_addr, its size and bool for ipv6
   * bool is \c true if ipv6 is used
   * \throws hostname_error if it's impossible to resolve host
   */
  static inline std::tuple<std::unique_ptr<char[]>, int, bool>
  resolve_host(const std::string& hostname) {
    static std::mutex resolve_mutex;
    os::hostent* hp;
    const std::size_t buflen = 0;
    auto buf = std::make_unique<char[]>(buflen);

    bool ipv6 = false;
    {
      std::unique_lock<std::mutex> resolve_lock(resolve_mutex);
      DEBUG << "Trying to resolve " << hostname;
      hp = os::gethostbyname2(hostname.c_str(), AF_INET);
      if (!hp) {
        hp = os::gethostbyname2(hostname.c_str(), AF_INET6);
        ipv6 = true;
      }
      if (!hp)
        throw hostname_error("Can't resolve hostname '" + hostname + "'");
      auto result = std::make_unique<char[]>(hp->h_length);
      _memcpy(result.get(), hp->h_addr, hp->h_length);
      return std::make_tuple(std::move(result), hp->h_length, ipv6);
    }
  }
#endif

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

    const auto& [resolved_host, resolved_host_len, ipv6] = resolve_host(url);
    TRACE << "Opening tcp connect to " << url;
    if (!ipv6) {
      struct os::sockaddr_in serv_addr;
      fd = socket(AF_INET, os::SOCK_STREAM, 0);
      if (fd < 0)
        throw network_error("Can't create socket");

      _memset(&serv_addr, 0, sizeof(struct os::sockaddr_in));
      serv_addr.sin_family = AF_INET;
      _memcpy((char *) &serv_addr.sin_addr.s_addr,
              (char *) resolved_host.get(),
              resolved_host_len);
      {
        /* Ugly workaround to use different optimization levels for compiler */
#ifndef htons
        using os::htons;
#endif
        serv_addr.sin_port = htons(port);
      }
      if (connect(fd, (struct os::sockaddr *) &serv_addr, sizeof(struct os::sockaddr_in)) < 0)
        throw network_error("Can't connect to server");
    } else {
      struct os::sockaddr_in6 serv_addr;

      fd = socket(AF_INET6, os::SOCK_STREAM, 0);
      if (fd < 0)
        throw network_error("Can't create ipv6 socket");
      _memset(&serv_addr, 0, sizeof(struct os::sockaddr_in6));
      serv_addr.sin6_family = AF_INET6;
      _memcpy((char *) &serv_addr.sin6_addr,
              (char *) resolved_host.get(),
              resolved_host_len);
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

#ifdef PROXY_SUPPORT

  class HTTPProxyConnectionManager: public http::ConnectionManager {
    const char* _buf;  /**< Pointer to buffer with response */
    int _left;         /**< Bytes left to read */
  public:
    HTTPProxyConnectionManager(int sz, const char* buf) : _buf(buf), _left(sz) {};
    ~HTTPProxyConnectionManager() {};
    ssize_t recv(void* buffer, size_t count) override {
      const auto copied = std::min<size_t>(count, _left);
      _memcpy(buffer, _buf, copied);
      _left -= copied;
      return copied;
    };
    ssize_t send(const void * const buffer [[maybe_unused]], size_t count [[maybe_unused]]) override {
      return 0;
    };
    ssize_t pending() override { return _left; };
    bool valid() override { return _left > 0; };
  };

#ifndef _UNIT_TEST_BUILD
  static
#endif
  int http_proxy_connect(const std::string& host, const std::string& proxy)
  {
    int fd = tcp_connect(proxy);
    static const std::string newline = "\r\n\r\n";
    static const std::string http = " HTTP/1.1";
    static const std::string connect_cmd = "CONNECT ";
    const ssize_t line_len = connect_cmd.length() + host.length() +
                             http.length() + newline.length();
    const auto line_buf = std::make_unique<char[]>(line_len + 1);
    int res = snprintf(line_buf.get(), line_len + 1, "%s%s%s%s",
                       connect_cmd.c_str(),
                       host.c_str(),
                       http.c_str(),
                       newline.c_str());
    line_buf[line_len] = '\0';
    if (res != line_len)
      throw std::runtime_error("Really awful snprintf() overflow happened");
    res = os::write(fd, line_buf.get(), line_len);
    if (res != line_len)
      throw proxy_error("Couldn't send proxy request");
    int bytes = 0;
    std::chrono::milliseconds timeout = proxy_timeout;
    /* Try FIONREAD until we get something or ioctl fails. */
    while (!bytes && os::ioctl (fd, FIONREAD, &bytes) >= 0 && timeout.count() > 0) {
      std::this_thread::sleep_for(proxy_loop_delay);
      timeout -= proxy_loop_delay;
    }
    if (res < 0 || !bytes)
      throw proxy_error("Proxy didn't respond");
    const auto response_buf = std::make_unique<char[]>(bytes + 1);
    res = os::read(fd, response_buf.get(), bytes);
    response_buf[bytes] = 0;
    auto manager = std::make_unique<HTTPProxyConnectionManager>(bytes, response_buf.get());
    auto response = std::make_unique<http::HTTPResponse>(std::move(manager), false);
    if (response->code() != 200) {
      ERROR << "Proxy returned " << std::to_string(response->code());
      throw proxy_error("Proxy connection error");
    }
    DEBUG << "Proxy connection established with " << proxy;
    return fd;
  }

  static int socks_proxy_connect(const std::string& host,
                                 const std::string& proxy)
  {
    DEBUG << "Connecting socks5 proxy " << proxy;
    return tcp_connect(host);
  }

  int proxy_tcp_connect(const std::string& host, const std::string& proxy, ProxyType type) {
    TRACE << "Connecting to proxy " << proxy;
    switch (type) {
    case ProxyType::HTTP:
      return http_proxy_connect(host, proxy);
      break;
    case ProxyType::SOCKS5:
      return socks_proxy_connect(host, proxy);
      break;
    default:
      throw proxy_error("Unsupported proxy protocol");
    };
  }
#endif

#ifdef TLS_SUPPORT
  TLSConnection::TLSConnection(int tcp_fd) :
    _fd(tcp_fd)
  {
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

  bool TLSConnection::valid() {
    errno = 0;
    return (os::fcntl(_fd, F_GETFD) != -1) && (errno != EBADF);
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
