#include "../src/hub.hpp"
#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

constexpr auto& hubName = "Hub1";
Hub::Hub* hub = nullptr;

TEST(hub, init)
{
  hub = new Hub::Hub(hubName);
  delete hub;
}

TEST(hub, name)
{
  hub = new Hub::Hub(hubName);

  ASSERT_EQ(hub->name(), hubName);
  const auto inch = channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=file\n");

  EXPECT_THROW({hub->activate();
               }, std::logic_error);
  auto ouch = channeling::ChannelFactory::create("irc", hub, "data://direction=output\nname=ircin\nserver=127.0.0.1\nport=0\nchannel=test");
  const auto msg = std::make_shared<const messaging::TextMessage>(0xFFFF,
                                                                  std::make_shared<const messaging::User>(messaging::User("system")),
                                                                  "test");

  EXPECT_THROW({msg >> *inch;
               }, std::logic_error);
  EXPECT_NO_THROW({msg >> *ouch;
                  });
  std::this_thread::sleep_for(std::chrono::milliseconds (50));
  hub->deactivate();

  delete hub;
}


TEST(hub, tox_bidir)
{
  hub = new Hub::Hub(hubName);

  ASSERT_EQ(hub->name(), hubName);
  const auto inch = channeling::ChannelFactory::create("tox", hub, "data://direction=inout\nname=toxconnect\ndatafile=/tmp/toxdata\n");

  const auto msg = std::make_shared<const messaging::TextMessage>(0xFFFF,
                                                                  std::make_shared<const messaging::User>(messaging::User("system")),
                                                                  "test");

  std::this_thread::sleep_for(std::chrono::milliseconds (150));
  hub->deactivate();

  delete hub;
}
