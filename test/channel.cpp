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
const char testLine[] = "Testing file writing";


TEST(FileChannel, name)
{
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = Channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=file\n");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(), "file");
    ASSERT_EQ(ich->direction(), Channeling::ChannelDirection::Input);

    delete hub;
}

TEST(FileChannel, files)
{
    const auto hub = new Hub::Hub ("Hub");

    char buffer[sizeof(testLine)];
    const auto ich = Channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=infile");

    Channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(), "infile");
    ASSERT_EQ(ich->direction(), Channeling::ChannelDirection::Input);

    // Open input pipe from file channel
    hub->activate();
    int fd = open("input", O_WRONLY | O_SYNC, 0666);
    ASSERT_NE(fd, -1);

    int err = write(fd, testLine, sizeof(testLine));
    ASSERT_EQ(err, sizeof(testLine));
    std::this_thread::sleep_for( std::chrono::milliseconds (50) );
    hub->deactivate();
    delete hub;

    close(fd);  // TODO: move after adding file close support in poll thread

    fd = open("output", O_RDONLY | O_SYNC);
    ASSERT_NE(fd, -1);
    bzero(buffer, sizeof(buffer));
    err = read(fd, buffer, sizeof(buffer) - 1);
    ASSERT_EQ(err, sizeof(buffer) - 1);
    close(fd);
    ASSERT_STREQ(buffer, testLine);
}

void sockListen() {
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int  n;

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

	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	if (newsockfd < 0) {
	    perror("ERROR on accept");
	    exit(1);
	}

	bzero(buffer,256);

	n = write(newsockfd, testLine, sizeof(testLine));
	if (n < 0) {
	    perror("ERROR writing to socket");
	    exit(1);
	}
}

TEST(IrcChannel, sockerr)
{
    const auto hub = new Hub::Hub ("Hub");

    auto ch = Channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=0\nchannel=test");
    Channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    // Wait for ERRSOCK on joining the closed socket
    EXPECT_THROW({
        hub->activate();
    },  Channeling::activate_error);
    delete hub;
}

TEST(IrcChannel, socket)
{
    const auto hub = new Hub::Hub ("Hub");
    char buffer[sizeof(testLine)];

    const auto server = std::make_unique<std::thread>(std::thread(&sockListen));
    std::this_thread::sleep_for( std::chrono::milliseconds (50) );  // Give time to open socket 
    auto ich = Channeling::ChannelFactory::create("irc", hub, "data://direction=input\nname=ircin\nserver=127.0.0.1\nport=" + std::to_string(port) + "\nchannel=test");
    Channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    hub->activate();
    std::this_thread::sleep_for( std::chrono::milliseconds (50) );
    hub->deactivate();
    delete hub;
    server->join();

    const auto fd = open("output", O_RDONLY | O_SYNC);
    ASSERT_NE(fd, -1);
    bzero(buffer, sizeof(buffer));
    auto err = read(fd, buffer, sizeof(buffer) - 1);
    ASSERT_EQ(err, sizeof(buffer) - 1);
    close(fd);
    ASSERT_STREQ(buffer, testLine);
}
