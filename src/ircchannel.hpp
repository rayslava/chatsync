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

    const std::string _server;                       /**< Server address */
    const uint32_t _port;                            /**< Connection port */
    const std::string _channel;                      /**< Channel name (starting with #) */

	int connect();
	int disconnect();
	int sendMessage(const std::string& msg);
	void activate();

	static const Channeling::ChannelCreatorImpl<IrcChannel> creator;
	
    public:
	explicit IrcChannel(Hub::Hub* hub, const std::string&& config);
        ~IrcChannel();

	std::string type() const {return "irc";};

    protected:
        void parse(const std::string& l);
    };
    const Channeling::ChannelCreatorImpl<IrcChannel> IrcChannel::creator("irc");
}
