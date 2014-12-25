#include <iostream>
#include "config.hpp"
#include "ircchannel.hpp"

int main()
{
    const auto hub = new Hub::Hub("Hubeg");
    const auto ch = new ircChannel::IrcChannel("lalka", Channeling::ChannelDirection::Input, hub, "irc.freenode.net", 6667, "chatsync");
    std::cout << ch->name() << std::endl;
    delete hub;
    return 0;
}
