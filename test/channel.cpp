#include "../src/ircchannel.hpp"
#include "../src/filechannel.hpp"
#include "../src/hub.hpp"
#include <gtest/gtest.h>
#include <chrono>

const auto hub = new Hub::Hub ("Hub1");

TEST(channell, name)
{
    const auto& chanName = "Channel1";
    Channeling::Channel *ich = new ircChannel::IrcChannel(chanName, Channeling::ChannelDirection::Input, hub);
    new fileChannel::FileChannel("file", Channeling::ChannelDirection::Input, hub);
    new fileChannel::FileChannel("outfile", Channeling::ChannelDirection::Output, hub);
    const auto& oneSec = std::chrono::milliseconds (1000);

    ASSERT_EQ(ich->name(), chanName);
    ASSERT_EQ(ich->direction(), Channeling::ChannelDirection::Input);

    hub->activate();

    std::this_thread::sleep_for( oneSec );
    hub->deactivate();

    delete hub;
}
