#include "ircchannel.hpp"
#include "hub.hpp"
#include <stdexcept>
#include <typeinfo>

namespace ircChannel {
    IrcChannel::IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub) :
    Channeling::Channel(name, direction, hub)
    {
    }

    void IrcChannel::parse(const std::string &l) {
        std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }
}
