#include <regex>
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
	std::regex optionRe("^\\s*(\\S+)\\s*=\\s*(\\S+)$");
	std::smatch optionMatches;

	std::stringstream ss(data);
	std::string item;

	while (std::getline(ss, item, '\n'))
	    if (std::regex_match(item, optionMatches, optionRe))
		configMap->emplace(optionMatches[1].str(), optionMatches[2].str());

	return configMap;
    }

    const ConfigOption ConfigParser::operator[] (const std::string&& option) const {
	return _config->at(option);
    }
}
