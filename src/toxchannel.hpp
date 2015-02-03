#pragma once
#include "channel.hpp"

namespace toxChannel {
    /**
    * Tox channel
    *
    *
    */
    class ToxChannel: public Channeling::Channel {
	void activate();
	static const Channeling::ChannelCreatorImpl<ToxChannel> creator;
    public:
	explicit ToxChannel(Hub::Hub* hub, const std::string&& config);
        ~ToxChannel();

	std::string type() const {return "tox";};

    protected:
        void parse(const std::string& l);
    };
    const Channeling::ChannelCreatorImpl<ToxChannel> ToxChannel::creator("tox");
}
