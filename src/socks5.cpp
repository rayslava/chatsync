#include "net.hpp"

#include <cstdint>

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

#define COPY_MEMBER(p, m) {_memcpy(p, &m, sizeof(m)); p += sizeof(m);}


  class SocksRequest {
    constexpr static std::uint8_t _ver = 0x05;
    constexpr static std::uint8_t _rsv = 0x00;

    const std::uint8_t _cmd;

    const std::uint8_t _atyp;
    const int _addr_len;
    const std::unique_ptr<char[]> _dst_addr;

    const std::uint16_t _dst_port;

    static std::uint8_t strToIPOctet(const std::string& str) {
      int number = std::stoi(str);
      if (number >= 0 && number < 256)
        return number;
      throw std::invalid_argument("Wrong IP address provided");
    }

    void packIpv4Addr(const std::string& addr) {
      std::vector<std::string> lines;
      std::string::size_type pos = 0, lastpos = 0;
      while ((pos = addr.find(".", lastpos)) != std::string::npos) {
        lines.push_back(addr.substr(lastpos, pos - lastpos));
        lastpos = pos + 1;
      }
      lines.push_back(addr.substr(lastpos, addr.length() - lastpos));
      int n = 0;
      for (const auto& l : lines)
        _dst_addr.get()[n++] = strToIPOctet(l);
    }

    void packIpv6Addr(const std::string& addr) {
      throw proxy_error("ipv6 addresses are not supported yet");
    }

    void packDomain(const std::string& addr) {
      *_dst_addr.get() = _addr_len;
      _memcpy(_dst_addr.get() + 1, addr.c_str(), addr.length());
    }

  public:
    enum Command {
      Connect = 0x01,
      Bind = 0x02,
      UDPAssoc = 0x03
    };

    enum AddrType {
      ipv4 = 0x01,
      ipv6 = 0x04,
      domain = 0x03
    };

    SocksRequest(Command	    cmd,
                 AddrType	    atyp,
                 const std::string& addr,
                 std::uint16_t	    port) :
      _cmd(cmd),
      _atyp(atyp),
      _addr_len(atyp == ipv4 ? 4 : atyp == ipv6 ? 16 : addr.length() + 1),
      _dst_addr(std::make_unique<char[]>(_addr_len)),
      _dst_port(port)
    {
      switch (_atyp) {
      case ipv4: packIpv4Addr(addr);
        break;
      case ipv6: packIpv6Addr(addr);
        break;
      case domain: packDomain(addr);
        break;
      default:
        throw proxy_error("Wrong socks5 proxy address type");
      }
    }

    auto buffer() const {
      int _buf_len = sizeof(_ver) + sizeof(_cmd) + sizeof(_rsv) + sizeof(_atyp) +
                     sizeof(_dst_port) + _addr_len + 1;
      std::unique_ptr<char[]> result = std::make_unique<char[]>(_buf_len);
      char* ptr = result.get();

      COPY_MEMBER(ptr, _ver);
      COPY_MEMBER(ptr, _cmd);
      COPY_MEMBER(ptr, _rsv);
      COPY_MEMBER(ptr, _atyp);

      /* Copy target address */
      _memcpy(ptr, _dst_addr.get(), _addr_len);
      ptr += _addr_len + 1;

      /* Set network byte format for port */
      *ptr++ = static_cast<std::uint8_t>(_dst_port & 0xFF00) >> 8;
      *ptr++ = static_cast<std::uint8_t>(_dst_port & 0xFF);

      return std::make_pair(std::move(result), _buf_len);
    }
  };

  enum SocksMethods : std::uint8_t {
    NoAuth = 0x00,
    GSSAPI = 0x01,
    UserPass = 0x02,
    NoAcceptable = 0xFF
  };

  /**
   * Server negotiation information
   *
   * Since it depends only on current implementation it's as static as possible.
   */
  struct SocksHello {
    constexpr static std::uint8_t _ver = 0x05;
    constexpr static std::uint8_t _nmethods = 0x01;
    constexpr static std::uint8_t _methods[1] = {SocksMethods::NoAuth};

    static auto buffer() {
      constexpr int _buf_len = sizeof(_ver) + sizeof(_nmethods) + sizeof(_methods);
      std::unique_ptr<char[]> result = std::make_unique<char[]>(_buf_len);
      char* ptr = result.get();
      COPY_MEMBER(ptr, _ver);
      COPY_MEMBER(ptr, _nmethods);
      COPY_MEMBER(ptr, _methods);
      return std::make_pair(std::move(result), _buf_len);
    }
  };

  struct SocksAck {
    std::uint8_t ver;
    SocksMethods method;
  };

  void authenticate (SocksMethods method) {
    switch (method) {
    case SocksMethods::NoAuth:
      /* No auth reqiured */
      break;
    case SocksMethods::GSSAPI:
      /* GSSAPI */
      throw proxy_error("GSSAPI is not supported yet");
      break;
    case SocksMethods::UserPass:
      /* Username/Password */
      throw proxy_error("user/pass auth is not supported yet");
      break;
    case SocksMethods::NoAcceptable:
    default:
      throw proxy_error("Server does not support any acceptable methods");
      break;
    }
  }

  struct SocksReply {
    std::uint8_t ver;
    std::uint8_t cmd;
    std::uint8_t rsv;
    std::uint8_t atyp;
  };

  int _do_socks_proxy_connect(const std::string& host,
                              const std::string& proxy)
  {
    DEBUG << "Connecting socks5 proxy " << proxy;
    int fd = tcp_connect(proxy);

    TRACE << "Requesting methods";
    const auto [hello, hello_len] = SocksHello::buffer();
    int res = os::write(fd, hello.get(), hello_len);

    SocksAck socks_ack;
    res = os::read(fd, &socks_ack, sizeof(socks_ack));
    if (res != sizeof(socks_ack))
      throw proxy_error("Wrong socks5 proxy response");

    if (socks_ack.ver != 5)
      throw proxy_error("Provided server is not socks5 proxy");
    TRACE << "Server chose method: " << int(socks_ack.method);

    authenticate(socks_ack.method);

    /* Set up appropriate authentication method */

    SocksRequest req(SocksRequest::Command::Connect,
                     SocksRequest::AddrType::domain,
                     host, 80);

    const auto [buf, len] = req.buffer();
    res = os::write(fd, buf.get(), len);

    char header[4];
    res = os::read(fd, header, 4);
    char body[6];
    res = os::read(fd, body, 6);
    std::uint16_t port = 0;
    port |= body[5];
    port |= body[4] << 8;
    if (header[1] == 0) {
      DEBUG << "Succeeded!";
      if (header[3] == 1) {
        std::string connection_line = std::string("") + std::to_string(int(std::uint8_t(body[0]))) +
                                      "." + std::to_string(int(std::uint8_t(body[1]))) + "." + std::to_string(int(std::uint8_t(body[2]))) + "."
                                      + std::to_string(int(std::uint8_t(body[3]))) + ":" + std::to_string(port);

        DEBUG << "Connect line " << connection_line;
        return fd;
        //	int newfd = tcp_connect(connection_line);
        os::write(fd, "GET / HTTP/1.1\r\n\r\n", sizeof("GET / HTTP/1.1\r\n\r\n"));
        char buf[256];
        res = os::read(fd, buf, 256);
        DEBUG << std::string(buf);
      }
    }
    return fd;
  }
}
