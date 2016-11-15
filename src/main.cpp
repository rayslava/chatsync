#include <iostream>
#include <fstream>
#include <regex>
#include <memory>
#include <iomanip>
#include "channel.hpp"
#include "config.hpp"
#include <signal.h>
#include <signal.h>


static std::atomic_bool running = ATOMIC_FLAG_INIT;

static void sighandler(int signum)
{
  if (signum == SIGINT) {
    running = false;
    std::cout << "SIGINT caught. Finalizing data. "
              << "Will exit after next tick." << std::endl;
  }
}

int main(int argc, char* argv[])
{
  uint64_t tick_timeout = 120000;

  if (argc < 2) {
    std::cerr << "Please use " << argv[0] << " config.ini" << std::endl;
    return 1;
  }
  std::string filename = argv[1];

  std::list<std::shared_ptr<Hub::Hub> > hublist;
  std::ifstream config_stream;
  config_stream.open(filename);
  while(!config_stream.eof())
  {
    std::regex sectionRe("^\\s*\\[(\\S+)\\]\\s*$");
    std::smatch optionMatches;
    std::string line;
    config_stream >> line;
    std::cout << "Parsing '" << line << "'" << std::endl;

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
        std::cout << "Tick timeout set to: " << tick_timeout << std::endl;
      }
      if (config::strutil::cistrcmp(optionMatches[1], "hub")) {
        std::string buffer = "data://";
        /* Hub options */
        do {
          config_stream >> line;
          buffer.append(line + "\n");
        } while(!std::regex_match(line, optionMatches, sectionRe));
        std::cout << "Hub: " << buffer << std::endl;
        config::ConfigParser hubOptions(buffer);
        const auto& currentHub = std::make_shared<Hub::Hub>(hubOptions["name"]);
        /* Hub created */

        /* Channel parsing */
        buffer = "data://";
        while (true) {
          config_stream >> line;
          if (std::regex_match(line, optionMatches, sectionRe) || config_stream.eof()) {
            std::cout << "Channel : " << buffer << std::endl;
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
    std::cerr << "Couldn't set up signal handling, continuing without graceful death possibility" << std::endl;

  running = true;
  for (auto & c : hublist)
    c->activate();

  while (running) {
    const auto now = std::chrono::system_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds (tick_timeout));
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&now_c), "%F0 %T0") << "] tick" << std::endl;
  }

  for (auto & c : hublist)
    c->deactivate();
  return 0;
}
