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

	int join(const std::string& ircServer, const uint32_t port, const std::string& channel);
	int disconnect();
	int sendMessage(const std::string& msg);

    public:
        explicit IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub,
			    const std::string& ircServer, const uint32_t port, const std::string& channel);
        ~IrcChannel();

    protected:
        void parse(const std::string& l);
    };
}
