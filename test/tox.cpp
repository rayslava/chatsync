#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <tox/tox.h>

namespace toxChannel {
  namespace util {
      template <size_t Size>
      std::string ToxId2HR(const uint8_t* address);
  }
}

TEST(ToxUtil, hex2bin)
{
    const uint8_t toxIdHex[TOX_ADDRESS_SIZE] = {
	0x96, 0x18, 0xfc, 0x56, 0x1e, 0x9e, 0xd1, 0xa1, 0xb3, 0x72, 0x62, 0x63,
	0x8a, 0x50, 0xb2, 0x5c, 0x6e, 0x5e, 0xad, 0x39, 0x43, 0xb9, 0x39, 0x4f,
	0x78, 0x69, 0x28, 0x0b, 0xb2, 0x69, 0x46, 0x36, 0x49, 0x69, 0x0c, 0x9c,
	0x14, 0x35
    };

    const std::string toxIdStr = "9618FC561E9ED1A1B37262638A50B25C6E5EAD3943B9394F7869280BB269463649690C9C1435";

    const auto converted = toxChannel::util::ToxId2HR<TOX_ADDRESS_SIZE>(toxIdHex);
    ASSERT_STREQ(converted.c_str(), toxIdStr.c_str());
}

TEST(ToxChannel, name)
{
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=tox\ndatafile=/tmp/toxdata\n");

    // Check that at least constructor works
    ASSERT_EQ(ich->name(), "tox");
    ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Input);

    delete hub;
}

#if !defined(TRAVIS_BUILD)
TEST(ToxChannel, running)
{
    const auto hub = new Hub::Hub ("Hub");
    channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=tox\ndatafile=/tmp/toxdata\n");
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    std::promise<bool> promisedFinished;
    auto futureResult = promisedFinished.get_future();
    std::thread([](std::promise<bool>& finished, Hub::Hub* hub) {
	    hub->activate();
	    std::this_thread::sleep_for( std::chrono::milliseconds (10000) );
	    hub->deactivate();
	    finished.set_value(true);
	}, std::ref(promisedFinished), std::ref(hub)).detach();

    EXPECT_TRUE(futureResult.wait_for(std::chrono::milliseconds(30000))!= std::future_status::timeout);

    delete hub;
}
#endif
