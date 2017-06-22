#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <cstdio>

namespace telegram {
  const std::string hash = getenv("TELEGRAM_BOT_HASH");
  const std::string botid = getenv("TELEGRAM_BOT_ID");

  TEST(Telegram, Name)
  {
    DEFAULT_LOGGING
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=input\nname=tg\nbotid=" + botid + "\nhash=" + hash);

    // Check that at least constructor works
    ASSERT_EQ(ich->name(),	"tg");
    ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);
    delete hub;
  }

  TEST(Telegram, apiReq)
  {
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=input\nname=tg\nbotid=" + botid + "\nhash=" + hash);

    // Check that at least constructor works
    delete hub;
  }
}
