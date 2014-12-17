#include "ircchannel.hpp"
#include "hub.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub) :
    Channeling::Channel(name, direction, hub)
    {
    }

    std::string const& IrcChannel::parse(const std::string &l) const {
        std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
        return l;
    }
}
