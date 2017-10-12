#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <fcntl.h>

constexpr auto port = 33445;
const char testLine[] = "Testing file writing\r\n";

TEST(FileChannel, name)
{
  const auto hub = new Hub::Hub ("Hub");
  const auto ich = channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=file\n");

  // Check that at least constructor works
  ASSERT_EQ(ich->name(),      "file");
  ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  delete hub;
}

TEST(FileChannel, files)
{
  unlink("output");
  const auto hub = new Hub::Hub ("Hub");

  const auto ich = channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=infile");
  const int buffer_size = sizeof(testLine) + ich->name().length() + 2 + 5;   // file: and ": " sizes
  const std::string valid_line = "file:" + ich->name() + ": " + testLine;
  const auto buffer = new char[buffer_size];

  channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

  // Check that at least constructor works
  ASSERT_EQ(ich->name(),      "infile");
  ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);

  // Open input pipe from file channel
  hub->activate();
  int fd = open("input", O_WRONLY | O_SYNC, 0666);
  ASSERT_NE(fd, -1);

  int err = write(fd, testLine, sizeof(testLine));
  ASSERT_EQ(err, sizeof(testLine));
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  hub->deactivate();
  std::this_thread::sleep_for( std::chrono::milliseconds (50) );
  delete hub;

  close(fd);    // @todo move after adding file close support in poll thread

  fd = open("output", O_RDONLY | O_SYNC);
  ASSERT_NE(fd, -1);
  bzero(buffer, buffer_size);
  err = read(fd, buffer, buffer_size - 1);
  ASSERT_EQ(err, buffer_size - 1);
  close(fd);
  ASSERT_STREQ(buffer, valid_line.c_str());
  delete[] buffer;
}
