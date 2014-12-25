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

	/**
	* Opens input pipe
	*
	* @param filename Pipe name with full path (if needed)
	* @retval int File descriptor
	*/
	int openPipe(const std::string& filename);

    public:
        explicit FileChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub);
        ~FileChannel();

    protected:
        void parse(const std::string& l);
    };
}
