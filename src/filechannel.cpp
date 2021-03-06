#include "filechannel.hpp"
#include "hub.hpp"
#include "messages.hpp"
#include "logging.hpp"

namespace fileChannel {
  const channeling::ChannelCreatorImpl<FileChannel> FileChannel::creator("file");

  FileChannel::FileChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config)
  {}

  std::future<void> FileChannel::activate() {
    return std::async(std::launch::async,
                      [this]() {
      if (_direction == channeling::ChannelDirection::Input) {
        _fd = openPipe("input");
        startPolling();
      } else if (_direction == channeling::ChannelDirection::Output) {
        _file.clear();
        _file.open("output", std::ios::out);
      };
    });
  }

  void FileChannel::incoming(const messaging::message_ptr&& msg) {
    DEBUG << "#file " << _name << " incoming message: ";
    if (msg->type() == messaging::MessageType::Text) {
      const auto textmsg = messaging::TextMessage::fromMessage(msg);
      _file << textmsg->user()->name() << ": " << textmsg->data() << std::endl;
      DEBUG << "#file " << _name << " " << textmsg->data();
    } else if (msg->type() == messaging::MessageType::Action) {
      const auto actionmsg = messaging::ActionMessage::fromMessage(msg);
      _file << actionmsg->user()->name() << "[ACTION]: " << actionmsg->data() << std::endl;
      DEBUG << "#file " << _name << " performes an action: " << actionmsg->data();
    } else {
      throw std::runtime_error("Unknown message type");
    }
  }

  const messaging::message_ptr FileChannel::parse(const char* line) const
  {
    const auto msg = std::make_shared<const messaging::TextMessage>(_id,
                                                                    std::make_shared<const messaging::User>(messaging::User("file:" + _name)),
                                                                    line);
    return msg;
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

  int FileChannel::openPipe(const std::string& filename) {
    int ret_val = mkfifo(filename.c_str(), 0666);
    if ((ret_val == -1) && (errno != EEXIST))
      throw channeling::activate_error(_name, ERR_FILE_OPEN);
    /* Open both ends within this process in on-blocking mode
       Must do like this otherwise open call will wait
       till other end of pipe is opened by another process */
    return open(filename.c_str(), O_RDONLY | O_NONBLOCK);
  }
}
