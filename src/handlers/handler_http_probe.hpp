#pragma once
#include "handler.hpp"
#include <regex>
#include <list>

namespace pipelining {
  /**
   * Handler that makes an HTTP request and reports title and stats
   */
  class HttpProbeHandler: public Handler {
    /**
     * Type of Hub::Hub::newMessage, which will receive the report
     */
    typedef std::function<void(const messaging::message_ptr&& msg)> Sender;

    /**
     * Function to send report
     */
     Sender _send;

    static const pipelining::HandlerCreatorImpl<HttpProbeHandler> creator;
  public:
    /**
     * Handler constructor
     *
     * \param _initializer a function to send message with \c Sender signature
     */
    explicit HttpProbeHandler(std::any _initializer);
    ~HttpProbeHandler() {};

    /**
     * Message processing function
     *
     * \retval Handler of function which checks pages and reports results.
     * Should be kept alive until the job is done.
     */
    std::future<message_ptr> processMessage(message_ptr&& msg) override;
  };
}
