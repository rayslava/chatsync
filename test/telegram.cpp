#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

namespace telegram {
  TEST(Telegram, Name)
  {
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=input\nname=tg\nhash=asdfbg\n");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(),      "tg");
    ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);
    delete hub;
  }
}
