#include "logging.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

namespace logging {

  void LoggerImpl::log(const LogMessage&& msg) {
    // Prepare the message
    {
      std::unique_lock<std::mutex> mlock(_mutex);
      _messages.push(std::move(msg));
    }
    // Notifying doesn't require mutex lock
    _cond.notify_one();
  }

  void LoggerImpl::writeOut() {
    while (_running && !_sink.expired()) {
      if (const auto& out = _sink.lock()) {
        std::unique_lock<std::mutex> mlock;
        while (_messages.empty()) {
          mlock = std::unique_lock<std::mutex>(_mutex);
          _cond.wait(mlock);
        }

        auto item = std::move(_messages.front());
        _messages.pop();
        if (mlock.owns_lock())
          mlock.unlock();
        out->write(std::move(item));
      }
    }
  }

  LoggerImpl& LoggerImpl::get() {
    static LoggerImpl instance;
    if (!instance._running)
      instance._running = true;

    return instance;
  }

  void LoggerImpl::setOutput(const std::shared_ptr<LogSink>& output) {
    _sink = output;
    if (!_writer)
      _writer = std::make_unique<std::thread>
                  (std::thread(&LoggerImpl::writeOut, this));
  }

  std::string LogSinkPrinter::formatMessage(const LogMessage&& msg) const {
    std::stringstream result;
    const auto& ms = std::chrono::duration_cast<std::chrono::milliseconds>
                       (msg._timestamp.time_since_epoch());
    const std::chrono::seconds& sec =
      std::chrono::duration_cast<std::chrono::seconds>(ms);
    std::time_t t = sec.count();
    std::size_t fractional_seconds = ms.count() % 1000;

    result << "[" << std::put_time(std::localtime(&t), "%F %T.")
           << fractional_seconds << "]  "
           << "[" << severity_lines[msg._severity] << "] "
           << msg._message;
    return result.str();
  }

  void LogSinkPrinter::write(const LogMessage&& msg) {
    std::cerr << formatMessage(std::move(msg)) << std::endl;
  }
}
