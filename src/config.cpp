#include <regex>
#include <string>
#include <fstream>
#include <streambuf>
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

    ConfigOption::operator Channeling::ChannelDirection() const {
	if (_value == "input")
	    return Channeling::ChannelDirection::Input;
	else if (_value == "output")
	    return Channeling::ChannelDirection::Output;

	throw option_error(ERR_WRONG_DIR + ": " + _value);
    }

    const std::string ConfigParser::openConfig(const std::string& path) {
	if (std::equal(configPrefixData.begin(), configPrefixData.end(), path.begin()))
	    return path.substr(configPrefixData.length());

	if (std::equal(configPrefixFile.begin(), configPrefixFile.end(), path.begin())) {
	    std::ifstream t(path.substr(configPrefixFile.length()));
	    std::string str((std::istreambuf_iterator<char>(t)),
			    std::istreambuf_iterator<char>());
	    return str;
	}

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
	try {
	    return _config->at(option);
	} catch (std::out_of_range) {
	    throw option_error(ERR_NO_OPTION + ": " + option);
	}
    }
}
