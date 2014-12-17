#pragma once
#include <string>
#include <iostream>
#include <vector>

#include "hub.hpp"

namespace Channeling {

    /**
    * Channel direction
    */
    enum class ChannelDirection {
        Input,                      /**< Receive data  */
        Output,                     /**< Transmit data */
        Bidirectional               /**< Receive and transmit data */
    };

    /**
    * Class describing a channel to connect to input or output
    *
    * The deriving class *must* implement:
    *   void print(std::ostream&) const; --- for data output
    *   void parse(std::string&) const; --- for data input
    *
    */
    class Channel {
    public:
        /**
        * @param name Channel name in config file
        * @param direction Channel transmission direction
        * @param hub Hub to add channel to
        */
        Channel(std::string const &name, ChannelDirection const &direction, Hub::Hub* const hub);
        virtual ~Channel() {};

        /**
        * Parses a configuration for this certain channel
        *
        * @param lines Part of config file to parse for this channel
        */
        virtual void parseConfig(std::vector<std::string> const& lines);

        /**
        * Returns channel name
        *
        * @retval Channel name
        */
        virtual std::string const & name() const;

        /**
        * Returns a channel direction
        *
        * @retval Transmission direction
        */
        ChannelDirection direction() const {return _direction;};

        friend Channel& operator>> (const std::string &in, Channel& channel);

    protected:
        const std::string _name;                            /**< The channel name in config file */
        const ChannelDirection _direction;                  /**< The channel direction for the whole transmission task */
        Hub::Hub* const _hub;                               /**< Hub the channel is attached to */

        virtual std::string const& parse(const std::string& l) const = 0;
    };
}
