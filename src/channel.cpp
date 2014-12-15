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

    std::ostream& operator<<(std::ostream &out, Channel const &channel) {
        channel.print(out);
        return out;
    }

    std::istream& operator>>(std::istream &in, Channel& channel) {
        std::string line;
        std::getline(in, line);
        channel.parse(line);
        return in;
    }

    void Channel::parseConfig(std::vector<std::string> const &lines) {
        if (lines.empty()) {
            throw std::runtime_error("Channel config is empty");
        }
        throw std::runtime_error(ERR_NOT_IMPL);
    }
}
