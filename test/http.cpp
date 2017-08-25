#include "../src/http.hpp"
#include "../src/logging.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace http {
  static void sockListen(const std::string& answer, int port, int& sockfd, int& newsockfd) {
    int portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = port;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
      perror("ERROR on accept");
      exit(1);
    }

    bzero(buffer, 256);

    n = write(newsockfd, answer.c_str(), answer.length());
    if (n < 0) {
      perror("ERROR writing to socket");
      exit(1);
    }
  }

  struct EmptyConnection: public ConnectionManager {
    EmptyConnection() {};
    ~EmptyConnection() {};
    ssize_t recv(void * buffer __attribute__((unused)),
                 size_t count __attribute__((unused))) override { return 0; }
    ssize_t send(const void * const buffer __attribute__((unused)),
                 size_t		    count __attribute__((unused))) override { return 0; }
    ssize_t pending() override {return 0;};
  };

  TEST(HTTPRequest, Creation)
  {
    HTTPRequest r(HTTPRequestType::GET, "test");
    r.addHeader("tEsT-hEaDeR", "Test value");
    ASSERT_STREQ(r.getHTTPLine().c_str(),
                 "GET / HTTP/1.1\r\nHost: test\r\n\
Accept: */*\r\nTest-Header: Test value\r\n\r\n");
  }

  TEST(HTTPRequest, Body)
  {
    HTTPRequest r(HTTPRequestType::GET, "test");
    auto body = std::unique_ptr<char[]>(new char[1024]);
    memset(body.get(), 0, 1024);
    snprintf(body.get(), 10, "%s", "test line");
    r.setBody(std::move(body), 1024);
    const void* ptr;
    size_t len;
    std::tie(ptr, len) = r.body();
    ASSERT_STREQ(static_cast<const char *>(ptr), "test line");
  }

  TEST(HTTPRequest, ProtoDetection)
  {
    DEFAULT_LOGGING;
    HTTPRequest req(HTTPRequestType::GET, "test.site", "/");
    EXPECT_THROW({ PerformHTTPRequest("test.site", req);
                 }, url_error);
    EXPECT_THROW({ PerformHTTPRequest("unsupp://test.site", req);
                 }, http_error);
  }

  TEST(PerformHTTPRequest, local)
  {
    int sfd, nfd;
    int port = 8080;
    const std::string& answer = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    const auto server = std::make_unique<std::thread>(std::thread(&sockListen, std::ref(answer), std::ref(port), std::ref(sfd), std::ref(nfd)));
    HTTPRequest req(HTTPRequestType::HEAD, "localhost", "/");
    auto hr = PerformHTTPRequest("http://localhost:8080/", req);
    hr.wait();
    auto result = hr.get();
    ASSERT_EQ(result->code(), 200);
    const auto data = result->data();
    const char* line = static_cast<const char *>(data.first);
    ASSERT_STREQ(line, "ok");
    ASSERT_EQ(data.second, 2);
    server->join();
  }

  TEST(PerformHTTPRequest, localNoBody)
  {
    int sfd, nfd;
    int port = 8081;
    const std::string& answer = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    const auto server = std::make_unique<std::thread>(std::thread(&sockListen, std::ref(answer), std::ref(port), std::ref(sfd), std::ref(nfd)));
    HTTPRequest req(HTTPRequestType::HEAD, "localhost", "/");
    auto hr = PerformHTTPRequest("http://localhost:8081/", req, false);
    hr.wait();
    auto result = hr.get();
    ASSERT_EQ(result->code(),	      200);
    ASSERT_EQ(result->_body_received, false);
    const auto data = result->data();
    ASSERT_EQ(result->_body_received, true);
    const char* line = static_cast<const char *>(data.first);
    ASSERT_STREQ(line, "ok");
    ASSERT_EQ(data.second, 2);
    server->join();
  }

  TEST(PerformHTTPRequest, Yandex)
  {
    HTTPRequest req(HTTPRequestType::HEAD, "ya.ru", "/");
    auto hr = PerformHTTPRequest("http://ya.ru/", req);
    hr.wait();
    auto result = hr.get();
    ASSERT_EQ(result->code(), 302);
    ASSERT_STREQ((*result)["location"].c_str(), "https://ya.ru/");
  }

  TEST(PerformHTTPRequest, NoBody)
  {
    HTTPRequest req(HTTPRequestType::HEAD, "ya.ru", "/");
    auto hr = PerformHTTPRequest("http://ya.ru/", req, false);
    hr.wait();
    auto result = hr.get();
    ASSERT_EQ(result->code(), 302);
    ASSERT_STREQ((*result)["location"].c_str(), "https://ya.ru/");
  }
}
