#include "handler_http_probe.hpp"
#include "logging.hpp"

namespace pipelining {
  const HandlerCreatorImpl<HttpProbeHandler> HttpProbeHandler::creator("http_probe");

  HttpProbeHandler::HttpProbeHandler(std::any _initializer) :
    ParallelHandler(_initializer),
    _send(std::any_cast<typeof(_send)>(_initializer))
  {}

  std::future<void> HttpProbeHandler::ProcessMessage(const message_ptr&& msg) {
    const messaging::TextMessage m(*messaging::TextMessage::fromMessage(msg));
    _send(std::move(m));
    return std::async(std::launch::async, [](){
      DEBUG << "Processing with initializer ";
    });
  }
}
