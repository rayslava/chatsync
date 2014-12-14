#include "ircchannel.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(std::string const & name, Channeling::ChannelDirection const &direction):
    Channeling::Channel(name, direction)
    {
    }

    void IrcChannel::print(std::ostream &o) const {
        if (o)
            throw std::runtime_error("Output not implementer yet");
    }

    void IrcChannel::parse(std::string &l) {
        if (!l.empty())
            throw std::runtime_error("Input not implementer yet");
    }
}