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

#ifndef TRAVIS_BUILD
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
