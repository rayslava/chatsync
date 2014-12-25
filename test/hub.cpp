#include "../src/hub.hpp"
#include "../src/ircchannel.hpp"
#include "../src/filechannel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

Hub::Hub *hub;

TEST(hub, name)
{
    const auto& hubName = "Hub1";
    hub = new Hub::Hub(hubName);

    ASSERT_EQ(hub->name(), hubName);

    //    EXPECT_THROW(hub->activate(), std::logic_error);
    //    Channeling::Channel *ouch = new ircChannel::IrcChannel("channelout", Channeling::ChannelDirection::Output, hub, "irc.freenode.net", 6667, "chatsync");
    EXPECT_THROW({hub->activate();}, std::logic_error);
    //    EXPECT_THROW({std::string("Test") >> *chan;}, std::logic_error);
    //    EXPECT_NO_THROW({std::string("Test") >> *ouch;});
    std::this_thread::sleep_for(std::chrono::milliseconds (100));
    hub->deactivate();

    delete hub;
}
