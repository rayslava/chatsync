#include "channel.hpp"
#include <stdexcept>
#include <typeinfo>
#include <sstream>

#include "messages.hpp"

namespace Channeling {
    Channel::Channel(std::string const &name, ChannelDirection const &direction, Hub::Hub* const hub):
    _thread(nullptr),
    _pipeRunning(ATOMIC_FLAG_INIT),
    _fd(-1),
    _name(name),
    _direction(direction),
    _hub(hub)
    {
        _hub->addChannel(this);
    }

    std::string const & Channel::name() const {
        return _name;
    }

    Channel& operator>>(const std::string &msg,  Channel& channel) {
	if (channel.direction() == Channeling::ChannelDirection::Input)
	    throw std::logic_error("Can't write data to input channel " + channel.name());
	channel.parse(msg);
        return channel;
    }

    void Channel::parseConfig(std::vector<std::string> const &lines) {
        if (lines.empty()) {
            throw std::runtime_error("Channel config is empty");
        }
        throw std::runtime_error(ERR_NOT_IMPL);
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

  
    Channel* ChannelFactory::create(const std::string& classname) {
	std::map<std::string, ChannelCreator*>::iterator i;
	i = get_table().find(classname);

	if (i != get_table().end())
	    return i->second->create();
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
	// Initialize time out struct for select()
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	// Implement the receiver loop
	while (_pipeRunning) {
	    // Initialize the set
	    FD_ZERO(&readset);
	    FD_SET(readFd, &readset);
	    // Now, check for readability
	    err = select(readFd+1, &readset, NULL, NULL, &tv);
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
		_hub->newMessage(std::string(line));
		delete[] line;
	    }
	}
    }
}
