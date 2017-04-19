#include "logging.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <future>

namespace logging {

  void LoggerImpl::log(const LogMessage&& msg) {
    // Prepare the message
    const static unsigned int timeout = 25;
    const static unsigned int max_repeats = 5;
    {
      std::unique_lock<std::mutex> mlock(_mutex, std::defer_lock);
      if (mlock.try_lock()) {
        _messages.push(std::move(msg));
        _log_repeat = 1;
      } else {
        if (_log_repeat > max_repeats)
          throw std::runtime_error("Maximum number of log attempts reached");
        std::thread(
          [this, msg {std::move(msg)}]() {
          const auto& rep = this->_log_repeat;
          std::this_thread::sleep_for(std::chrono::milliseconds(timeout * rep * rep));
          this->log(std::move(msg));
        }).detach();
        ++_log_repeat;
      }
    }
    // Notifying doesn't require mutex lock
    _cond.notify_one();
  }

  void LoggerImpl::writeOut() {
    while (_running && !_sink.expired()) {
      if (const auto& out = _sink.lock()) {
        std::unique_lock<std::mutex> mlock(_mutex);

        while (_messages.empty() && _running)
          _cond.wait(mlock);

        if (!_running)
          continue;

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
           << std::setfill('0') << std::setw(3) << fractional_seconds << "]  "
           << "[" << severity_lines[msg._severity] << "] "
           << msg._message;
    return result.str();
  }

  void LogSinkPrinter::write(const LogMessage&& msg) {
    std::cerr << formatMessage(std::move(msg)) << std::endl;
  }
}
