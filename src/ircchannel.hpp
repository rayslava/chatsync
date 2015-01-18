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

	int join();
	int disconnect();
	int sendMessage(const std::string& msg);
	void activate();

	static const Channeling::ChannelCreatorImpl<IrcChannel> creator;
	
    public:
	explicit IrcChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub);
//			    const std::string& ircServer, const uint32_t port, const std::string& channel);
        ~IrcChannel();

	std::string type() const {return "irc";};
        virtual void parseConfig(std::vector<std::string> const& lines);

    protected:
        void parse(const std::string& l);
    };
    const Channeling::ChannelCreatorImpl<IrcChannel> IrcChannel::creator("irc");
}
