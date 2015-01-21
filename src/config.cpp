#include "config.hpp"
#include "messages.hpp"

namespace Config {
    ConfigParser::ConfigParser(const std::string&& path):
	_config(std::move(parseConfig(openConfig(path))))
    {
	/* TODO: config data path parsing and data reading */
    }

    ConfigOption::operator int() const {
	/* TODO: exception handling */
	return std::stoi(_value);
    }

    const std::string ConfigParser::openConfig(const std::string& path) {
	if (std::equal(configPrefixData.begin(), configPrefixData.end(), path.begin()))
	    return path.substr(configPrefixData.length());

	throw config_error(ERR_CONFIG_SCHEME);
    }

    std::map<const std::string, const ConfigOption>* ConfigParser::parseConfig(const std::string& data) {
	auto configMap = new std::map<const std::string, const ConfigOption>;
	configMap->emplace("test", "testval");

	if (data.empty())
	    return configMap;
	return configMap;
    }
}
