#pragma once
#include <chrono>
#include "channel.hpp"
#include "hub.hpp"

namespace ircChannel {

  constexpr size_t irc_message_max = 256;
  constexpr std::chrono::duration<double> max_timeout(5.0);
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


    mutable std::mutex _pong_time_mutex;                 /**< Lock for _last_pong_time */
    mutable std::chrono::time_point<std::chrono::high_resolution_clock> _last_pong_time; /**< Last pong received from server */
    mutable std::atomic_bool _connection_issue /**< The connection issue has been detected, check is ongoing*/;

    /**
     * Sends PASS, NICK and USER commands to register irc connection
     */
    void registerConnection();
    /**
     * Sends PING message to server
     */
    void ping();
    /**
     * Reacts to PONG message from server
     */
    void pong() const;
    /**
     * Checks if max_timeout is reached and calls ping()
     *
     * @todo Add heuristics of timeout
     */
    void checkTimeout();
    std::future<void> activate() override;
    const messaging::message_ptr parse(const char* line) const override;

    static const channeling::ChannelCreatorImpl<IrcChannel> creator;
    const messaging::message_ptr parseImpl(const std::string& toParse) const;
  public:
    explicit IrcChannel(Hub::Hub* hub, const std::string& config);
    ~IrcChannel();

    std::string type() const override {return "irc"; };

  protected:
    /**
     * If during max_timeout*5 we got no PONG we try to reconnect
     */
    void tick() override;

    void incoming(const messaging::message_ptr&& msg) override;
  };
  const channeling::ChannelCreatorImpl<IrcChannel> IrcChannel::creator("irc");
}
