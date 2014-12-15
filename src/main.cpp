#include <iostream>
#include "config.hpp"
#include "ircchannel.hpp"

int main()
{
    const auto hub = new Hub::Hub("Hubeg");
    const auto ch = new ircChannel::IrcChannel("lalka", Channeling::ChannelDirection::Input, hub);

    try {
        std::cout << *ch;
    } catch (std::exception& e) {
        std::cout << "Fail :(" << std::endl;
    };
    delete hub;
    return 0;
}
