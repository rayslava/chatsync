#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>

#include "messages.hpp"

namespace Channeling {
    Channel::Channel(std::string const &name, ChannelDirection const &direction, Hub::Hub* const hub):
    _name(name),
    _direction(direction),
    _hub(hub)
    {
        _hub->addChannel(this);
    }

    std::string const & Channel::name() const {
        return _name;
    }

    Channel& operator>>(const std::string &msg,  Channel& channel) {
	if (channel.direction() == Channeling::ChannelDirection::Input)
	    throw std::logic_error("Can't write data to input channel " + channel.name());
	channel.parse(msg);
        return channel;
    }

    void Channel::parseConfig(std::vector<std::string> const &lines) {
        if (lines.empty()) {
            throw std::runtime_error("Channel config is empty");
        }
        throw std::runtime_error(ERR_NOT_IMPL);
    }


}
