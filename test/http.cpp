#define _UNIT_TEST_BUILD
#include "../src/http.hpp"
#undef _UNIT_TEST_BUILD
#include "../src/logging.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <cstdio>

namespace http {
  struct EmptyConnection: public ConnectionManager {
    EmptyConnection() {};
    ~EmptyConnection() {};
    ssize_t recv(void* buffer, size_t count) override { return 0; }
    ssize_t send(const void * const buffer, size_t count) override { return 0; }
    ssize_t pending() {return 0;};
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
    DEFAULT_LOGGING
      EXPECT_THROW({ PerformHTTPRequest("test.site");
                   }, url_error);
    EXPECT_THROW({ PerformHTTPRequest("unsupp://test.site");
                 }, http_error);
  }

  TEST(PerformHTTPRequest, Yandex)
  {
    DEFAULT_LOGGING

    auto hr = PerformHTTPRequest("http://ya.ru/", HTTPRequestType::HEAD);
    hr.wait();
    auto result = hr.get();
    ASSERT_EQ(result->code(), 302);
    ASSERT_STREQ((*result)["location"].c_str(), "https://ya.ru/");
  }
}