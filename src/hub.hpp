#pragma once
#include "message.hpp"

#include <list>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifdef HANDLERS
#include "handlers/handler.hpp"
#endif

namespace channeling {
  class Channel;
}

namespace Hub {
  typedef std::unique_ptr<channeling::Channel> chanPtr;
#ifdef HANDLERS
  typedef std::unique_ptr<pipelining::Handler> handlerPtr;
#endif

  /**
   * A thread-safe implementation of two connected channels sets.
   * All input channels are redirected to every output channel.
   */
  class Hub {
  private:
    const std::string _name;                        /**< Human-readable name */
    std::list<chanPtr> _inputChannels;              /**< Container for all input channels */
    std::list<chanPtr> _outputChannels;             /**< Container for all output channels */
#ifdef HANDLERS
    /**
     * Handlers list
     *
     * These handlers are executed one-by-one in order of adding to the list.
     * The runs are pipelined: result on first handler becomes input of the
     * second one.
     */
    std::list<handlerPtr> _handlers;
#endif
    std::queue<messaging::message_ptr> _messages;   /**< Message queue */
    std::mutex _mutex;                              /**< Message queue lock */
    std::condition_variable _cond;                  /**< Lock condvar */

    std::unique_ptr<std::thread> _msgLoop;          /**< Message processing thread (created from msgLoop() */
    std::atomic_bool _loopRunning;                  /**< Messaging is active */

    /**
     * Append one more input channel to list taking ownership
     */
    void addInput(channeling::Channel * const);

    /**
     * Append one more output channel to list taking ownership
     */
    void addOutput(channeling::Channel * const);

    /*
     * Queue operations
     */

    /**
     * Sleeps on _cond waiting for messages. When the message comes returns it.
     *
     * @return Incoming message from _messages queue.
     */
    const messaging::message_ptr popMessage();
    void pushMessage(const messaging::message_ptr&& item);

    /**
     * Thread function with main event loop
     */
    void msgLoop();

#ifdef HANDLERS
    /**
     * Thread function with main event loop
     */
    void msgLoopPipelined();
#endif

    /**
     * Shows the hub is still alive and channels should continue work
     */
    std::atomic<bool> _alive;
  public:
    Hub(std::string const& name);
    ~Hub() { };

    const std::string& name() const { return _name; };
    bool alive() const { return _alive.load(std::memory_order_acquire); };

    /**
     * Append channel accordingly to its direction
     */
    void addChannel(channeling::Channel * const);

#ifdef HANDLERS
    /**
     * Append channel accordingly to its direction
     */
    void addHandler(handlerPtr);
#endif

    /**
     * New message receiving callback
     *
     * @param msg The message. Constref is used to pass ownership to this
     * Hub. Must be normally passed using std::move()
     */
    void newMessage(const messaging::message_ptr&& msg);

    /**
     * Start message loop
     */
    void activate();

    /**
     * Stop message loop
     */
    void deactivate();

    /**
     * Returns whether thread is running
     */
    bool active() {return _loopRunning; };

    /**
     * Triggered on tick by main thread
     */
    void tick();
  };
}
