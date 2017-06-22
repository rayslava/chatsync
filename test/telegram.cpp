#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <cstdio>

namespace telegram {
  const std::string hash = getenv("TELEGRAM_BOT_HASH");
  const std::string botid = getenv("TELEGRAM_BOT_ID");
  const std::string chat = getenv("TELEGRAM_BOT_CHAT");
  /*
     TEST(Telegram, Name)
     {
     DEFAULT_LOGGING
     const auto hub = new Hub::Hub ("Hub");
     const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=input\nname=tg\nbotid=" + botid + "\nhash=" + hash + "\nchat=test");

     // Check that at least constructor works
     ASSERT_EQ(ich->name(),	"tg");
     ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);
     delete hub;
     }
   */
  TEST(Telegram, apiReq)
  {
    DEFAULT_LOGGING
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=" + botid + "\nhash=" + hash + "\nchat=" + chat);
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");
    auto msg = std::make_shared<const messaging::TextMessage>(0,
                                                              std::make_shared<const messaging::User>( messaging::User("R")), "test");
    hub->activate();
    hub->newMessage(std::move(msg));
    std::this_thread::sleep_for( std::chrono::milliseconds (20000) );
    hub->deactivate();

    // Check that at least constructor works
    delete hub;
  }
}
