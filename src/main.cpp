#include <iostream>
#include <fstream>
#include <regex>
#include <memory>
#include <iomanip>
#include <signal.h>
#ifdef TLS_SUPPORT
#include <gnutls/gnutls.h>
#endif
#ifdef HANDLERS
#include <functional>
#include "handlers/handler.hpp"
#endif
#include "channel.hpp"
#include "config.hpp"
#include "logging.hpp"
#include "strutil.hpp"


static std::atomic_bool running = ATOMIC_FLAG_INIT;

static void sighandler(int signum)
{
  if (signum == SIGINT)
    running = false;
}

int main(int argc, char* argv[])
{
  uint64_t tick_timeout = 120000;

  if (argc < 2) {
    std::cerr << "Please use " << argv[0] << " config.ini" << std::endl;
    return 1;
  }
  std::string filename = argv[1];

  DEFAULT_LOGGING;
  DEFAULT_LOGGER_SEVERITY(logging::Severity::debug);

  std::list<std::shared_ptr<Hub::Hub> > hublist;
  std::ifstream config_stream;
  config_stream.open(filename);
  while(!config_stream.eof())
  {
    std::regex sectionRe("^\\s*\\[(\\S+)\\]\\s*$");
    std::smatch optionMatches;
    std::string line;
    config_stream >> line;
    DEBUG << std::string("Parsing '") << line << std::string("'");

    /* Hub parsing */
    if (std::regex_match(line, optionMatches, sectionRe)) {
      if (strutil::cistrcmp(optionMatches[1], "general")) {
        std::string buffer = "data://";
        do {
          config_stream >> line;
          buffer.append(line + "\n");
        } while(!std::regex_match(line, optionMatches, sectionRe));
        config::ConfigParser generalOptions(buffer);
        tick_timeout = generalOptions.get("tick_timeout", "1200000");
        DEBUG << "Tick timeout set to: " << tick_timeout;
      }
      if (strutil::cistrcmp(optionMatches[1], "hub")) {
        std::string buffer = "data://";
        /* Hub options */
        do {
          config_stream >> line;
          buffer.append(line + "\n");
        } while(!std::regex_match(line, optionMatches, sectionRe));
        DEBUG << "Hub: " << buffer;
        config::ConfigParser hubOptions(buffer);
        const auto& currentHub = std::make_shared<Hub::Hub>(hubOptions["name"]);
        /* Hub created */
#ifdef HANDLERS
        /* Parse and create handlers */
        std::string handlers_list = hubOptions.get("handlers", "");
        if (!handlers_list.empty()) {
          DEBUG << "Handlers for : " << hubOptions["name"] << ": " << handlers_list;
          std::vector<std::string> handlers;
          size_t pos = 0;
          const std::string delimiter = ",";
          std::string token;
          if (handlers_list.find(delimiter) == std::string::npos) {
            /* The only handler */
            handlers.emplace_back(strutil::trimmed(handlers_list));
          } else {
            /* Several handlers */
            while ((pos = handlers_list.find(delimiter)) != std::string::npos) {
              token = handlers_list.substr(0, pos);
              handlers.emplace_back(strutil::trimmed(token));
              handlers_list.erase(0, pos + delimiter.length());
            }
          }
          for (const auto& handler : handlers) {
            std::any initializer;
            /* Now we have to prepare appropriate initializers for handlers
               accordingly to their documentation */
            if (handler == "http_probe") {
              initializer = std::function<void(const messaging::message_ptr&& msg)>(std::bind(&Hub::Hub::newMessage, currentHub.get(), std::placeholders::_1));
            } else {
              throw config::config_error("Unsupported handler " + handler);
            }
            Hub::handlerPtr probe =
              pipelining::CreateHandler(handler, initializer);
            currentHub->addHandler(std::move(probe));
          }
        }
#endif
        /* Channel parsing */
        buffer = "data://";
        while (true) {
          config_stream >> line;
          if (std::regex_match(line, optionMatches, sectionRe) || config_stream.eof()) {
            DEBUG << "Channel : " << buffer;
            config::ConfigParser channelOptions(buffer);
            channeling::ChannelFactory::create(std::string(channelOptions["type"]), currentHub.get(), buffer);
            buffer = "data://";
            if (strutil::cistrcmp(optionMatches[1], "hub"))
              break;
            if (!strutil::cistrcmp(optionMatches[1], "channel"))
              throw config::config_error("Config file format error");
          } else
            buffer.append(line + "\n");
        }
        hublist.push_back(std::move(currentHub));
      }
    }
  }
  config_stream.close();

  /* Signal handling */
  struct sigaction sa;

  sa.sa_handler = sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;   /* Restart functions if interrupted by handler */
  if (sigaction(SIGINT, &sa, NULL) == -1)
    ERROR << "Couldn't set up signal handling, continuing without graceful death possibility";

#ifdef TLS_SUPPORT
  gnutls_global_init();
#endif

  running = true;
  for (auto& c : hublist)
    c->activate();

  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds (tick_timeout));
    INFO << "tick";
    for (auto& c : hublist)
      c->tick();
  }

  WARNING << "SIGINT caught. Finalizing data. "
          << "Will exit after threads are stopped.";

  for (auto& c : hublist)
    c->deactivate();

#ifdef TLS_SUPPORT
  gnutls_global_deinit();
#endif

  return 0;
}
