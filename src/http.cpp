#include <stdexcept>
#include <algorithm>
#include "net.hpp"
#include "http.hpp"
#include "logging.hpp"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace http {

  static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
  }

  static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
            s.end());
  }

  static inline void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
  }

  static inline std::string trimmed(std::string s) {
    trim(s);
    return s;
  }

  const std::string& HTTPResponse::operator[](const std::string& header) const {
    std::string upcase_header;
    std::transform(header.begin(), header.end(),
                   std::back_inserter(upcase_header), ::toupper);
    return _headers.at(upcase_header);
  }

  const std::pair<const void * const, size_t> HTTPResponse::data() const {
    const char* _body = static_cast<const char *>(_buffer) + _header_size;
    return std::make_pair(static_cast<const void *>(_body), _buffer_size - _header_size);
  }

  std::future<std::unique_ptr<HTTPResponse> >
  PerformHTTPRequest(const std::string& url, const HTTPRequest& req) {
    bool https = false;
    const std::string::size_type proto_end = url.find("://");
    if (proto_end == std::string::npos)
      throw url_error("Can't detect protocol");
    const auto& proto = url.substr(0, proto_end);
    if (proto == "http") {
      WARNING << "Raw HTTP connection is insecure";
      https = false;
    } else if (proto == "https") {
      DEBUG << "HTTPS connection is secure";
      https = true;
    } else {
      throw http_error("Unsupported protocol provided");
    }

    /* Now trying to detect if there is a port */
    const std::vector<char> c {':', '/'};
    const auto server_end_c = std::find_first_of(std::next(url.cbegin(), proto_end + 3),
                                                 url.cend(),
                                                 c.begin(), c.end());
    const auto server_end = std::distance(url.cbegin(), server_end_c);

    std::string port;
    if (*server_end_c == ':') {
      /* There is port */
      std::string::size_type port_end = url.find("/", server_end);
      port = url.substr(server_end + 1, server_end - port_end);
    } else {
      port = https ? "443" : "80";
    }

    const auto& server = url.substr(proto_end + 3, server_end - proto_end - 3);

    DEBUG << "Opening " << std::string(https ? "secure" : "insecure")
          << " connection to " << server << " on port " << port;

    std::string request_line = req.getHTTPLine();
    const auto request_body = req.body();
    TRACE << "Requesting: " << request_line;

    if (!https) {
      return std::async(std::launch::async, [server, port, request_line, request_body]() {
        const int fd = networking::tcp_connect(server + ":" + port);
        auto&& conn_mgr = std::make_unique<HTTPConnectionManager>(fd);
        {
          auto buf = new char[request_body.second + 1];
          int res = snprintf(buf, request_body.second + 1, "%.*s",
                             static_cast<int>(request_body.second),
                             static_cast<const char *>(request_body.first));
          TRACE << "Sending " << request_line << buf;
          delete[] buf;
        }
        conn_mgr->send(request_line.c_str(), request_line.length());
        if (request_body.second > 0)
          conn_mgr->send(request_body.first, request_body.second);
        auto&& response = std::make_unique<HTTPResponse>(std::move(conn_mgr));
        return std::move(response);
      });
    }
#ifdef TLS_SUPPORT
    else {
      return std::async(std::launch::async, [server, port, request_line, request_body]() {
        auto&& conn = networking::tls_connect(server + ":" + port);
        auto&& conn_mgr = std::make_unique<HTTPSConnectionManager>(std::move(conn));
        {
          auto buf = new char[request_body.second + 1];
          int res = snprintf(buf, request_body.second + 1, "%.*s",
                             static_cast<int>(request_body.second),
                             static_cast<const char *>(request_body.first));
          TRACE << "Sending " << request_line << buf;
          delete[] buf;
        }
        conn_mgr->send(request_line.c_str(), request_line.length());
        if (request_body.second > 0)
          conn_mgr->send(request_body.first, request_body.second);
        auto&& response = std::make_unique<HTTPResponse>(std::move(conn_mgr));
        return std::move(response);
      });
    }
#else
    throw http_error("Unsupported protocol provided");
#endif
  }

  constexpr auto MAX_BUF = 4096;
  constexpr auto receive_timeout = std::chrono::milliseconds(5);

  HTTPResponse::HTTPResponse(std::unique_ptr<ConnectionManager>&& mgr) :
    _connection_manager(std::move(mgr)),
    _code(0),
    _buffer(malloc(MAX_BUF)),
    _buffer_size(0),
    _header_size(0)
  {
    memset(_buffer, 0, MAX_BUF);
    _buffer_size = _connection_manager->recv(_buffer, MAX_BUF - 1);
    /** TODO : check res < 0 */
    parseHttp();
    recvBody();
  }

  HTTPResponse::~HTTPResponse() {
    free(_buffer);
  }

  void HTTPResponse::parseHttp() {
    _resp = std::make_unique<std::istringstream>(static_cast<char *>(_buffer));
    std::string header;
    std::string::size_type index;

    // Parse HTTP response code
    std::getline(*_resp, header);
    index = header.find(' ', 0);
    const std::string http_proto = header.substr(0, index);
    /* Checking HTTP version */
    {
      const auto& http_keyword = http_proto.substr(0, http_proto.find("/"));
      if (http_keyword != "HTTP")
        throw http_error("Response is not HTTP");
      const auto& http_ver =
        http_proto.substr(http_keyword.length() + 1,
                          http_proto.length() - http_keyword.length());
      TRACE << "HTTP: " << http_keyword << " " << http_ver;
      if (std::stof(http_ver) > 1.1f)
        throw http_error("Unsupported version of HTTP");
    }
    index = header.find(' ', index + 1);
    const uint32_t http_code = std::stoi(header.substr(http_proto.length() + 1, index));
    const std::string http_text = header.substr(index + 1, header.length());
    TRACE << "The response of proto " << http_proto << " is " << http_code << " with text " << http_text;
    _code = http_code;
    if (http_code >= 400) {
      throw http_error("Server return HTTP error " + std::to_string(http_code), http_code);
    }
    // Parse HTTP Headers
    while (std::getline(*_resp, header) && header != "\r") {
      index = header.find(':', 0);
      if(index != std::string::npos) {
        std::string key = header.substr(0, index);
        TRACE << "Found key " << key;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        _headers.insert(std::make_pair(trimmed(key), trimmed(header.substr(index + 1))));
      }
    }
  }

  void HTTPResponse::recvBody() {
    _header_size = _resp->tellg();
    if (_headers.find("CONTENT-LENGTH") != _headers.end()) {
      const size_t response_size = std::stoi(_headers["CONTENT-LENGTH"]);
      DEBUG << "Content-Length provided with " << response_size << " bytes.";
      if (MAX_BUF - _header_size < response_size) {
        _buffer = realloc(_buffer, MAX_BUF + response_size + 1);
        memset(static_cast<char *>(_buffer) + _buffer_size, 0, response_size + 1);
      }
      char* buf = static_cast<char *>(_buffer);
      while (_buffer_size < _header_size + response_size) {
        TRACE << "[" << _buffer_size << "/" << _header_size + response_size << "] downloaded";
        try {
          auto res = _connection_manager->recv(buf + _buffer_size, response_size);
          _buffer_size += res;
        }
        catch (networking::tls_error e) {
          if (e.code == 0) break; // Just end of data in TLS connection
          else throw networking::tls_error(e);
        }
      }
    } else {
      DEBUG << "Content-Length not provided, reading the socket until timeout";
      size_t current_size = MAX_BUF;
      size_t res = 0;
      size_t pending;
      auto last_read = std::chrono::high_resolution_clock::now();
      auto timeout =
        [](auto start) {
          const auto now = std::chrono::high_resolution_clock::now();
          const std::chrono::duration<double> diff = now - start;
          return diff > receive_timeout;
        };
      while ((pending = _connection_manager->pending()) || !timeout(last_read)) {
        if (current_size < _buffer_size + pending + 1) {
          current_size += MAX_BUF;
          _buffer = realloc(_buffer, current_size);
          memset(static_cast<char *>(_buffer) + _buffer_size, 0, MAX_BUF);
          pending = MAX_BUF;
        }
        char* buf = static_cast<char *>(_buffer);
        res = _connection_manager->recv(buf + _buffer_size, pending);
        _buffer_size += res;
        if (res) {
          TRACE << "[" << current_size << "/" << _buffer_size + pending << "] downloaded";
          last_read = std::chrono::high_resolution_clock::now();
        }
      }
    }
  }

  HTTPConnectionManager::~HTTPConnectionManager() {
    close(_fd);
  }

  ssize_t HTTPConnectionManager::recv(void* buffer, size_t count) {
    return read(_fd, buffer, count);
  }

  ssize_t HTTPConnectionManager::send(const void * const buffer, size_t count) {
    return write(_fd, buffer, count);
  }

  ssize_t HTTPConnectionManager::pending() {
    int bytes = 0;
    ioctl(_fd, FIONREAD, &bytes);
    return bytes;
  }

