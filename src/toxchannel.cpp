#include "toxchannel.hpp"
#include "messages.hpp"

namespace toxChannel {

    namespace util {
	/* Thanks to [0xd34df00d](https://github.com/0xd34df00d) for this bunch of functions */

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

	template<size_t Size>
	std::string ToxId2HR (const uint8_t *address)
	{
	    std::string result;
	    auto toHexChar = [] (uint8_t num) -> char
		{
		    return num >= 10 ? (num - 10 + 'A') : (num + '0');
		};

	    for (size_t i = 0; i < Size; ++i)
		{
		    const auto num = address [i];
		    result += toHexChar ((num & 0xf0) >> 4);
		    result += toHexChar (num & 0xf);
		}
	    return result;
	}

	template<size_t Size>
	std::string ToxId2HR (const std::array<uint8_t, Size>& address)
	{
	    return ToxId2HR<Size> (address.data ());
	}
#include <string.h>
    }

    ToxChannel::ToxChannel(Hub::Hub* hub, const std::string&& config):
	channeling::Channel(hub, std::move(config)),
	_tox(tox_new(NULL)),    /** TODO: make options handling */
	wasConnected(false)
    {
    }

    std::future<void> ToxChannel::activate() {
	return std::async(std::launch::async, [this]() {
		_pipeRunning = true;
		toxInit();
		_thread = std::make_unique<std::thread> (std::thread(&ToxChannel::pollThread, this));
	    });}

    void ToxChannel::incoming(const messaging::message_ptr&& msg) {
        if (msg->type() == messaging::MessageType::Text) {
            const auto textmsg = messaging::TextMessage::fromMessage(msg);
            std::cerr << "[DEBUG] #tox " << _name << " " << textmsg->data() << std::endl;
        }
    }

    void ToxChannel::pollThread() {
	std::cerr << "[DEBUG] Starting tox thread" << std::endl;
	while (_pipeRunning) {
	    tox_do(_tox);
	    auto wait = tox_do_interval(_tox);
	    std::this_thread::sleep_for( std::chrono::milliseconds (wait));
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
	const auto friendNum = tox_add_friend_norequest(tox, public_key);
	std::cerr << "[DEBUG] tox id with data" << data << " of " << length << " bytes "  << util::ToxId2HR<TOX_FRIEND_ADDRESS_SIZE>(public_key) << " wants to be your friend. Added with #" << friendNum << std::endl;
    }

    void ToxChannel::messageCallback(Tox *tox, int32_t friendnumber, const uint8_t * message, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);
	const auto buffer = std::make_unique<char*>(new char[length+1]);
	snprintf(*buffer, length+1, "%s", message);
	std::cerr << "[DEBUG] Message from friend #" << friendnumber << "> " << *buffer << std::endl;
	if (util::strncmp(cmd_invite, *buffer, length) == 0)
	    tox_invite_friend(tox, friendnumber, 0);
    }

    void ToxChannel::groupMessageCallback(Tox *tox, int32_t groupnumber, int32_t peernumber, const uint8_t * message, uint16_t length, void *userdata) {
	const auto channel = static_cast<ToxChannel*>(userdata);

	const auto nameBuffer = std::make_unique<uint8_t*>(new uint8_t[TOX_MAX_NAME_LENGTH]);
	const auto nameLen = tox_group_peername(tox, groupnumber, peernumber, *nameBuffer);
	const auto name = std::make_unique<char*>(new char[nameLen + 2]);
	snprintf(*name, nameLen + 1, "%s", *nameBuffer);
	
	const auto msg = std::make_unique<char*>(new char[length + 2]);
	snprintf(*msg, length + 1, "%s", message);

	const auto newMessage = std::make_shared<const messaging::TextMessage>(channel->_id,
	    std::move(std::make_shared<const messaging::User>(messaging::User(*name))),
	    *msg);
	std::cerr << "[DEBUG] tox Group msg " << newMessage->user()->name() << "> " << newMessage->data() << std::endl;	
	channel->_hub->newMessage(std::move(newMessage));
    }

    const messaging::message_ptr ToxChannel::parse(const char* line) const
    {
        const std::string s(line);
        const auto name = s.substr(0, s.find(":"));
        const auto text = s.substr(s.find(":"), s.length());

        const auto msg = std::make_shared<const messaging::TextMessage>(_id,
            std::move(std::make_shared<const messaging::User>(messaging::User(name.c_str()))),
	    text.c_str());
        return msg;
    }


    int ToxChannel::toxInit() {
	int result = 0;

	std::unique_ptr<uint8_t[]> pubKey(new uint8_t[TOX_CLIENT_ID_SIZE]);
	tox_callback_friend_request(_tox, friendRequestCallback, this);
	tox_callback_friend_message(_tox, messageCallback, this);
	tox_callback_group_message(_tox, groupMessageCallback, this);

	const std::string nick = _config.get("nickname", defaultBotName);
	const uint8_t* nickData = reinterpret_cast<const uint8_t*>(nick.c_str());
	result = tox_set_name(_tox, nickData, nick.length());
	if (result < 0)
	    throw channeling::activate_error(ERR_TOX_INIT + "(tox_set_name)");
	
	const std::string statusMsg = _config.get("status_message", defaultStatusMessage);
	const uint8_t* statusData = reinterpret_cast<const uint8_t*>(statusMsg.c_str());

	result = tox_set_status_message(_tox, statusData, statusMsg.length());
	if (result < 0)
	    throw channeling::activate_error(ERR_TOX_INIT + "(tox_set_status_message)");

	result = tox_set_user_status(_tox, defaultBotStatus);
	if (result < 0)
	    throw channeling::activate_error(ERR_TOX_INIT + "(tox_set_user_status)");

	result = tox_bootstrap_from_address(_tox, defaultBootstrapAddress, defaultBootstrapPort, reinterpret_cast<const uint8_t*>(util::hex2bin(defaultBootstrapKey).c_str()));
	if (result < 1)
	    throw channeling::activate_error(ERR_TOX_INIT + ": Can't decode bootstrapping ip");

	std::cerr << "[DEBUG] Bootstrapping" << std::endl;
	/* TODO: Make timeout exception handling */

	while (!wasConnected) {
	    tox_do(_tox);
	    auto wait = tox_do_interval(_tox);
	    if (! wasConnected && tox_isconnected(_tox)) {
		std::array<uint8_t, TOX_FRIEND_ADDRESS_SIZE> address;
		tox_get_address (_tox, address.data ());
		std::cerr << "[DEBUG] Tox is connected with id " << util::ToxId2HR (address) << std::endl
;
		wasConnected = true;
	    }
	    std::this_thread::sleep_for( std::chrono::milliseconds (wait));
	}

	result = tox_add_groupchat (_tox);
	if (result < 0)
	    throw channeling::activate_error(ERR_TOX_INIT + "(tox_add_groupchat) Can't create a group chat");

	return 0;
    }
}
