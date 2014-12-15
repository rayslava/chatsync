#include "ircchannel.hpp"
#include "hub.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub) :
    Channeling::Channel(name, direction, hub)
    {
    }

    void IrcChannel::print(std::ostream &o) const {
        if (o)
            throw std::runtime_error("Output not implementer yet");
    }

    std::string const& IrcChannel::parse(std::string &l) {
        _hub->newMessage(l);
        return l;
    }
}