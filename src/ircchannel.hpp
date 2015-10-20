#pragma once
#include <chrono>
#include "channel.hpp"
#include "hub.hpp"

namespace ircChannel {

  constexpr size_t irc_message_max = 256;
  constexpr std::chrono::duration<double> max_timeout(2.0);
  /**
   * IRC connection channel
   *
   * Capable of connection an IRC server, joining one single channel and message transmission/receiving.
   * Responds to PING with PONG to maintain connection.
   */
  class IrcChannel: public channeling::Channel {

    const std::string _server;                           /**< Server address */
    const uint32_t _port;                                /**< Connection port */
    const std::string _channel;                          /**< Channel name (starting with #) */
    std::chrono::time_point<std::chrono::high_resolution_clock> _ping_time; /**< Ping to server in microseconds */

    /**
     * Sends PASS, NICK and USER commands to register irc connection
     */
    int registerConnection();
    /**
     * Sends PING message to server
     */
    void ping();
    /**
     * Checks if max_timeout is reached and calls ping()
     *
     * @todo Add heuristics of timeout
     */
    void checkTimeout();
    std::future<void> activate() override;
    const messaging::message_ptr parse(const char* line) const override;

    static const channeling::ChannelCreatorImpl<IrcChannel> creator;
  public:
    explicit IrcChannel(Hub::Hub* hub, const std::string& config);
    ~IrcChannel();

    std::string type() const override {return "irc"; };

  protected:
    void incoming(const messaging::message_ptr&& msg) override;
  };
  const channeling::ChannelCreatorImpl<IrcChannel> IrcChannel::creator("irc");
}
