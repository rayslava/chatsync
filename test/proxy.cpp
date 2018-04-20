#include "../src/net.hpp"
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

namespace networking {
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
  int http_proxy_connect(const std::string& host, const std::string& proxy);

  TEST(http_proxy, connect)
  {
    int fd = http_proxy_connect("google.com:80", "localhost:1080");
  }

  TEST(http_proxy, not_connect)
  {
    EXPECT_THROW({ http_proxy_connect("google.com:80", "localhost:12345");
                 }, proxy_error);
  }

}
