#include "../src/hub.hpp"
#include "../src/channel.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

constexpr auto& hubName = "Hub1";
Hub::Hub *hub = nullptr;

TEST(hub, init)
{
    hub = new Hub::Hub(hubName);
    delete hub;
}

TEST(hub, name)
{
    hub = new Hub::Hub(hubName);

    ASSERT_EQ(hub->name(), hubName);
    const auto inch = Channeling::ChannelFactory::create("file", hub, "data://direction=input\nname=file\n");    

    EXPECT_THROW({hub->activate();}, std::logic_error);
    auto ouch = Channeling::ChannelFactory::create("irc", hub, "data://direction=output\nname=ircin\nserver=127.0.0.1\nport=0\nchannel=test");

    EXPECT_THROW({std::string("Test") >> *inch;}, std::logic_error);
    EXPECT_NO_THROW({std::string("Test") >> *ouch;});
    std::this_thread::sleep_for(std::chrono::milliseconds (50));
    hub->deactivate();

    delete hub;
}

