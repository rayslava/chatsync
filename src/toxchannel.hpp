#pragma once
#include "channel.hpp"
#include <tox/tox.h>

namespace toxChannel {

  constexpr auto defaultBootstrapAddress = "23.226.230.47";   /**< Tox bootstrap address @todo enable grabbing from gist */
  constexpr auto defaultBootstrapPort = "33445";   /**< Tox bootstrap server port */
  constexpr auto defaultBootstrapKey = "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074"; /**< Tox default bootstrap key */
  //  constexpr auto defaultBootstrapKey = "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67";

  constexpr auto cmd_invite = "invite";
  constexpr auto cmd_conference = "conference";


  constexpr auto defaultBotName = "chatsyncbot";   /**< Default bot nickname */
  constexpr auto defaultStatusMessage = "Online";   /**< Default tox status message */
  constexpr auto defaultBotStatus = TOX_USER_STATUS_NONE;   /**< Default bot name */
  /**
   * Tox channel
   */
  class ToxChannel: public channeling::Channel {
    Tox * const _tox;        /**< Main tox structure */
    int toxStart();
    std::future<void> activate() override;
    bool wasConnected;

    static void friendRequestCallback(Tox* tox, const uint8_t* public_key, const uint8_t* data, size_t length, void* userdata);
    static void messageCallback(Tox* tox, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t* message, size_t length, void* userdata);
    static void groupMessageCallback(Tox* tox, int32_t groupnumber, int32_t peernumber, const uint8_t* message, uint16_t length, void* userdata);
    static const channeling::ChannelCreatorImpl<ToxChannel> creator;

    const messaging::message_ptr parse(const char* line) const override;
    void pollThread() override;     /**< Thread for tox infinite loop */
  public:
    explicit ToxChannel(Hub::Hub* hub, const std::string& config);
    ~ToxChannel();

    std::string type() const override { return "tox"; };

  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };
  const channeling::ChannelCreatorImpl<ToxChannel> ToxChannel::creator("tox");
}
