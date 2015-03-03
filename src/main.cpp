#include <iostream>
#include "channel.hpp"
#include "config.hpp"

int main()
{
    const auto hub = new Hub::Hub("Hubeg");
    const auto ch = channeling::ChannelFactory::create("irc", hub, "data://direction=inout\nname=irc\nserver=irc.freenode.net\nport=6667\nchannel=#chatsync\nnickname=csb");
    std::cout << ch->name() << std::endl;
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=logfile");
    channeling::ChannelFactory::create("tox", hub, "data://direction=inout\nname=toxconnect");
    hub->activate();
    std::this_thread::sleep_for(std::chrono::milliseconds (50000));
    hub->deactivate();
    delete hub;
    return 0;
}
