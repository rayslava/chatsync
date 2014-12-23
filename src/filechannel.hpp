#pragma once
#include "channel.hpp"
#include <fstream>

namespace fileChannel {

    /**
    * File channel
    *
    * May be useful for logging
    */
    class FileChannel: public Channeling::Channel {
	std::fstream _file;                         /**< File stream to save log */
	std::unique_ptr<std::thread> _thread;       /**< Pointer to reader thread in case of input channel */
	std::atomic_bool _pipeRunning;              /**< Pipe reading thread is running */

	/**
	* Thread which polls on input pipe and sends messages in case of input channel direction
	* @param fd Opened input pipe descriptor
	*/
	void pollThread(int fd);
    public:
        explicit FileChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub);
        ~FileChannel();

    protected:
        void parse(const std::string& l);
    };
}
