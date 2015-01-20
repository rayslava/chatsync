#include "config.hpp"

namespace Config {
    ConfigParser::ConfigParser(std::string path)
    {
	/* TODO: config data path parsing and data reading */
	config = std::move(parseConfig(path));
    }

    ConfigOption::operator int() const {
	/* TODO: exception handling */
	return std::stoi(value);
    }

    decltype(ConfigParser::config) ConfigParser::parseConfig(std::string data) {
	if (data.empty())
	    return nullptr;
	return nullptr;
    }
}
