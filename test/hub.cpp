#include "../src/hub.hpp"
#include "../src/ircchannel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

Channeling::Channel *chan;
Hub::Hub *hub;

TEST(hub, name)
{
    const auto& hubName = "Hub1";
    hub = new Hub::Hub(hubName);

    ASSERT_EQ(hub->name(), hubName);

    new ircChannel::IrcChannel("channel", Channeling::ChannelDirection::Input, hub);
    new ircChannel::IrcChannel("channel", Channeling::ChannelDirection::Input, hub);
    new ircChannel::IrcChannel("channel", Channeling::ChannelDirection::Input, hub);
    chan = new ircChannel::IrcChannel("channelin", Channeling::ChannelDirection::Input, hub);

    EXPECT_THROW(hub->activate(), std::logic_error);
    Channeling::Channel *ouch = new ircChannel::IrcChannel("channelout", Channeling::ChannelDirection::Output, hub);
    hub->activate();
    EXPECT_THROW({std::string("Test") >> *chan;}, std::logic_error);
    EXPECT_NO_THROW({std::string("Test") >> *ouch;});
    hub->deactivate();

    delete hub;
}
