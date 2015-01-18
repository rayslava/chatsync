#include "../src/hub.hpp"
#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

constexpr auto& hubName = "Hub1";
Hub::Hub *hub = nullptr;

TEST(hub, init)
{
    hub = new Hub::Hub(hubName);
    delete hub;
}
/*
TEST(hub, name)
{
    hub = new Hub::Hub(hubName);

    ASSERT_EQ(hub->name(), hubName);
    auto inch = new fileChannel::FileChannel("file", Channeling::ChannelDirection::Input, hub);
    EXPECT_THROW({hub->activate();}, std::logic_error);
    Channeling::Channel *ouch = new ircChannel::IrcChannel("channelout", Channeling::ChannelDirection::Output, hub); //, "irc.freenode.net", 6667, "chatsync");

    EXPECT_THROW({std::string("Test") >> *inch;}, std::logic_error);
    EXPECT_NO_THROW({std::string("Test") >> *ouch;});
    std::this_thread::sleep_for(std::chrono::milliseconds (50));
    hub->deactivate();

    delete hub;
}
*/
