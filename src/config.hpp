#pragma once

#include <string>
#include <map>
#include <memory>

namespace Config {

    /**
     * Generic configuration error
     */
    class config_error: public std::runtime_error {
    public:
        config_error(std::string const& message): std::runtime_error(message) {};
    };

    /**
     * Requested option can't be received (e.g. not found)
     */
    class option_error: public config_error {
    public:
        option_error(std::string const& message): config_error(message) {};
    };


    /**
     * Single option from config.
     * Can be casted to needed type, however is stored as std::string
     */
    class ConfigOption {
    private:
	const std::string _value;
    public:
	/**
	 * Create option with
	 *
	 * @param value data which will be implicitly lazily converted to type needed
	 */
	ConfigOption(const std::string&& value): _value(std::move(value)) {};
	ConfigOption(const char* value): _value(value) {};

	/**
	 * Implicit conversions to string-compatible classes
	 */
	template <typename T>
	operator T() const {return T(_value);}

	/**
	 * Implicit conversion to int
	 */
	operator int() const;
    };

    /**
     * Raw data is provided as plain text line
     */
    static const std::string configPrefixData = "data://";

    /**
     * Filename is provided to read config
     */
    static const std::string configPrefixFile = "file://";

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
	const std::unique_ptr<std::map<const std::string, const ConfigOption> const> _config;

	/**
	 * Opens @param path parsing scheme and returns a single line with configuration
	 *
	 * Possible schemes:
	 * - file:// open a file, read it to memory and return
	 * - data:// raw data line, just cut the "data://" and return line
	 *
	 * @param path path to config
	 * @retval string with config lines separated with \n
	 *
	 * @throws config_error if file can't be opened
	 */
	static const std::string openConfig(const std::string& path);
	static std::map<const std::string, const ConfigOption>* parseConfig(const std::string& data);
    public:
        /**
	 * Creates an object
	 *
	 * @param path Configuration file name and path
	 */
        ConfigParser(const std::string&& path);

	/**
	 * Get option from storage
	 * @throws option_error if no option found in storage
	 */
	const ConfigOption operator[] (const std::string&& option) const;
    };
}
