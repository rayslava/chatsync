#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

constexpr auto port = 33445;
const char testLine[] = "Testing file writing\r\n";
const char lorem_ipsum[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
const char valid_lorem_ipsum[] = "PRIVMSG #test :[test]: Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo \r\nPRIVMSG #test :[test]: consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est \r\nPRIVMSG #test :[test]: laborum.\r\n";

void sockListen(const std::string& ircTestLine, int& sockfd, int& newsockfd) {
  int portno;
  socklen_t clilen;
  constexpr int buffer_size = 4096;
  char buffer[buffer_size];
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

  n = write(newsockfd, ircTestLine.c_str(), ircTestLine.length());
  if (n < 0) {
    perror("ERROR writing to socket");
    exit(1);
  }
}

void ircServer(char* buffer, int buffer_size, int& sockfd, int& newsockfd) {
  int portno;
  socklen_t clilen;
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

  bzero(buffer, buffer_size);
  while(strncmp("PRIVMSG", buffer, 7)) {
    bzero(buffer, buffer_size);
    n = read(newsockfd, buffer, buffer_size);
    if (n < 0) {
      perror("ERROR reading from socket");
      exit(1);
    }
  };

  while (strcmp("laborum.\r\n", buffer + strlen(buffer) - strlen("laborum.\r\n"))) {
    n = read(newsockfd, buffer + strlen(buffer), buffer_size - strlen(buffer));
    if (n < 0) {
      perror("ERROR reading from socket");
      exit(1);
    }
  }
}

TEST(IrcChannel, sockerr)
{
  const auto hub = new Hub::Hub ("Hub");

  auto ch = channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=0\nchannel=test");
  channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

  // Wait for ERRSOCK on joining the closed socket
  EXPECT_THROW({
    hub->activate();
  }, channeling::activate_error);

  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  delete hub;
}

TEST(IrcChannel, socket)
{
  unlink("output");
  const auto hub = new Hub::Hub ("Hub");
  int sfd, nfd;
  const std::string& msgLine = ":testuser!~testhost PRIVMSG #chatsync :message\r\n";

  const auto server = std::make_unique<std::thread>(std::thread(&sockListen, msgLine, std::ref(sfd), std::ref(nfd)));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );    // Give time to open socket

  const auto ich = channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=" + std::to_string(port) + "\nchannel=test");
  channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");
  const std::string valid_line = "testuser: message";
  const int buffer_size = valid_line.length() + 1;
  const auto buffer = new char[buffer_size];

  hub->activate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  delete hub;
  server->join();

  const auto fd = open("output", O_RDONLY | O_SYNC);
  ASSERT_NE(fd, -1);
  bzero(buffer, buffer_size);
  auto err = read(fd, buffer, buffer_size - 1);
  ASSERT_EQ(err, buffer_size - 1);
  close(fd);
  ASSERT_STREQ(buffer, valid_line.c_str());
  delete[] buffer;
  close(sfd);
  close(nfd);
}

TEST(IrcChannel, Action)
{
  unlink("output");
  const auto hub = new Hub::Hub ("Hub");
  int sfd, nfd;
  const std::string& actionLine = ":testuser!~testhost PRIVMSG #chatsync :\001ACTION test\001\r\n";
  const auto server = std::make_unique<std::thread>(std::thread(&sockListen, std::ref(actionLine), std::ref(sfd), std::ref(nfd)));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );    // Give time to open socket
  const auto ich = channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=" + std::to_string(port) + "\nchannel=test");
  channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");
  const std::string valid_line = "testuser[ACTION]: test";
  const int buffer_size = valid_line.length() + 1;
  const auto buffer = new char[buffer_size];

  hub->activate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  delete hub;
  server->join();

  const auto fd = open("output", O_RDONLY | O_SYNC);
  ASSERT_NE(fd, -1);
  bzero(buffer, buffer_size);
  auto err = read(fd, buffer, buffer_size - 1);
  ASSERT_EQ(err, buffer_size - 1);
  close(fd);
  ASSERT_STREQ(buffer, valid_line.c_str());
  delete[] buffer;
  close(sfd);
  close(nfd);
}

TEST(IrcChannel, MultiLine)
{
  unlink("output");
  const auto hub = new Hub::Hub ("Hub");
  int sfd, nfd;

  const std::string& msgLine = ":testuser!~testhost PRIVMSG #chatsync :message1\r\n:testuser!~testhost PRIVMSG #chatsync :message2\r\n:testuser!~testhost PRIVMSG #chatsync :message3\r\n";
  const auto server = std::make_unique<std::thread>(std::thread(&sockListen, std::ref(msgLine), std::ref(sfd), std::ref(nfd)));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );    // Give time to open socket
  const auto ich = channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=" + std::to_string(port) + "\nchannel=test");
  channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");
  const std::string valid_line = "testuser: message1\ntestuser: message2\ntestuser: message3\n";
  const int buffer_size = valid_line.length() + 1;
  const auto buffer = new char[buffer_size];

  hub->activate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (150) );
  delete hub;
  server->join();

  const auto fd = open("output", O_RDONLY | O_SYNC);
  ASSERT_NE(fd, -1);
  bzero(buffer, buffer_size);
  auto err = read(fd, buffer, buffer_size - 1);
  ASSERT_EQ(err, buffer_size - 1);
  close(fd);
  ASSERT_STREQ(buffer, valid_line.c_str());
  delete[] buffer;
  close(sfd);
  close(nfd);
}

TEST(IrcChannel, LoremIpsum)
{
  const auto hub = std::make_unique<Hub::Hub>("Hub");
  int sfd, nfd;
  constexpr int buffer_size = 4096;
  std::vector<char> buffer;
  buffer.reserve(buffer_size);

  const std::string& msgLine = std::string(":testuser!~testhost PRIVMSG #chatsync :") + lorem_ipsum + "\r\n";
  const auto server = std::make_unique<std::thread>(std::thread(&ircServer, buffer.data(), buffer_size, std::ref(sfd), std::ref(nfd)));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );    // Give time to open socket
  const auto ich = channeling::ChannelFactory::create("irc", hub.get(), "data://direction=output\nname=ircin\nserver=127.0.0.1\nport=" + std::to_string(port) + "\nchannel=test");

  hub->activate();
  const auto msg = std::make_shared<const messaging::TextMessage>(0xffff,
                                                                  std::make_shared<const messaging::User>(messaging::User("test")),
                                                                  lorem_ipsum);
  hub->newMessage(std::move(msg));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  server->join();

  ASSERT_STREQ(buffer.data(), valid_lorem_ipsum);
  close(sfd);
  close(nfd);
}
