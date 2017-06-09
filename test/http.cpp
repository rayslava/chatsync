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

  TEST(HTTPResponse, Creation)
  {
    std::unique_ptr<ConnectionManager> ec(new EmptyConnection());
    HTTPResponse resp(std::move(ec));
    resp._code = 200;
    resp._headers["test"] = "testline1";
    resp._headers["TEST"] = "testline2";
    ASSERT_STREQ("testline2", resp["test"].c_str());
    ASSERT_EQ(200, resp.code());
  }

  TEST(HTTPRequest, ProtoDetection)
  {
    DEFAULT_LOGGING

      EXPECT_THROW({ HTTPRequest("test.site");
                   }, url_error);
    EXPECT_THROW({ HTTPRequest("unsupp://test.site");
                 }, http_error);
    EXPECT_NO_THROW({ HTTPRequest ("http://test.site");
                    });
    EXPECT_NO_THROW({ HTTPRequest ("http://test.site:443");
                    });
    EXPECT_NO_THROW({ HTTPRequest ("https://test.site:8080");
                    });
    EXPECT_NO_THROW({ HTTPRequest ("http://test.site/");
                    });
    EXPECT_NO_THROW({ HTTPRequest ("https://test.site/");
                    });
  }

  TEST(HTTPRequest, Yandex)
  {
    DEFAULT_LOGGING

    auto hr = HTTPRequest("https://yandex.ru");
    hr.wait();
    std::cerr << "HR is waited";
    auto result = hr.get();
    std::cerr << (*result)["content-size"] << std::endl;
  }
}
