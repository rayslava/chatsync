#pragma once

#include <string>
#include <map>
#include <memory>

namespace Config {

    /**
     * Single option from config.
     * Can be casted to needed type, however is stored as std::string
     */
    class ConfigOption {
    private:
	const std::string name;
	const std::string value;
    public:
	/**
	 * Create option with
	 *
	 * @param name string to be the option unique name (in the config set)
	 * @param value data which will be implicitly lazily converted to type needed
	 */
	ConfigOption(const std::string&& name, const std::string&& value): name(name), value(value) {};

	/**
	 * Implicit conversions to string-compatible classes
	 */
	template <typename T>
	operator T() const {return T(value);}

	/**
	 * Implicit conversion to int
	 */
	operator int() const;
    };

    /**
     * An object for parsing configuration file
     *
     * Config file format is just like .ini files
     *
     * Hub::Hub config:
     * [hub]
     * name = "Hub 1"
     *
     * Channelling::Channel config:
     * [channel]
     * name = "Channel name"
     * direction = in|out|both
     *
     */
    class ConfigParser {
    private:
	std::unique_ptr<std::map<const std::string, const ConfigOption> const> config;
	static decltype(config) parseConfig(std::string data);
    public:
        /**
	 * Creates an object
	 *
	 * param @filename Configuration file name and path
	 */
        ConfigParser(std::string path);

        /**
	 * Parses a configuration file
	 */
        void parseFile() const;
    };
}
