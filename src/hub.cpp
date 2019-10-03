#include "hub.hpp"
#include <memory>
#include <algorithm>
#include <future>

#include "messages.hpp"
#include "channel.hpp"


namespace Hub {
  Hub::Hub(std::string const& name) :
    _name(name),
    _mutex(),
    _cond(),
    _loopRunning(ATOMIC_FLAG_INIT),
    _alive(ATOMIC_FLAG_INIT)
  {
    _alive.store(true, std::memory_order_release);
  }

  void Hub::addInput(channeling::Channel * const channel) {
    if (!_inputChannels.empty() &&
        std::find_if(std::begin(_inputChannels), std::end(_inputChannels),
                     [&channel](chanPtr const& p) -> bool {
      return p.get() == channel;
    }) != std::end(_inputChannels))
      throw std::logic_error(ERR_HUB_CHANNEL_ALREADY_IN);
    _inputChannels.push_back(std::unique_ptr<channeling::Channel>(channel));
  }

  void Hub::addOutput(channeling::Channel * const channel) {
    if (!_outputChannels.empty() &&
        std::find_if(std::begin(_outputChannels), std::end(_outputChannels),
                     [&channel](chanPtr const& p) -> bool {
      return p.get() == channel;
    }) != std::end(_outputChannels))
      throw std::logic_error(ERR_HUB_CHANNEL_ALREADY_IN);
    _outputChannels.push_back(std::unique_ptr<channeling::Channel>(channel));
  }

  void Hub::addChannel(channeling::Channel * const channel) {
    switch (channel->direction()) {
    case channeling::ChannelDirection::Input:
      addInput(channel);
      break;
    case channeling::ChannelDirection::Output:
      addOutput(channel);
      break;
    case channeling::ChannelDirection::Bidirectional:
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
    /* We'll sleep here during the reconnection attempts to prevent resource
       deadlock while the channel tries to reach the server and start
       messaging again */
    while (!_loopRunning.load(std::memory_order_acquire))
      std::this_thread::sleep_for(std::chrono::milliseconds (1000));

    std::unique_lock<std::mutex> mlock(_mutex);
    while (_messages.empty()) {
      _cond.wait(mlock);
    }

    const auto item = _messages.front();
    _messages.pop();
    if (mlock.owns_lock())
      mlock.unlock();
    return item;
  }

  void Hub::pushMessage(const messaging::message_ptr&& item) {
    {
      std::unique_lock<std::mutex> mlock(_mutex);
      _messages.push(std::move(item));
      _cond.notify_one();
      mlock.unlock();
    }
  }

  void Hub::msgLoop() {
    while (_loopRunning.load(std::memory_order_acquire)) {
      const auto msg = popMessage();
      for (auto& out : _outputChannels)
        if ((nullptr != msg) && (msg->_originId != out->_id))
          msg >> *out;
    }
  }

  void Hub::activate() {
    if (_outputChannels.empty())
      throw std::logic_error("Can't run with no outputs");

    std::vector<std::future<void> > activators;

    for (auto& out : _outputChannels)
      activators.push_back(out->activate());

    bool ready = true;
    try {
      do {
        ready = true;
        for (auto& ch : activators) {
          ready &= ch.valid();
          if (ch.valid())
            ch.get();
        }
      } while (!ready);
      activators.clear();
    } catch(const channeling::channel_error& ce) {
      ERROR << "Can't run channel " << ce._name << ":" << ce.what();
      throw std::runtime_error("Failed to activate hub " + _name +
                               " due to channel " + ce._name);
    }

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

    _loopRunning.store(true, std::memory_order_release);
#ifdef HANDLERS
    _msgLoop = std::make_unique<std::thread>(std::thread(&Hub::msgLoopPipelined, this));
#else
    _msgLoop = std::make_unique<std::thread>(std::thread(&Hub::msgLoop, this));
#endif
  }

  void Hub::deactivate() {
    if (!_loopRunning.load(std::memory_order_acquire))
      return;
    _loopRunning.store(false, std::memory_order_release);
    _alive.store(false, std::memory_order_release);
    const auto msg = std::make_shared<const messaging::TextMessage>(messaging::system_user_id,
                                                                    std::make_shared<const messaging::User>(messaging::User("system")),
                                                                    MSG_EXITING);
    pushMessage(std::move(msg));
    _msgLoop->join();
  }

  void Hub::tick() {
    std::vector<std::future<void> > activators;

    for (auto& out : _outputChannels)
      activators.push_back(
        std::async(std::launch::async, [&out]()
      {
        out->tick();
      }));
    for (auto& in : _inputChannels)
      activators.push_back(
        std::async(std::launch::async, [&in]()
      {
        in->tick();
      }));
  }

#ifdef HANDLERS
  void Hub::msgLoopPipelined() {
    while (_loopRunning.load(std::memory_order_acquire)) {
      auto msg = popMessage();
      if (!_handlers.empty())
        for (auto& handler : _handlers)
          if (nullptr != msg) {
            msg = handler->processMessage(std::move(msg)).get();
          }
      for (auto& out : _outputChannels)
        if ((nullptr != msg) && (msg->_originId != out->_id))
          msg >> *out;
    }
  }

  void Hub::addHandler(handlerPtr hndl) {
    DEBUG << "Adding handler";
    _handlers.push_back(std::move(hndl));
  }
#endif
}
