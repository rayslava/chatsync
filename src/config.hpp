#pragma once

#include <string>

namespace Config {

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
        const std::string configFile;
    public:
        /**
        * Creates an object
        *
        * param @filename Configuration file name and path
        */
        ConfigParser(std::string filename);

        /**
        * Parses a configuration file
        */
        void parseFile() const;
    };
}
