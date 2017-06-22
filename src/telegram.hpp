#pragma once
#include "channel.hpp"
#include "rapidjson/document.h"

namespace telegram {
  const static std::string telegram_api_srv = "api.telegram.org:443";

  namespace api {
    enum class Chat_type {
      Private,
      Group,
      Supergroup,
      Channel
    };

    struct Chat {
      const std::int64_t id;
      const Chat_type type {};
      const std::string title {};
      const std::string username {};
      const std::string first_name {};
      const std::string last_name {};
      const bool all_are_admins {};
    };

    struct User {
      const std::int64_t id;
      const std::string first_name {};
      const std::string last_name {};
      const std::string lang {};
    };

    struct Message {
      const std::int64_t id;
      const std::int64_t date;
      const std::string text {};
      const User from {};
      const Chat chat {};
    };
  }

  /*
     {"ok":true,"result":[{"update_id":544181547,
     "message":{"message_id":19,"from":{"id":336435018,"first_name":"ray","language_code":"en-RU"},"chat":{"id":336435018,"first_name":"ray","type":"private"},"date":1498049744,"text":"\u0429"}}]} */

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
    const messaging::message_ptr buildTextMessage(const api::Message& msg) const;
  public:
    explicit TgChannel(Hub::Hub *, const std::string&);
    ~TgChannel();

    std::string type() const override { return "telegram"; };
  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };

  const channeling::ChannelCreatorImpl<TgChannel> TgChannel::creator("telegram");
}
