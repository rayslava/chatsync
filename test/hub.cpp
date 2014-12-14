#include "../src/hub.hpp"
#include "../src/ircchannel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

ircChannel::IrcChannel *chan;
Hub::Hub *hub;

TEST(hub, name)
{
    const auto& hubName = "Hub1";
    hub = new Hub::Hub(hubName);

    ASSERT_EQ(hub->name(), hubName);

    chan = new ircChannel::IrcChannel("channel", Channeling::ChannelDirection::Input);

    hub->addInput(chan);
    EXPECT_THROW(hub->addOutput(chan), std::logic_error);

    delete(hub);
}
