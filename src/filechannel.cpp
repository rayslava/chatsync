#include "filechannel.hpp"
#include "hub.hpp"
#include <sstream>

namespace fileChannel {

    FileChannel::FileChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub):
	Channeling::Channel(name, direction, hub),
	_thread(nullptr),
	_pipeRunning(ATOMIC_FLAG_INIT)
    {
	if (direction == Channeling::ChannelDirection::Input) {
	    const int pipeFd = openPipe("input");
	    _pipeRunning = true;
	    _thread = std::make_unique<std::thread> (std::thread(&FileChannel::pollThread, this, pipeFd));
	} else if (direction == Channeling::ChannelDirection::Output) {
	    _file.clear();
	    _file.open("output", std::ios::out);
	}
    }

    void FileChannel::parse(const std::string &l) {
	_file << l;
	std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }

    FileChannel::~FileChannel() {
	if (_file.is_open())
	    _file.close();

	if (_thread) {
	    _pipeRunning = false;
	    _thread->join();
	}
    }

/* OS interaction code begins here */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stropts.h>

    int FileChannel::openPipe(const std::string &filename) {
	int ret_val = mkfifo(filename.c_str(), 0666);
	if ((ret_val == -1) && (errno != EEXIST)) {
	    perror("Error creating the named pipe");
	}
	// Open both ends within this process in on-blocking mode
	// Must do like this otherwise open call will wait
	// till other end of pipe is opened by another process
	return open(filename.c_str(), O_RDONLY|O_NONBLOCK);
    }

    void FileChannel::pollThread (const int fd) {
	// Form descriptor
	const int readFd = fd;
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
		read(readFd, line, bytes);
		// Dump read data
		_hub->newMessage(std::string(line));
		delete line;
	    }
	}
    }
}
