#pragma once
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>

#define LOG(lvl)    logging::LogWriter<lvl>
#define TRACE   LOG(logging::Severity::trace)() << __FILE__ << ":" << __LINE__ << " "
#define DEBUG   LOG(logging::Severity::debug)()
#define INFO    LOG(logging::Severity::info)()
#define WARNING LOG(logging::Severity::warning)()
#define ERROR   LOG(logging::Severity::error)()
#define FATAL   LOG(logging::Severity::fatal)()

#define DEFAULT_LOGGING \
  auto default_sink = std::make_shared<logging::LogSinkPrinter>(); \
  logging::LoggerImpl::get().setOutput(std::static_pointer_cast<logging::LogSink>(default_sink));

namespace logging {
  enum Severity {
    fatal,
    error,
    warning,
    info,
    debug,
    trace
  };

  const std::string severity_lines[] = {"FATAL", "ERROR", "WARNING",
                                        "INFO", "DEBUG", "TRACE"};

  /**
   * Single log message
   */
  struct LogMessage {
    const std::chrono::high_resolution_clock::time_point _timestamp;
    const Severity _severity;
    const std::string _message;
    LogMessage(const std::chrono::high_resolution_clock::time_point t,
               const Severity					    s,
               const std::string				  & m) :
      _timestamp(t),
      _severity(s),
      _message(m) {};
  };

  /**
   * Default sink class. Every sink should be derived from it.
   */
  struct LogSink {
    /**
     * Make the actual output to the sink
     */
    virtual void write(const LogMessage&& msg) = 0;
  };

  /**
   * Sink that prints the log to std::cerr
   */
  class LogSinkPrinter: public LogSink {
    /**
     * Formats message as [TIME] [SEVERITY] Message
     */
    std::string formatMessage(const LogMessage&& msg) const;
  public:
    void write(const LogMessage&& msg) override;
  };

  /**
   * Implementation of the logger singleton.
   *
   * The logger will gather the messages inside LoggerImpl::_messages until
   * the LoggerImpl::setOutput() is called and the sink is set. After start
   * the logs will be redirected to sink while it's functional.
   *
   * If the sink becomes unavailable the log messages are collected inside
   * the LoggerImpl::_messages again and may possibly lead to large memory
   * usage.
   */
  class LoggerImpl {
    std::queue<LogMessage> _messages;      /**< Message queue */
    std::mutex _mutex;                     /**< Message queue lock */
    std::condition_variable _cond;         /**< The var to signal new messages */
    std::unique_ptr<std::thread> _writer;  /**< Thread for outing */
    std::atomic_bool _running;             /**< Writing logs is active */
    std::weak_ptr<LogSink> _sink;          /**< Place to send messages */

    /**
     * The main thread function for the output.
     *
     * Pops out the messages from the thread and calls LoggerImpl::_sink->write()
     */
    void writeOut();
  public:
    static LoggerImpl& get();              /**< Access point to the singleton */

    LoggerImpl(LoggerImpl const&) = delete;
    LoggerImpl(LoggerImpl&&) = delete;
    LoggerImpl& operator=(LoggerImpl const&) = delete;
    LoggerImpl& operator=(LoggerImpl&&) = delete;

    /**
     * Just pushs the \c msg to the \c _messages queue
     */
    void log(const LogMessage&& msg);

    /**
     * Switch on output and start logging.
     *
     * \param output The prepared logging::LogSink to write the logs.
     */
    void setOutput(const std::shared_ptr<LogSink>& output);

  protected:
    LoggerImpl() :
      _messages(),
      _mutex(),
      _cond(),
      _writer(nullptr),
      _running(ATOMIC_FLAG_INIT)
    {}

    ~LoggerImpl() {
      if (_writer) {
        while (!_messages.empty() && !_sink.expired()) { _cond.notify_one(); }
        _running = false;
        _writer->detach();
        _cond.notify_one();
      }
    }
  };

  /**
   * The in-code writer object. Should be used when the actual logging is needed.
   *
   * The timestamp is taken when the object created but the message may be
   * constructed for a while after then and the actual sending to the logger
   * happens on the object destroy. So the logging gets latency but won't make
   * freezes during the program run.
   */
  template <Severity s>
  class LogWriter {
    std::stringstream _message;
    const std::chrono::high_resolution_clock::time_point _timestamp;
  public:
    LogWriter() : _timestamp(std::chrono::high_resolution_clock::now()) {};
    ~LogWriter() {
      LoggerImpl::get().log(LogMessage(_timestamp, s, _message.str()));
    };

    /**
     * The main logging function. Every log object should be pushed there
     * using the LogWriter::operator<<()
     */
    template <typename T>
    LogWriter& operator<< (const T& log_object) {
      _message << log_object;
      return *this;
    }
  };

}
