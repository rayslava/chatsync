#include "toxchannel.hpp"
#include "messages.hpp"

namespace toxChannel {
    ToxChannel::ToxChannel(Hub::Hub* hub, const std::string&& config):
	Channeling::Channel(hub, std::move(config)),
	_tox(tox_new(NULL))    /** TODO: make options handling */
    {
    }

    void ToxChannel::activate() {
	toxInit();
	_pipeRunning = true;
	_thread = std::make_unique<std::thread> (std::thread(&ToxChannel::toxThread, this));
    }

    void ToxChannel::parse(const std::string &l) {
	std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }

    void ToxChannel::toxThread() {
	std::cerr << "[DEBUG] Starting tox thread" << std::endl;
	while (_pipeRunning) {
	    uint16_t rtmp = 0;
	    tox_wait_prepare(_tox, NULL, &rtmp);
	    uint8_t tox_wait_buffer = malloc(rtmp);
	    int error = 0;
	    error = tox_wait_execute(_tox, tox_wait_buffer, rtmp, 999);
	    tox_wait_cleanup(_tox, tox_wait_buffer, rtmp);
	    free(tox_wait_buffer);
	    tox_do(_tox);
	    std::this_thread::sleep_for( std::chrono::milliseconds (tox_do_interval(_tox)));
	}
    }

    ToxChannel::~ToxChannel() {
	if (_thread) {
	    _pipeRunning = false;
	    _thread->join();
	}
	tox_kill(_tox);
    }

    void ToxChannel::friendRequestCallback(Tox *tox, const uint8_t * public_key, const uint8_t * data, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);
    }

    void ToxChannel::messageCallback(Tox *tox, int32_t friendnumber, const uint8_t * message, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);
    }

    int ToxChannel::toxInit() {
	int result = 0;

	std::unique_ptr<uint8_t[]> pubKey(new uint8_t[TOX_CLIENT_ID_SIZE]);
	tox_callback_friend_request(_tox, friendRequestCallback, this);
	tox_callback_friend_message(_tox, messageCallback, this);


	const std::string nick = _config.get("nickname", defaultBotName);
	const uint8_t* nickData = reinterpret_cast<const uint8_t*>(nick.c_str());
	result = tox_set_name(_tox, nickData, nick.length());
	if (result < 0)
	    throw Channeling::activate_error(ERR_TOX_INIT + "(tox_set_name)");
	
	const std::string statusMsg = _config.get("status_message", defaultStatusMessage);
	const uint8_t* statusData = reinterpret_cast<const uint8_t*>(statusMsg.c_str());

	result = tox_set_status_message(_tox, statusData, statusMsg.length());
	if (result < 0)
	    throw Channeling::activate_error(ERR_TOX_INIT + "(tox_set_status_message)");

	result = tox_set_user_status(_tox, defaultBotStatus);
	if (result < 0)
	    throw Channeling::activate_error(ERR_TOX_INIT + "(tox_set_user_status)");

	result = tox_bootstrap_from_address(_tox, defaultBootstrapAddress, defaultBootstrapPort, reinterpret_cast<const uint8_t*>(defaultBootstrapKey));
	if (result < 1)
	    throw Channeling::activate_error(ERR_TOX_INIT + ": Can't decode bootstrapping ip");

	return 0;
    }

    #include <cassert>
    std::string hex2bin(std::string const& s) {
    assert(s.length() % 2 == 0);

    std::string sOut;
    sOut.reserve(s.length()/2);

    std::string extract;
    for (std::string::const_iterator pos = s.begin(); pos<s.end(); pos += 2)
    {
        extract.assign(pos, pos+2);
        sOut.push_back(std::stoi(extract, nullptr, 16));
    }
    return sOut;
}
}
