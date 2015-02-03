#include "../src/channel.hpp"
#include <gtest/gtest.h>

TEST(ToxChannel, name)
{
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = Channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=tox\n");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(), "tox");
    ASSERT_EQ(ich->direction(), Channeling::ChannelDirection::Input);

    delete hub;
}
