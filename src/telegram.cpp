#include "telegram.hpp"

namespace telegram {
  TgChannel::TgChannel(Hub::Hub* hub, const std::string& config):
    channeling::Channel(hub, config),
    _hash(_config["hash"])
  {
  }

  void TgChannel::pollThread() {
    DEBUG << "Starting telegram thread";
    while (_pipeRunning) {
      std::this_thread::sleep_for( std::chrono::milliseconds (100));
    }
  }

  std::future<void> TgChannel::activate() {
    return std::async(std::launch::async, [this]() {
	if (_active)
	  return;
	_pipeRunning = true;
	_thread = std::make_unique<std::thread>(std::thread(&TgChannel::pollThread, this));
	_active = true;
      });
  }

  TgChannel::~TgChannel() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
  }

  void TgChannel::incoming(const messaging::message_ptr&& msg) {
  }

  const messaging::message_ptr TgChannel::parse(const char* line) const
  {
    const std::string s(line);
    const auto name = s.substr(0, s.find(":"));
    const auto text = s.substr(s.find(":"), s.length());

    const auto msg = std::make_shared<const messaging::TextMessage>(_id,
                                                                    std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                    text.c_str());
    return msg;
  }

}
