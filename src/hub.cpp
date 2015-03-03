#include "hub.hpp"
#include <memory>
#include <algorithm>
#include <sstream>
#include <future>

#include "messages.hpp"
#include "channel.hpp"


namespace Hub {
    Hub::Hub(std::string const &name):
    _name(name),
    _loopRunning(ATOMIC_FLAG_INIT)
    {
    }

    void Hub::addInput(channeling::Channel * const channel) {
        if (!_inputChannels.empty() &&
             std::find_if(std::begin(_inputChannels), std::end(_inputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_inputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _inputChannels.push_back(std::unique_ptr<channeling::Channel> (channel));
    }

    void Hub::addOutput(channeling::Channel * const channel) {
        if (!_outputChannels.empty() &&
                std::find_if(std::begin(_outputChannels), std::end(_outputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_outputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _outputChannels.push_back(std::unique_ptr<channeling::Channel> (channel));
    }

    void Hub::addChannel(channeling::Channel * const channel) {
        switch (channel->direction()) {
            case channeling::ChannelDirection::Input:
                addInput(channel);
                break;
            case channeling::ChannelDirection::Output:
                addOutput(channel);
                break;
            default:
                throw std::runtime_error(ERR_NOT_IMPL);
        }
    }

    void Hub::newMessage(const messaging::message_ptr&& msg) {
      pushMessage(std::move(msg));
    }

    const messaging::message_ptr Hub::popMessage() {
       std::unique_lock<std::mutex> mlock(_mutex);
       while (_messages.empty() && _loopRunning)
	   _cond.wait(mlock);

       const auto item = _messages.front();
       _messages.pop();
       return item;
    }

    void Hub::pushMessage(const messaging::message_ptr&& item) {
	std::unique_lock<std::mutex> mlock(_mutex);
	_messages.push(std::move(item));
	mlock.unlock();
	_cond.notify_one();
    }

    void Hub::msgLoop() {
        while (_loopRunning) {
	    const auto msg = popMessage();
	    for (auto& out : _outputChannels)
		msg >> *out;
        }
    }

    void Hub::activate() {
        if (_outputChannels.empty())
            throw std::logic_error("Can't run with no outputs");

	std::vector<std::future<void>> activators;

        for (auto& out : _outputChannels)
	    activators.push_back(out->activate());

	bool ready = true;
	do {
	    ready = true;
	    for (auto& ch : activators) {
		ready &= ch.valid();
		if (ch.valid())
		    ch.get();
	    }
	} while (!ready);
	activators.clear();

        for (auto& in : _inputChannels)
            activators.push_back(in->activate());

	do {
	    ready = true;
	    for (auto& ch : activators) {
		ready &= ch.valid();
		if (ch.valid())
		    ch.get();
	    }

	} while (!ready);
	activators.clear();

        _loopRunning = true;
        _msgLoop = std::make_unique<std::thread>(std::thread(&Hub::msgLoop, this));
    }

    void Hub::deactivate() {
	if (!_loopRunning)
	    return;
	_loopRunning = false;
	const auto msg = std::make_shared<const messaging::TextMessage>(0xFFFF,
	    std::move(std::make_shared<const messaging::User>(messaging::User("system"))),
	    MSG_EXITING);
	pushMessage(std::move(msg));
	_msgLoop->join();
    }
}
