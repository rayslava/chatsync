#include "../src/channel.hpp"
#include <gtest/gtest.h>

TEST(ToxChannel, name)
{
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=tox\ndatafile=/tmp/toxdata\n");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(), "tox");
    ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);

    delete hub;
}

TEST(ToxChannel, running)
{
    const auto hub = new Hub::Hub ("Hub");
    channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=tox\ndatafile=/tmp/toxdata\n");
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    hub->activate();
    std::this_thread::sleep_for( std::chrono::milliseconds (20000) );
    hub->deactivate();

    delete hub;
}
