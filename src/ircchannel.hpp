#pragma once
#include "channel.hpp"
#include "hub.hpp"

namespace ircChannel {

    /**
    * IRC connection channel
    *
    * Capable of connection an IRC server, joining one single channel and message transmission/receiving.
    * Responds to PING with PONG to maintain connection.
    */
    class IrcChannel: public Channeling::Channel {
    public:
        explicit IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub);
        ~IrcChannel() {};

    protected:
        std::string const& parse(const std::string& l) const;
    };
}
