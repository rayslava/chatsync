#pragma once
#include "channel.hpp"
#include "http.hpp"
#include "rapidjson/document.h"

namespace telegram {
  const static std::string telegram_api_srv = "api.telegram.org";
  constexpr int telegram_api_port = 443;
  constexpr int max_reconnects = 5;

  namespace api {
    enum class ChatType {
      Private,
      Group,
      Supergroup,
      Channel
    };

    struct Chat {
      const std::int64_t id;
      const ChatType type {};
      const std::string title {};
      const bool all_are_admins {};
      const std::string username {};
      const std::string first_name {};
      const std::string last_name {};
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

  struct telegram_error: std::runtime_error {
    telegram_error(std::string const& message) :
      std::runtime_error(message) {};
  };

  struct parse_error: telegram_error {
    const int code;
    parse_error(int c, std::string const& message) :
      telegram_error(message),
      code(c)
    {};
  };

  class TgChannel: public channeling::Channel {
    const std::string _botid;                             /**< Bot id */
    const std::string _hash;                              /**< Bot access hash */
    const std::string _server;                            /**< Server address to connect to */
    const std::string _endpoint;                          /**< API endpoint to compute URI from */
    const int64_t _chat;                                  /**< Chat id to use */
    mutable std::uint64_t _last_update_id;                /**< Last processed update id */
    int64_t _reconnect_attempt;                           /**< Number of server http errors */
    std::future<void> activate() override;
    static const channeling::ChannelCreatorImpl<TgChannel> creator;

    const messaging::message_ptr parse(const char* line) const override; /**< This would be useful if telegram read something from socket */
    void pollThread() override;                           /**< Thread for telegram infinite loop */

    /**
     * Perform a POST request to \c url with \c body
     */
    void apiRequest(const std::string& uri, const std::string& body);
    const messaging::message_ptr buildTextMessage(const api::Message& msg) const;

    std::unique_ptr<http::HTTPResponse>
    httpRequest(const std::string& srv, const http::HTTPRequest& req) const;

    /**
     * Check if the message is from chat which should be processed
     */
    bool messageMatch(const api::Message& msg) const;
  public:
    explicit TgChannel(Hub::Hub *, const std::string&);
    ~TgChannel();

    std::string type() const override { return "telegram"; };
  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };
}
