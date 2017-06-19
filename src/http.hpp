#pragma once
#include <memory>
#include <map>
#include <future>
#include <functional>
#include <list>
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

  /**
   * Sypport HTTP requests enumeration
   */
  enum class HTTPRequestType {
    GET,
    HEAD
  };

  /**
   * HTTP requests generator
   *
   * use \c getHTTPLine() in order to get text to send to server
   */
  class HTTPRequest {
    std::list<std::pair<const std::string, const std::string> > _headers;
  public:
    const HTTPRequestType _type;           /**< Request type */
    const std::string _host;               /**< Host to insert into Host: header */
    const std::string _url;                /**< Endpoint on the \c _host */
    const std::string getHTTPLine() const; /**< Get resulting request */

    /**
     * Add custom HTTP header
     *
     * \param header Left part of header (before :)
     * \param val Right part of header (after :)
     */
    void addHeader(const std::string& header, const std::string& val);
    HTTPRequest(const HTTPRequestType type, const std::string& host,
                const std::string& url="/");
    ~HTTPRequest() {};
#if defined(_UNIT_TEST_BUILD)
  private:
    FRIEND_TEST(HTTPRequest, Creation);
#endif
  };

  /**
   * Class to keep the socket to server opened
   */
  struct ConnectionManager {
    /**
     * Read \count bytes from managed socket to \c buffer
     *
     * \retval number of bytes received.
     */
    virtual ssize_t recv(void* buffer, size_t count) = 0;

    /**
     * Write \c count bytes from \c buffer to managed socket
     *
     * \retval number of bytes sent.
     */
    virtual ssize_t send(const void * const buffer, size_t count) = 0;

    /**
     * Check if there is something in socket buffer
     *
     * \retval number of bytes waiting for reading.
     */
    virtual ssize_t pending() = 0;
    virtual ~ConnectionManager() {};
  };

  /**
   * Plain old HTTP connection manager
   */
  class HTTPConnectionManager: public ConnectionManager {
    const int _fd;       /**< File descriptor to use for connection */
  public:
    /**
     * Construct new ConnectionManager on top of \c fd.
     *
     * \c fd must be opened before creation.
     */
    HTTPConnectionManager(int fd) : _fd(fd) {};
    ~HTTPConnectionManager(); /**< Closes the fd */
    ssize_t recv(void* buffer, size_t count) override;
    ssize_t send(const void * const buffer, size_t count) override;
    ssize_t pending() override;
  };

#ifdef TLS_SUPPORT
  /**
   * HTTPS connection manager
   */
  class HTTPSConnectionManager: public ConnectionManager {
    /**
     * TLSConnection to send/receive data securely
     */
    const std::unique_ptr<networking::TLSConnection> _conn;
  public:
    /**
     * Construct new ConnectionManager on top of \c TLSConnection.
     *
     * \c TLSConnection must be opened before creation.
     */
    HTTPSConnectionManager(std::unique_ptr<networking::TLSConnection>&&);
    ~HTTPSConnectionManager();
    ssize_t recv(void* buffer, size_t count) override;
    ssize_t send(const void * const buffer, size_t count) override;
    ssize_t pending() override;
  };
#endif

  /**
   * HTTP response data
   */
  class HTTPResponse {
    /**
     * HTTPConnectionManager or HTTPSConnectionManager
     */
    const std::unique_ptr<ConnectionManager> _connection_manager;
    int _code;                                     /**< HTTP return code */
    std::map<std::string, std::string> _headers;   /**< HTTP headers */
    void* _buffer;                                 /**< Raw data received */
    size_t _buffer_size;                           /**< Guaranteed size of \c _buffer */
    std::unique_ptr<std::istringstream> _resp;     /**< Internal conversion object */

    void parseHttp();                              /**< Parse headers filling \c _headers */
    void recvPayload();                            /**< Download the rest of answer */
  public:
    HTTPResponse(std::unique_ptr<ConnectionManager>&&);
    ~HTTPResponse();
    /**
     * Get HTTP header
     *
     * \retval header or empty string if no such header exists.
     */
    const std::string& operator[](const std::string& header) const;
    int code() const {return _code;};

    /**
     * Get data received from sever
     *
     * \retval \c std::pair<buffer, size> with answer.
     */
    const std::pair<const void * const, size_t> data() const;
#if defined(_UNIT_TEST_BUILD)
  private:
    FRIEND_TEST(HTTPResponse, Creation);
#endif
  };

  /**
   * Run single HTTP (HTTPS) request
   *
   * \param url The full url (e.g. https://google.com:443/index.html)
   * \param type type of request.
   *
   * \retval the HTTPResponse with answer.
   */
  std::future<std::unique_ptr<HTTPResponse> >
  PerformHTTPRequest(const std::string& url,
                     HTTPRequestType	type=HTTPRequestType::GET,
                     const void	      * payload=nullptr,
                     size_t		payload_size=0);

}
