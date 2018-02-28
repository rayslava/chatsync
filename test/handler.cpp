#include "../src/hub.hpp"
#include "../src/channel.hpp"
#include "../src/handlers/handler.hpp"
#include "../src/handlers/handler_http_probe.hpp"
#include "../src/logging.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace pipelining {
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

  std::list<std::string> extractUrl(const std::string& message);
  using namespace messaging;
  static std::string result;
  void f(const message_ptr&& msg) {
    const auto& tm = messaging::TextMessage::fromMessage(msg);
    result = tm->data();
  }
  void g(int a) {a = 0;}

  auto&& user = std::make_shared<const User>("system");
  auto msg = std::make_shared<TextMessage>(0xFFFF, std::move(user),
                                           "http://localhost:8082/test_page.html");

  std::string extract_title(const std::string& html);
  const std::string test_html = "<html><title>The Title</title><body>bla</body></html>";
  const std::string test_html2 = "<html>\
<head>\
  <meta charset=\"UTF-8\">\
  <meta name=\"description\" content=\"The Description\">\
  <meta name=\"keywords\" content=\"HTML,CSS,XML,JavaScript\">\
  <meta name=\"author\" content=\"John Doe\">\
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
</head> \
<body>bla</body></html>";
  const std::string test_html3 = "<html>\
<head>\
  <meta charset=\"UTF-8\">\
  <meta name='description' content=\"The Description\">\
</head> \
<body>bla</body></html>";
  const std::string test_html4 = "<html>\
<head>\
  <meta charset=\"UTF-8\">\
  <meta name=\"description\" content='The Description'>\
</head> \
<body>bla</body></html>";

  TEST(HttpProbe, extract_title)
  {
    EXPECT_STREQ("The Title",	    extract_title(test_html.c_str()).c_str());
    EXPECT_STREQ("The Description", extract_title(test_html2.c_str()).c_str());
    EXPECT_STREQ("The Description", extract_title(test_html3.c_str()).c_str());
    EXPECT_STREQ("The Description", extract_title(test_html4.c_str()).c_str());
  }

  TEST(Handler, Factory)
  {
    const std::string name = "http_probe";
    EXPECT_NO_THROW({
      auto handler = CreateHandler(name, std::function<void(const message_ptr&&)>(f));
    });
    EXPECT_THROW({
      auto handler = CreateHandler(name, std::function<void(int)>(g));
    }, handler_error::wrong_initializer);
    EXPECT_THROW({
      auto handler = CreateHandler("no_such_handler", nullptr);
    }, handler_error::not_registered);
  }

  TEST(HttpProbe, extractUrl)
  {
    const std::string& testline = "https://localhost.localdomain:9000/index.html,https://ya.ru\nhttp://testdomain.com/asdf http://test.site/test_page.html http://localhost:8082/test_page.html";
    const std::list<std::string> original {
      "https://localhost.localdomain:9000/index.html",
      "https://ya.ru",
      "http://testdomain.com/asdf",
      "http://test.site/test_page.html",
      "http://localhost:8082/test_page.html",
    };
    auto extracted = extractUrl(testline);
    EXPECT_THAT(extracted, ::testing::ContainerEq(original));
  }

  TEST(Handler, HttpProbe)
  {
    int sfd, nfd;
    int port = 8082;
    const std::string len = std::to_string(test_html2.length());
    const std::string& answer = "HTTP/1.1 200 OK\r\nContent-Length: " + len + "\r\nContent-Type: text/html\r\n\r\n" + test_html2;
    const std::string right_msg = "URL: [text/html, " + len + " b]: The Description";
    const auto server = std::make_unique<std::thread>(std::thread(&sockListen, std::ref(answer), std::ref(port), std::ref(sfd), std::ref(nfd)));

    const std::string name = "http_probe";
    auto probe = HttpProbeHandler(std::function<void(const message_ptr&&)>(f));
    auto result_future = probe.processMessage(msg);
    result_future.wait();
    result_future.get();
    ASSERT_STREQ(result.c_str(), right_msg.c_str());
    server->join();
  }

  TEST(Handler, Hub_addHandler)
  {
    DEFAULT_LOGGING
    Hub::handlerPtr probe = CreateHandler("http_probe", std::function<void(const message_ptr&&)>(f));
    auto hub = new Hub::Hub("testhub");
    hub->addHandler(std::move(probe));
    delete hub;
  }
}
