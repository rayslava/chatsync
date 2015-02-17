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
	*
	* @throws Channeling::activate_error when can't open file
	*/
	int openPipe(const std::string& filename);

	std::future<void> activate();
	static const Channeling::ChannelCreatorImpl<FileChannel> creator;
    public:
	explicit FileChannel(Hub::Hub* hub, const std::string&& config);
        ~FileChannel();

	std::string type() const {return "file";};

    protected:
        void parse(const std::string& l);
    };
    const Channeling::ChannelCreatorImpl<FileChannel> FileChannel::creator("file");
}
