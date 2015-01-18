#include <iostream>
#include "channel.hpp"
#include "config.hpp"

int main()
{
    const auto hub = new Hub::Hub("Hubeg");
    const auto ch = Channeling::ChannelFactory::create("irc", "lalka", Channeling::ChannelDirection::Input, hub); //, "irc.freenode.net", 6667, "chatsync");
    std::cout << ch->name() << std::endl;
    delete hub;
    return 0;
}
