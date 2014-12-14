#include <iostream>
#include "config.hpp"
#include "ircchannel.hpp"

int main()
{
    const auto ch = new ircChannel::IrcChannel ("lalka", Channeling::ChannelDirection::Bidirectional);

    try {
        std::cout << *ch;
    } catch (std::exception& e) {
        std::cout << "Fail :(";
    };
    delete ch;
    return 0;
}
