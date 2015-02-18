#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>
#include <utility>

#include "messages.hpp"

namespace Channeling {
  Channel::Channel(Hub::Hub* const hub, const std::string&& config):
    _thread(nullptr),
    _pipeRunning(ATOMIC_FLAG_INIT),
    _fd(-1),
    _config(std::move(config)),
    _name(_config["name"]),
    _direction(_config["direction"]),
    _hub(hub)
  {
    std::cout << _name << std::endl;
    _hub->addChannel(this);
  }

  std::string const & Channel::name() const {
    return _name;
  }
 
  Channel& operator>> (const message_ptr msg,  Channel& channel) {
    if (channel.direction() == Channeling::ChannelDirection::Input)
      throw std::logic_error("Can't write data to input channel " + channel.name());
    std::cerr << "[DEBUG] Incoming message " << msg->data() << std::endl;    
    channel.incoming(std::move(msg));
    return channel;
  }

  void Channel::startPolling() {
    if (_fd < 0)
      throw std::runtime_error(ERR_FD);
    _pipeRunning = true;
    _thread = std::make_unique<std::thread> (std::thread(&Channel::pollThread, this));
  }

  void Channel::stopPolling() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
  }

  
  Channel* ChannelFactory::create(const std::string& classname, Hub::Hub* const hub, const std::string&& config) {
    std::map<std::string, ChannelCreator*>::iterator i;
    i = get_table().find(classname);

    if (i != get_table().end())
      return i->second->create(hub, std::move(config));
    else
      return (Channel*)NULL;
  }

  void ChannelFactory::registerClass(const std::string& classname, ChannelCreator* creator) {
    get_table()[classname] = creator;
  }

  std::map<std::string, ChannelCreator*>& ChannelFactory::get_table() {
    static std::map<std::string, ChannelCreator*> table;
    return table;
  }


  // have the creator's constructor do the registration
  ChannelCreator::ChannelCreator(const std::string& classname) {
    ChannelFactory::registerClass(classname, this);
  }
  
  /* OS interaction code begins here */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stropts.h>

  void Channel::pollThread() {
    // Form descriptor
    const int readFd = _fd;
    fd_set readset;
    int err = 0;
    // Implement the receiver loop
    while (_pipeRunning) {
      // Initialize time out struct for select()
      struct timeval tv;
      tv.tv_sec = _config.get("poll_time", "5");
      tv.tv_usec = 0;
      // Initialize the set
      FD_ZERO(&readset);
      FD_SET(readFd, &readset);
      // Now, check for readability
      err = select(readFd+1, &readset, NULL, NULL, &tv);
      std::cerr << "[DEBUG] Selected fd " << err << std::endl;
      if (err > 0 && FD_ISSET(readFd, &readset)) {
	// Clear flags
	FD_CLR(readFd, &readset);
	// Check available size
	int bytes;
	ioctl(readFd, FIONREAD, &bytes);
	auto line = new char[bytes + 1];
	line[bytes] = 0;
	// Do a simple read on data
	err = read(readFd, line, bytes);
	if (err != bytes)
	  throw std::runtime_error(ERR_SOCK_READ);
	// Dump read data
	const auto msg = std::make_shared<const messaging::Message>(
	    std::move(std::make_shared<const messaging::User>(messaging::User(_name.c_str()))),
	    line);
	_hub->newMessage(std::move(msg));
	delete[] line;
      }
    }
  }
}
