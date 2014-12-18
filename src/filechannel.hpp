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
	std::fstream _outfile;                    /**< File stream to save log */
    public:
        explicit FileChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub);
        ~FileChannel() {};

    protected:
        void parse(const std::string& l) const;
    };
}
