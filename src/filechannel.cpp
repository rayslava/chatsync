#include "filechannel.hpp"
#include "hub.hpp"
#include <sstream>

namespace fileChannel {
    FileChannel::FileChannel(const std::string &name, Channeling::ChannelDirection const &direction, Hub::Hub* hub):
    Channeling::Channel(name, direction, hub),
    _outfile("file.lo")
    {
    }

    void FileChannel::parse(const std::string &l) const {
        std::cerr << "[DEBUG] Parsing line " << l << " inside " << _name << std::endl;
    }
}
