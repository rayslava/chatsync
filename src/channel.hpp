#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <future>

#include "message.hpp"
#include "hub.hpp"
#include "config.hpp"

namespace channeling {
  using namespace messaging;
  /**
   * Thrown during Channel::activate in case of activation problems
   */
  class activate_error: public std::runtime_error {
  public:
    activate_error(std::string const& message) : std::runtime_error(message) {};
  };

  /**
   * A channel to connect to input or output
   *
   * The deriving class *must* override:
   *   void void incoming(const message_ptr msg) = 0; --- for data input in case of output channel
   *   void activate(); --- to implement specific activities for channel work starting, _active should be set
   *
   * Input channel should send message to hub->newMessage()
   *
   * If channel implementation depends on any abstraction represented by file descriptor the pollThread()
   * can be used:
   * _fd : file descriptor, should be prepared during activate()
   * startPolling() and stopPolling() functions start and stop the pollThread which reads _fd (using select())
   * and sends messages to _hub.
   *
   * If channel has own abstraction then it can override _pollThread() and implement own message loop.
   */
  class Channel {
  protected:
    std::atomic_bool _active;                       /**< Channel is prepared and active */

    /* Polling functions */
    std::unique_ptr<std::thread> _thread;           /**< Pointer to reader thread in case of input channel */
    std::atomic_bool _pipeRunning;                  /**< Pipe reading thread is running */
    virtual void pollThread();                      /**< Thread which selects the descriptor and send messages when new ones come */
    int _fd;                                        /**< File descriptor to select */

    const config::ConfigParser _config;             /**< Configuration storage */
    const std::string _name;                        /**< The channel name in config file */
    const ChannelDirection _direction;              /**< The channel direction for the whole transmission task */
    Hub::Hub * const _hub;                           /**< Hub the channel is attached to */


    /**
     * Parse line and send it to needed output place in case of Output direction
     *
     * @param msg Incoming message from hub. Passed by value to preserve ownership
     *            and ensure existence during processing in all output channels.
     */
    virtual void incoming(const message_ptr&& msg) = 0;

    /**
     * Parse a text line and generate a corresponding Message
     * @todo Decide if it should throw something
     *
     * @param line Text line from socket
     */
    virtual const message_ptr parse(const char* line) const = 0;

    /**
     * Start thread which polls the descriptor Channel::_fd
     *
     * Descriptor must be opened before running.
     * @throws std::runtime_error(ERR_FD) If descriptor is not opened
     */
    void startPolling();

    /**
     * Stops and joins polling thread
     */
    void stopPolling();

  public:
    const uint16_t _id;                             /**< Unique channel id */

    /**
     * @param hub Hub to add channel to
     * @param config configuration data or path (see config.hpp for details)
     */
    Channel(Hub::Hub * const hub, const std::string& config);

    virtual ~Channel() {};

    /**
     * Returns channel name
     *
     * @retval Channel name
     */
    virtual std::string const& name() const;


    /**
     * Returns channel "type" --- unique string to identify channel in config file
     * @retval std::string Type line
     */
    virtual std::string type() const = 0;

    /**
     * Returns a channel direction
     *
     * @retval Transmission direction
     */
    ChannelDirection direction() const {return _direction; };

    /**
     * Prepare all prerequisites for polling thread or prepare the output and start working
     *
     * Opening file descriptors, connecting to network must be done here.
     * After future is valid channel is considered to be ready for work.
     *
     * @throws activate_error in case of problems
     */
    virtual std::future<void> activate() = 0;

    /**
     * Operator >> is used to push data into output channels
     *
     * Example:
     * @code{.cpp}
     * "Line" >> *output;
     * @endcode
     * @throws std::logic_error on writing to input channel
     */
    friend Channel& operator>> (const message_ptr, Channel& channel);

    /**
     * Open socket to server:port using IPv4 and then (if failed) IPv6
     *
     * @param hostname hostname to connect to
     * @param port port to use
     *
     * @throws activate_error with message in case of problems
     */
    virtual int connect(const std::string& hostname, const uint32_t port) const;

    /**
     * Send a line msg to a socket fd
     *
     * @param fd Socket descriptor
     * @param msg line to send
     *
     * @throws std::runtime_error with message if socket fails
     */
    virtual int send(const uint32_t fd, const std::string& msg) const;

    /**
     * Send a line msg to a default socket _fd
     *
     * @param msg line to send
     */
    virtual int send(const std::string& msg) const {return send(_fd, msg); };

    /**
     * Close a socket
     *
     * @param fd Socket descriptor
     */
    virtual int disconnect(const uint32_t fd) const;

    /**
     * Close an _fd socket
     */
    virtual int disconnect() const {return disconnect(_fd); };
  };

  class ChannelCreator
  {
  public:
    ChannelCreator(const std::string& classname);
    virtual Channel * create(Hub::Hub * const hub, const std::string& config) = 0;
  };

  template <class T>
  class ChannelCreatorImpl: public ChannelCreator
  {
  public:
    ChannelCreatorImpl(const std::string& classname) : ChannelCreator(classname) {}

    virtual Channel * create(Hub::Hub * const hub, const std::string& config) { return new T(hub, config); }
  };

  class ChannelFactory
  {
  public:
    static Channel * create(const std::string& classname, Hub::Hub * const hub, const std::string& config);
    static void registerClass(const std::string& classname, ChannelCreator* creator);
    static uint16_t nextId() { return id++; }

  private:
    static std::atomic_int id;
    static std::map<std::string, ChannelCreator *>& get_table();
  };
}