#ifdef TLS_SUPPORT
  HTTPSConnectionManager::HTTPSConnectionManager(std::unique_ptr<networking::TLSConnection>&& connection) :
    _conn(std::move(connection)) {}

  HTTPSConnectionManager::~HTTPSConnectionManager() {}

  ssize_t HTTPSConnectionManager::recv(void* buffer, size_t count) {
    return _conn->recv(buffer, count);
  }

  ssize_t HTTPSConnectionManager::send(const void * const buffer, size_t count) {
    return _conn->send(buffer, count);
  }

  ssize_t HTTPSConnectionManager::pending() {
    return _conn->pending_bytes();
  }
#endif

  HTTPRequest::HTTPRequest(const HTTPRequestType type,
                           const std::string   & host,
                           const std::string   & url) :
    _body_size(0),
    _type(type),
    _host(host),
    _uri(url)
  {
    addHeader("host",	host);
    addHeader("accept", "*/*");
  }

  void HTTPRequest::addHeader(const std::string& header,
                              const std::string& val) {
    std::string head;
    std::transform(header.begin(), header.end(),
                   std::back_inserter(head), ::tolower);
    bool nextup = true;
    for (auto i = head.begin(); i != head.end(); ++i) {
      if (nextup) {
        *i = ::toupper(*i);
        nextup = false;
      }
      if (*i == '-') nextup = true;
    }
    _headers.push_back(std::make_pair(head, val));
  }

  const std::string HTTPRequest::getHTTPLine() const {
    const static std::string http_suffix {"HTTP/1.1"};
    const static std::string newline {"\r\n"};
    const std::string req =
      [this](){
        switch (_type) {
        case HTTPRequestType::GET:  return "GET";
        case HTTPRequestType::HEAD: return "HEAD";
        case HTTPRequestType::POST: return "POST";
        default: throw http_error("Can't construct request");
        }
      } ();
    std::string request_line = req + " " + _uri + " " + http_suffix + newline;
    for (auto h : _headers)
      request_line.append(h.first + ": " + h.second + newline);
    request_line.append(newline);
    return request_line;
  }

  void HTTPRequest::setBody(std::unique_ptr<char[]>&& body, size_t body_size) {
    _body = std::move(body);
    _body_size = body_size;
  }

  std::pair<const void * const, size_t> HTTPRequest::body() const {
    const void * const ptr = static_cast<void *>(_body.get());
    return std::make_pair(ptr, _body_size);
  }
}
