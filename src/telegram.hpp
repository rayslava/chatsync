#pragma once
#include "channel.hpp"
#include "rapidjson/document.h"

namespace telegram {
  const static std::string telegram_api_srv = "https://api.telegram.org";

  class telegram_error: std::runtime_error {
  public:
    telegram_error(std::string const& message) :
      std::runtime_error(message) {};
  };

  class TgChannel: public channeling::Channel {
    const std::string _botid;                             /**< Bot id */
    const std::string _hash;                              /**< Bot access hash */
    const std::string _endpoint;                          /**< API endpoint to compute URI from */
    std::future<void> activate() override;
    static const channeling::ChannelCreatorImpl<TgChannel> creator;

    const messaging::message_ptr parse(const char* line) const override; /**< This would be useful if telegram read something from socket */
    void pollThread() override;     /**< Thread for telegram infinite loop */

    rapidjson::Document apiRequest(const std::string& uri);
  public:
    explicit TgChannel(Hub::Hub *, const std::string&);
    ~TgChannel();

    std::string type() const override { return "telegram"; };
  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };

  const channeling::ChannelCreatorImpl<TgChannel> TgChannel::creator("telegram");
}
