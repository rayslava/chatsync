#include "toxchannel.hpp"

namespace toxChannel {
    ToxChannel::ToxChannel(Hub::Hub* hub, const std::string&& config):
	Channeling::Channel(hub, std::move(config))
    {
    }

    void ToxChannel::activate() {
    }

    void ToxChannel::parse(const std::string &l) {
	std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }

    ToxChannel::~ToxChannel() {
    }
}
