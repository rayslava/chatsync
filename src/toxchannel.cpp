#include "toxchannel.hpp"
#include "messages.hpp"

namespace linux {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

namespace toxChannel {

  namespace util {
    /* Thanks to [0xd34df00d](https://github.com/0xd34df00d) for this bunch of functions */

#include <cassert>

    std::string hex2bin(std::string const& s) {
      assert(s.length() % 2 == 0);

      std::string sOut;
      sOut.reserve(s.length() / 2);

      std::string extract;
      for (std::string::const_iterator pos = s.begin(); pos < s.end(); pos += 2)
      {
        extract.assign(pos, pos + 2);
        sOut.push_back(std::stoi(extract, nullptr, 16));
      }
      return sOut;
    }

    template <size_t Size>
    std::string ToxId2HR (const uint8_t* address)
    {
      std::string result;
      auto toHexChar = [] (uint8_t num) -> char
                       {
                         return num >= 10 ? (num - 10 + 'A') : (num + '0');
                       };

      for (size_t i = 0; i < Size; ++i)
      {
        const auto num = address [i];
        result += toHexChar ((num & 0xf0) >> 4);
        result += toHexChar (num & 0xf);
      }
      return result;
    }

    template <size_t Size>
    std::string ToxId2HR (const std::array<uint8_t, Size>& address)
    {
      return ToxId2HR<Size>(address.data ());
    }
#include <string.h>
  }

  static Tox * toxInit(const std::string& datafile)
  {
    TOX_ERR_NEW tox_error;
    Tox* retval;
    struct Tox_Options options;
    tox_options_default(&options);

    try {
      const auto dataFileName = datafile.c_str();
      struct linux::stat sb;
      if ((stat(dataFileName, &sb) == -1))
        throw config::option_error("No such file");
      if ((sb.st_mode & S_IFMT) != S_IFREG)
        throw config::option_error("Not a file");
      const size_t filesize = sb.st_size;
      const auto toxData = std::make_unique<uint8_t[]>(filesize);
      const int toxfd = linux::open(dataFileName, O_RDONLY);
      int result = linux::read(toxfd, toxData.get(), filesize);
      if (result < 0)
        throw config::option_error("Error reading file");
      result = linux::close(toxfd);
      if (result < 0)
        throw config::option_error("Error closing file");
      if (result > 0)
        throw config::option_error("Tox data is encrypted");
      options.savedata_data = toxData.get();
      options.savedata_length = filesize;
      options.savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
      retval = tox_new(&options, &tox_error);
    } catch (config::option_error e) {
      std::cerr << "[DEBUG] Can't open tox data: " << e.what() << std::endl;
      options.savedata_data = nullptr;
      options.savedata_length = 0;
      retval = tox_new(&options, &tox_error);
    }
    switch (tox_error) {
    case TOX_ERR_NEW_OK:
      std::cerr << "[DEBUG] toxdata successfully loaded" << std::endl;
      break;
    case TOX_ERR_NEW_LOAD_BAD_FORMAT:
      std::cerr << "[DEBUG] toxdata file is damaged" << std::endl;
      break;
    default:
      std::cerr << "[DEBUG] tox_new error: " << tox_error << std::endl;
    }
    return retval;
  }

  ToxChannel::ToxChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _tox(toxInit(std::string(_config["datafile"]))),        /** TODO: make options handling */
    wasConnected(false)
  {}

  std::future<void> ToxChannel::activate() {
    return std::async(std::launch::async, [this]() {
      if (_active)
        return;
      _pipeRunning = true;
      toxStart();
      _thread = std::make_unique<std::thread>(std::thread(&ToxChannel::pollThread, this));
      _active = true;
    });
  }

  void ToxChannel::incoming(const messaging::message_ptr&& msg) {
    if (msg->type() == messaging::MessageType::Text) {
      const auto textmsg = messaging::TextMessage::fromMessage(msg);
      std::cerr << "[DEBUG] #tox " << _name << " " << textmsg->data() << std::endl;
      static uint8_t msg[TOX_MAX_MESSAGE_LENGTH];
      const auto len = snprintf(reinterpret_cast<char *>(msg), TOX_MAX_MESSAGE_LENGTH,
                                "[%s]: %s",
                                textmsg->user()->name().c_str(), textmsg->data().c_str());
      tox_group_message_send(_tox, 0, msg, len);
    }
  }

  void ToxChannel::pollThread() {
    std::cerr << "[DEBUG] Starting tox thread" << std::endl;
    while (_pipeRunning) {
      tox_iterate(_tox);
      auto wait = tox_iteration_interval(_tox);
      std::this_thread::sleep_for( std::chrono::milliseconds (wait));
    }
  }

  ToxChannel::~ToxChannel() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
    try {
      const auto dataFileName = std::string(_config["datafile"]).c_str();
      const size_t filesize = tox_get_savedata_size(_tox);
      const auto toxData = std::make_unique<uint8_t[]>(filesize);
      tox_get_savedata(_tox, toxData.get());
      const int toxfd = linux::open(dataFileName, O_WRONLY | O_CREAT, 0644);
      int result = linux::write(toxfd, toxData.get(), filesize);
      if (result < 0)
        throw config::option_error("Error writing file");
      std::cerr << "[DEBUG] #tox " << _name << " Successfully saved " << result << " bytes of tox data" << std::endl;
      result = linux::close(toxfd);
      if (result < 0)
        throw config::option_error("Error closing file");
    } catch (config::option_error e) {
      std::cerr << "[DEBUG] Can't save tox data: " << e.what() << std::endl;
    }
    tox_kill(_tox);
  }

