#include "../src/ircchannel.hpp"
#include "../src/hub.hpp"
#include <gtest/gtest.h>

TEST(channell, name)
{
    const auto hub = new Hub::Hub ("Hub1");
    const auto& chanName = "Channel1";
    Channeling::Channel *ch = new ircChannel::IrcChannel(chanName, Channeling::ChannelDirection::Input, hub);

    ASSERT_EQ(ch->name(), chanName);
    ASSERT_EQ(ch->direction(), Channeling::ChannelDirection::Input);

    ASSERT_THROW({std::istringstream("line") >> *ch;}, std::runtime_error);
    delete hub;
}
