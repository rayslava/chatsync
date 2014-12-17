#include "../src/ircchannel.hpp"
#include "../src/hub.hpp"
#include <gtest/gtest.h>

const auto hub = new Hub::Hub ("Hub1");

TEST(channell, name)
{
    const auto& chanName = "Channel1";
    Channeling::Channel *ich = new ircChannel::IrcChannel(chanName, Channeling::ChannelDirection::Input, hub);
    Channeling::Channel *och = new ircChannel::IrcChannel("outChannel", Channeling::ChannelDirection::Output, hub);

    ASSERT_EQ(ich->name(), chanName);
    ASSERT_EQ(ich->direction(), Channeling::ChannelDirection::Input);

    hub->activate();
    "line" >> *och;
    hub->deactivate();

    delete hub;
}