  void ToxChannel::friendRequestCallback(Tox* tox, const uint8_t* public_key, const uint8_t* data, size_t length, void* userdata) {
    TOX_ERR_FRIEND_ADD friend_error;
    const auto channel = static_cast<ToxChannel *>(userdata);
    const auto friendNum = tox_friend_add_norequest(tox, public_key, &friend_error);     /** TODO: check friend_error */
    std::cerr << "[DEBUG] tox id with data" << data << " of " << length << " bytes " << util::ToxId2HR<TOX_ADDRESS_SIZE>(public_key) << " wants to be your friend. Added with #" << friendNum << std::endl;
  }

  void ToxChannel::messageCallback(Tox* tox, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t* message, size_t length, void* userdata) {
    const auto channel = static_cast<ToxChannel *>(userdata);
    const auto buffer = std::make_unique<char *>(new char[length + 1]);
    snprintf(*buffer, length + 1, "%s", message);
    switch (type) {
    case TOX_MESSAGE_TYPE_NORMAL:
      std::cerr << "[DEBUG] Message from friend #" << friendnumber << "> " << *buffer << std::endl;
      if (util::strncmp(cmd_invite, *buffer, length) == 0)
        tox_invite_friend(tox, friendnumber, 0);
      break;
    case TOX_MESSAGE_TYPE_ACTION:
      std::cerr << "[DEBUG] Action from friend #" << friendnumber << "> " << *buffer << std::endl;
      break;
    default:
      std::cerr << "[DEBUG] Unknown message type from " << friendnumber << "> " << *buffer << std::endl;
    }
  }

  void ToxChannel::groupMessageCallback(Tox* tox, int32_t groupnumber, int32_t peernumber, const uint8_t* message, uint16_t length, void* userdata) {
    const auto channel = static_cast<ToxChannel *>(userdata);

    const auto nameBuffer = std::make_unique<uint8_t *>(new uint8_t[TOX_MAX_NAME_LENGTH]);
    const auto nameLen = tox_group_peername(tox, groupnumber, peernumber, *nameBuffer);
    const auto name = std::make_unique<char *>(new char[nameLen + 2]);
    snprintf(*name, nameLen + 1, "%s", *nameBuffer);

    const auto msg = std::make_unique<char *>(new char[length + 2]);
    snprintf(*msg,  length + 1,	 "%s", message);

    if (std::string(*name) != std::string(channel->_config.get("nickname", defaultBotName))) {
      const auto newMessage = std::make_shared<const messaging::TextMessage>(channel->_id,
                                                                             std::make_shared<const messaging::User>(messaging::User(*name)),
                                                                             *msg);
      std::cerr << "[DEBUG] tox Group msg " << newMessage->user()->name() << "> " << newMessage->data() << std::endl;
      channel->_hub->newMessage(std::move(newMessage));
    }
  }

  const messaging::message_ptr ToxChannel::parse(const char* line) const
  {
    const std::string s(line);
    const auto name = s.substr(0, s.find(":"));
    const auto text = s.substr(s.find(":"), s.length());

    const auto msg = std::make_shared<const messaging::TextMessage>(_id,
                                                                    std::make_shared<const messaging::User>(messaging::User(name.c_str())),
                                                                    text.c_str());
    return msg;
  }

  int ToxChannel::toxStart() {
    TOX_ERR_SET_INFO result;
    TOX_ERR_BOOTSTRAP bootstrap_result;

    //std::unique_ptr<uint8_t[]> pubKey(new uint8_t[TOX_CLIENT_ID_SIZE]);
    tox_callback_friend_request(_tox, friendRequestCallback, this);
    tox_callback_friend_message(_tox, messageCallback, this);
    tox_callback_group_message(_tox, groupMessageCallback, this);

    const std::string nick = _config.get("nickname", defaultBotName);
    const uint8_t* nickData = reinterpret_cast<const uint8_t *>(nick.c_str());
    tox_self_set_name(_tox, nickData, nick.length(), &result);
    if (result)
      throw channeling::activate_error(ERR_TOX_INIT + "(tox_set_name)");

    const std::string statusMsg = _config.get("status_message", defaultStatusMessage);
    const uint8_t* statusData = reinterpret_cast<const uint8_t *>(statusMsg.c_str());

    tox_self_set_status_message(_tox, statusData, statusMsg.length(), &result);
    if (result)
      throw channeling::activate_error(ERR_TOX_INIT + "(tox_set_status_message)");

    tox_self_set_status(_tox, defaultBotStatus);

    if (tox_self_get_connection_status(_tox) == TOX_CONNECTION_NONE)
      tox_bootstrap(_tox, defaultBootstrapAddress, defaultBootstrapPort, reinterpret_cast<const uint8_t *>(util::hex2bin(defaultBootstrapKey).c_str()), &bootstrap_result);

    if (bootstrap_result)
      throw channeling::activate_error(ERR_TOX_INIT + ": Can't decode bootstrapping ip");

    std::cerr << "[DEBUG] Bootstrapping" << std::endl;
    /* TODO: Make timeout exception handling */

    while (!wasConnected) {
      TOX_CONNECTION status;
      tox_iterate(_tox);
      auto wait = tox_iteration_interval(_tox);
      if (!wasConnected && (TOX_CONNECTION_NONE != tox_self_get_connection_status(_tox))) {
        std::array<uint8_t, TOX_ADDRESS_SIZE> address;
        tox_self_get_address (_tox, address.data ());
        std::cerr << "[DEBUG] Tox is connected with id " << util::ToxId2HR (address) << std::endl
        ;
        wasConnected = true;
      }
      std::this_thread::sleep_for( std::chrono::milliseconds (wait));
    }

    tox_add_groupchat (_tox);

    return 0;
  }
}
