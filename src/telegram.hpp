#pragma once
#include "channel.hpp"

namespace telegram {
  class TgChannel: public channeling::Channel {
    std::string _hash;                              /**< Bot access hash */
    std::future<void> activate() override;
    static const channeling::ChannelCreatorImpl<TgChannel> creator;

    const messaging::message_ptr parse(const char* line) const override; /**< This would be useful if telegram read something from socket */
    void pollThread() override;     /**< Thread for telegram infinite loop */
  public:
    explicit TgChannel(Hub::Hub*, const std::string&);
    ~TgChannel();

    std::string type() const override { return "telegram"; };
  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };

  const channeling::ChannelCreatorImpl<TgChannel> TgChannel::creator("telegram");
}
