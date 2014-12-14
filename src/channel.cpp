#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>

namespace Channeling {
    Channel::Channel(std::string const &name, ChannelDirection const &direction):
    _name(name),
    _direction(direction)
    {
    }

    std::string const & Channel::name() {
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
        throw std::runtime_error("Not implemented yet");
    }
}
