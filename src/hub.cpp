#include "hub.hpp"
#include <memory>
#include <algorithm>

#include "messages.hpp"
#include "channel.hpp"


namespace Hub {
    Hub::Hub(std::string const &name):
    _name(name)
    {
    }

    void Hub::addInput(Channeling::Channel const* channel) {
        if (!_outputChannels.empty() &&
             std::find_if(std::begin(_outputChannels), std::end(_outputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_inputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _inputChannels.push_back(std::unique_ptr<Channeling::Channel const> (channel));
    }

    void Hub::addOutput(Channeling::Channel const* channel) {
        if (!_inputChannels.empty() &&
                std::find_if(std::begin(_inputChannels), std::end(_inputChannels),
                [&channel](chanPtr const& p)->bool {return p.get() == channel;}) != std::end(_inputChannels))
            throw std::logic_error(ERR_HUB_CHAN_CANT_BE_IN_OUT);
        _outputChannels.push_back(std::unique_ptr<Channeling::Channel const> (channel));
    }

    void Hub::addChannel(const Channeling::Channel* channel) {
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
        throw std::runtime_error("Don't know what to do with " + msg);
    }
}
