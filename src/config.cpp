#include "config.hpp"

namespace Config {
    ConfigParser::ConfigParser(std::string path):
	_config(std::move(parseConfig(path)))
    {
	/* TODO: config data path parsing and data reading */
    }

    ConfigOption::operator int() const {
	/* TODO: exception handling */
	return std::stoi(_value);
    }

    std::map<const std::string, const ConfigOption>* ConfigParser::parseConfig(std::string data) {
	auto configMap = new std::map<const std::string, const ConfigOption>;
	configMap->emplace("test", "testval");

	if (data.empty())
	    return configMap;
	return configMap;
    }
}
