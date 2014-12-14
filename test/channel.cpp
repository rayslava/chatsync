#include "../src/ircchannel.h"
#include <gtest/gtest.h>

TEST(channell, name)
{
    const auto& chanName = "Channel1";
    const auto channel = new ircChannel::IrcChannel(chanName, Channeling::ChannelDirection::Bidirectional);

    ASSERT_EQ(channel->name(), chanName);
    ASSERT_EQ(channel->direction(), Channeling::ChannelDirection::Bidirectional);
    delete(channel);
}
