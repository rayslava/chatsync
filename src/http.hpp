#pragma once
#include <memory>
#include <map>
#include <future>
#include <functional>
#if defined(_UNIT_TEST_BUILD)
#include <gtest/gtest_prod.h>
#endif

#ifdef TLS_SUPPORT
namespace networking {
  class TLSConnection;
}
#endif

namespace http {

  /**
   * Generic HTTP error
   */
  class http_error: public std::runtime_error {
  public:
    http_error(std::string const& message) :
      std::runtime_error(message)
    {};
  };

  /**
   * Malformed URL error
   */
  class url_error: public http_error {
  public:
    url_error(std::string const& message) :
      http_error(message)
    {};
  };

  enum class HTTPRequestType {
    GET
  };

  struct ConnectionManager {
    virtual ssize_t recv(void* buffer, size_t count) = 0;
    virtual ssize_t send(const void * const buffer, size_t count) = 0;
    virtual ssize_t pending() = 0;
    virtual ~ConnectionManager() {};
  };

  class HTTPConnectionManager: public ConnectionManager {
    const int _fd;
  public:
    HTTPConnectionManager(int fd) : _fd(fd) {};
    ~HTTPConnectionManager();
    ssize_t recv(void* buffer, size_t count) override;
    ssize_t send(const void * const buffer, size_t count) override;
    ssize_t pending() override;
  };

#ifdef TLS_SUPPORT
  class HTTPSConnectionManager: public ConnectionManager {
    const std::unique_ptr<networking::TLSConnection> _conn;
  public:
    HTTPSConnectionManager(std::unique_ptr<networking::TLSConnection>&&);
    ~HTTPSConnectionManager();
    ssize_t recv(void* buffer, size_t count) override;
    ssize_t send(const void * const buffer, size_t count) override;
    ssize_t pending() override;
  };
#endif

  class HTTPResponse {
    const std::unique_ptr<ConnectionManager> _connection_manager;
    int _code;
    std::map<std::string, std::string> _headers;
    void* _buffer;
    size_t _buffer_size;
    std::unique_ptr<std::istringstream> _resp;

    void parseHttp();
    void recvPayload();
  public:
    HTTPResponse(std::unique_ptr<ConnectionManager>&&);
    ~HTTPResponse();
    /**
     * Grants access to headers
     */
    const std::string& operator[](const std::string& header) const;
    int code() const {return _code;};
    const std::pair<const void * const, size_t> data() const;
#if defined(_UNIT_TEST_BUILD)
  private:
    FRIEND_TEST(HTTPResponse, Creation);
#endif
  };

  std::future<std::unique_ptr<HTTPResponse> >
  HTTPRequest(const std::string& url,
              HTTPRequestType	 type=HTTPRequestType::GET,
              const void       * payload=nullptr,
              size_t		 payload_size=0);

}
