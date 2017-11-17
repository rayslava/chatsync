#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>
#include <utility>
#include <memory>

#include "net.hpp"
#include "messages.hpp"
#include "logging.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace channeling {
  std::atomic_int ChannelFactory::id {ATOMIC_FLAG_INIT};

  Channel::Channel(Hub::Hub * const hub, const std::string& config) :
    _reconnect_attempt(0),
    _active(ATOMIC_FLAG_INIT),
    _thread(nullptr),
    _pipeRunning(ATOMIC_FLAG_INIT),
    _fd(-1),
    _config(config),
    _name(_config["name"]),
    _direction(_config["direction"]),
    _hub(hub),
    _id(ChannelFactory::nextId())
  {
    DEBUG << _name << " : " << _id;
    _hub->addChannel(this);
  }

  Channel::~Channel() {
    DEBUG << "Destroying " << _name << " : " << _id;
  }

  std::string const& Channel::name() const {
    return _name;
  }

  Channel& operator>> (const message_ptr msg, Channel& channel) {
    if (channel.direction() == channeling::ChannelDirection::Input)
      throw std::logic_error("Can't write data to input channel " + channel.name());
    const auto message = messaging::TextMessage::fromMessage(msg);
    DEBUG << "Incoming message " << message->data();
    std::async(std::launch::async, [&channel, msg = std::move(msg)]()
    {
      channel.incoming(std::move(msg));
    });
    return channel;
  }

  void Channel::startPolling() {
    if (_fd < 0) {
      DEBUG << "Channel " << _name << " fd < 0, reconnecting";
      reconnect();
      return;
    }
    _pipeRunning.store(true, std::memory_order_release);
    _thread = std::make_unique<std::thread>(std::thread(&Channel::pollThread, this));
  }

  void Channel::stopPolling() {
    if (_thread) {
      DEBUG << "Thread " << _name << " stops polling.";
      _active.store(false, std::memory_order_release);
      _pipeRunning.store(false, std::memory_order_release);
      _thread->join();
      _thread.reset();
      DEBUG << "Thread " << _name << " joined.";
    }
  }

  void Channel::reconnect() {
    if (!_hub->alive()) {
      DEBUG << "Hub is dead. Aborting reconnect";
      return;
    }
    DEBUG << "Channel trying to reconnect after stop. @" << this;

    const unsigned int timeout = _config.get("reconnect_timeout", "5000");
    const unsigned int max_repeats = _config.get("max_reconnects", "3");
    if (_reconnect_attempt > max_repeats)
      throw connection_error(_name, "Maximum number of reconnections reached");
    stopPolling();
    disconnect();

    DEBUG << "Channel " << name() << ": file descriptor closed";

    activate().wait();
    if (_pipeRunning.load(std::memory_order_acquire)) {
      // Successfully reconnected
      _reconnect_attempt = 0;
      return;
    }

    ++_reconnect_attempt;
    std::thread([this, timeout]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeout * _reconnect_attempt));
      reconnect();
    }).detach();
  }

  Channel * ChannelFactory::create(const std::string& classname, Hub::Hub * const hub, const std::string& config) {
    std::map<std::string, ChannelCreator *>::iterator i;
    i = get_table().find(classname);

    if (i != get_table().end())
      return i->second->create(hub, config);
    else
      return (Channel *) NULL;
  }

  std::map<std::string, ChannelCreator *>& ChannelFactory::get_table() {
    static std::map<std::string, ChannelCreator *> table;
    return table;
  }

  void ChannelFactory::registerClass(const std::string& classname, ChannelCreator* creator) {
    get_table()[classname] = creator;
  }

  ChannelCreator::ChannelCreator(const std::string& classname) {
    ChannelFactory::registerClass(classname, this);
  }

  void Channel::pollThread() {
    const int readFd = _fd;
    fd_set readset;
    int err = 0;
    const unsigned int select_timeout = _config.get("poll_time", "5");
    // Implement the receiver loop
    while (_pipeRunning.load(std::memory_order_acquire)) {
      // Initialize time out struct for select()
      struct timeval tv;
      tv.tv_sec = select_timeout;
      tv.tv_usec = 0;
      // Initialize the set
      FD_ZERO(&readset);
      FD_SET(readFd, &readset);
      // Now, check for readability
      errno = 0;
      err = select(readFd + 1, &readset, NULL, NULL, &tv);
      if (!_hub->alive()) return;
      if (_pipeRunning.load(std::memory_order_acquire) &&
          (err < 0 || fcntl(readFd, F_GETFD) == -1 || errno == EBADF)) {
        if (!_hub->alive()) return;
        DEBUG << "select() failed. Reconnecting channel " << name();
        std::thread([this]() {
          reconnect();
        }).detach();
        return;
      }
      if (err > 0 && FD_ISSET(readFd, &readset)) {
        // Clear flags
        FD_CLR(readFd, &readset);
        // Check available size
        int bytes;
        err = ioctl(readFd, FIONREAD, &bytes);
        if (err < 0 || bytes == 0) {
          if (!_hub->alive()) return;
          std::thread([this]() {
            reconnect();
          }).detach();
          return;
        }
        auto line = new char[bytes + 1];
        line[bytes] = 0;
        // Do a simple read on data
        err = read(readFd, line, bytes);
        if (err != bytes)
          throw std::runtime_error(ERR_SOCK_READ);
        // Parse received data
        if (direction() == ChannelDirection::Input || direction() == ChannelDirection::Bidirectional)
          _hub->newMessage(parse(line));
        delete[] line;
      }
    }
  }

  int Channel::connect(const std::string& hostname, const uint32_t port) const {
    return networking::tcp_connect(hostname + ":" + std::to_string(port));
  }

  int Channel::send(const uint32_t fd, const std::string& msg) const {
    const int n = write(fd, msg.c_str(), msg.length());
    if (n < 0)
      throw std::runtime_error(ERR_SOCK_WRITE);

    return n;
  }

  int Channel::disconnect(const uint32_t fd) const {
    if (_fd > 0)
      close(fd);
    return 0;
  }
}
