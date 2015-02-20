#include "filechannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include <sstream>

namespace fileChannel {

    FileChannel::FileChannel(Hub::Hub* hub, const std::string&& config):
	Channeling::Channel(hub, std::move(config))
    {
    }

    std::future<void> FileChannel::activate() {
	return std::async(std::launch::async,
			  [this]() {
			      if (_direction == Channeling::ChannelDirection::Input) {
				  _fd = openPipe("input");
				  startPolling();
			      } else if (_direction == Channeling::ChannelDirection::Output) {
				  _file.clear();
				  _file.open("output", std::ios::out);
			      };
			  });}

    void FileChannel::incoming(const std::shared_ptr<const messaging::Message>&& msg) {
	_file << msg->user()->name() << ": " << msg->data();
        std::cerr << "[DEBUG] #file " << _name << " " << msg->data() << std::endl;
    }

    FileChannel::~FileChannel() {
        stopPolling();
        if (_file.is_open()) {
            _file.flush();
            _file.close();
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
        if ((ret_val == -1) && (errno != EEXIST))
            throw Channeling::activate_error(ERR_FILE_OPEN);
            /* Open both ends within this process in on-blocking mode
               Must do like this otherwise open call will wait
               till other end of pipe is opened by another process */
            return open(filename.c_str(), O_RDONLY|O_NONBLOCK);
        }
}
