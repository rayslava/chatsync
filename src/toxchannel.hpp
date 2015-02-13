#pragma once
#include "channel.hpp"
#include <tox/tox.h>

namespace toxChannel {

    constexpr auto defaultBootstrapAddress = "23.226.230.47"; /**< Tox bootstrap address TODO: enable grabbing from gist */
    constexpr uint32_t defaultBootstrapPort = 33445; /**< Tox bootstrap server port */
    constexpr auto defaultBootstrapKey = "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074"; /**< Tox default bootstrap key */

    constexpr auto defaultBotName = "chatsyncbot"; /**< Default bot nickname */
    constexpr auto defaultStatusMessage = "Online"; /**< Default tox status message */
    constexpr auto defaultBotStatus = TOX_USERSTATUS_NONE; /**< Default bot name */
    /**
    * Tox channel
    */
    class ToxChannel: public Channeling::Channel {
	Tox* const _tox;    /**< Main tox structure */
	int toxInit();
	void activate();

	static void friendRequestCallback(Tox *tox, const uint8_t * public_key, const uint8_t * data, uint16_t length, void *userdata);
	static void messageCallback(Tox *tox, int32_t friendnumber, const uint8_t * message, uint16_t length, void *userdata);
	static const Channeling::ChannelCreatorImpl<ToxChannel> creator;

	void toxThread(); /**< Thread for tox infinite loop */
    public:
	explicit ToxChannel(Hub::Hub* hub, const std::string&& config);
        ~ToxChannel();

	std::string type() const {return "tox";};

    protected:
        void parse(const std::string& l);
    };
    const Channeling::ChannelCreatorImpl<ToxChannel> ToxChannel::creator("tox");
}
