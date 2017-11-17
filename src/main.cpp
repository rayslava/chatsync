#include <iostream>
#include <fstream>
#include <regex>
#include <memory>
#include <iomanip>
#include <signal.h>
#ifdef TLS_SUPPORT
#include <gnutls/gnutls.h>
#endif
#include "channel.hpp"
#include "config.hpp"
#include "logging.hpp"


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

  DEFAULT_LOGGING
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
      if (config::strutil::cistrcmp(optionMatches[1], "general")) {
        std::string buffer = "data://";
        do {
          config_stream >> line;
          buffer.append(line + "\n");
        } while(!std::regex_match(line, optionMatches, sectionRe));
        config::ConfigParser generalOptions(buffer);
        tick_timeout = generalOptions.get("tick_timeout", "1200000");
        DEBUG << "Tick timeout set to: " << tick_timeout;
      }
      if (config::strutil::cistrcmp(optionMatches[1], "hub")) {
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

        /* Channel parsing */
        buffer = "data://";
        while (true) {
          config_stream >> line;
          if (std::regex_match(line, optionMatches, sectionRe) || config_stream.eof()) {
            DEBUG << "Channel : " << buffer;
            config::ConfigParser channelOptions(buffer);
            channeling::ChannelFactory::create(std::string(channelOptions["type"]), currentHub.get(), buffer);
            buffer = "data://";
            if (config::strutil::cistrcmp(optionMatches[1], "hub"))
              break;
            if (!config::strutil::cistrcmp(optionMatches[1], "channel"))
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
