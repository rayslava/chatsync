#include "toxchannel.hpp"

namespace toxChannel {
    ToxChannel::ToxChannel(Hub::Hub* hub, const std::string&& config):
	Channeling::Channel(hub, std::move(config)),
	_tox(tox_new(NULL))    /** TODO: make options handling */
    {
    }

    void ToxChannel::activate() {
	toxInit();
    }

    void ToxChannel::parse(const std::string &l) {
	std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }

    ToxChannel::~ToxChannel() {
	tox_kill(_tox);
    }

    void ToxChannel::friendRequestCallback(Tox *tox, const uint8_t * public_key, const uint8_t * data, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);
    }

    void ToxChannel::messageCallback(Tox *tox, int32_t friendnumber, const uint8_t * message, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);
    }

    int ToxChannel::toxInit() {
	std::unique_ptr<uint8_t[]> pubKey(new uint8_t[TOX_CLIENT_ID_SIZE]);
	tox_callback_friend_request(_tox, friendRequestCallback, this);
	tox_callback_friend_message(_tox, messageCallback, this);


	const std::string nick = _config.get("nickname", defaultBotName);
	const uint8_t* nickData = reinterpret_cast<const uint8_t*>(nick.c_str());
	tox_set_name(_tox, nickData, nick.length());
	
	const std::string statusMsg = _config.get("status_message", defaultStatusMessage);
	const uint8_t* statusData = reinterpret_cast<const uint8_t*>(statusMsg.c_str());

	tox_set_status_message(_tox, statusData, statusMsg.length());

	tox_set_user_status(_tox, defaultBotStatus);
	return 0;
    }
}
