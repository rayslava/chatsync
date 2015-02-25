#include <iostream>
#include "channel.hpp"
#include "config.hpp"

int main()
{
    const auto hub = new Hub::Hub("Hubeg");
    const auto ch = Channeling::ChannelFactory::create("irc", hub, "data://direction=output\nname=irc\nserver=irc.freenode.net\nport=6667\nchannel=#chatsync\nnickname=csb");
    std::cout << ch->name() << std::endl;
    Channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=logfile");
    Channeling::ChannelFactory::create("tox", hub, "data://direction=input\nname=toxconnect");
    hub->activate();
    std::this_thread::sleep_for(std::chrono::milliseconds (50000));
    hub->deactivate();
    delete hub;
    return 0;
}
