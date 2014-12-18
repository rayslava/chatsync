#include "hub.hpp"
#include <memory>
#include <algorithm>
#include <sstream>

#include "messages.hpp"
#include "channel.hpp"


namespace Hub {
    Hub::Hub(std::string const &name):
    _name(name),
    _loopRunning(ATOMIC_FLAG_INIT)
    {
    }

    void Hub::addInput(Channeling::Channel * const channel) {
        if (!_outputChannels.empty() &&
             std::find_if(std::begin(_outputChannels), std::end(_outputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_inputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _inputChannels.push_back(std::unique_ptr<Channeling::Channel> (channel));
    }

    void Hub::addOutput(Channeling::Channel * const channel) {
        if (!_outputChannels.empty() &&
                std::find_if(std::begin(_outputChannels), std::end(_outputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_outputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _outputChannels.push_back(std::unique_ptr<Channeling::Channel> (channel));
    }

    void Hub::addChannel(Channeling::Channel * const channel) {
        switch (channel->direction()) {
            case Channeling::ChannelDirection::Input:
                addInput(channel);
                break;
            case Channeling::ChannelDirection::Output:
                addOutput(channel);
                break;
            default:
                throw std::runtime_error(ERR_NOT_IMPL);
        }
    }

    void Hub::newMessage(std::string const &msg) {
      pushMessage(msg);
    }

    const std::string Hub::popMessage() {
       std::unique_lock<std::mutex> mlock(_mutex);
       while (_messages.empty())
	   _cond.wait(mlock);

       auto item = _messages.front();
       _messages.pop();
       return item;
    }

    void Hub::pushMessage(const std::string& item) {
	std::unique_lock<std::mutex> mlock(_mutex);
	_messages.push(item);
	mlock.unlock();
	_cond.notify_one();
    }

    void Hub::pushMessage(const std::string&& item) {
	std::unique_lock<std::mutex> mlock(_mutex);
	_messages.push(std::move(item));
	mlock.unlock();
	_cond.notify_one();
    }

    void Hub::msgLoop() {
	while (_loopRunning) {
	    thread_local const std::string& msg(popMessage());
	    for (auto out = _outputChannels.begin(); out != _outputChannels.end(); ++out)
	      msg >> **out;
	std::cout << "Loop started, yay!";
	}
    }

    void Hub::activate() {
	if (_outputChannels.empty())
	    throw std::logic_error("Can't run with no outputs");
	_loopRunning = true;
	_msgLoop = std::make_unique<std::thread>(std::thread(&Hub::msgLoop, this));
    }

    void Hub::deactivate() {
	_loopRunning = false;
	pushMessage(MSG_EXITING);
	_msgLoop->join();
    }
}
