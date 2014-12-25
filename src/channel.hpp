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
    * A channel to connect to input or output
    *
    * The deriving class *must* implement:
    *   void parse(std::string&) const; --- for data input in case of output channel
    *
    * Input channel should send message to hub->newMessage()
    */
    class Channel {
	/* Polling functions */
	std::unique_ptr<std::thread> _thread;       /**< Pointer to reader thread in case of input channel */
	std::atomic_bool _pipeRunning;              /**< Pipe reading thread is running */

	void pollThread();                          /**< Thread which selects the descriptor and send messages when new ones come */

    protected:
      	int _fd;                                    /**< File descriptor to select */
        const std::string _name;                            /**< The channel name in config file */
        const ChannelDirection _direction;                  /**< The channel direction for the whole transmission task */
        Hub::Hub* const _hub;                               /**< Hub the channel is attached to */

	/**
	* Parse line and send it to needed output place in case of Output direction
	*
	* @param l An input line coming from hub
	*/
        virtual void parse(const std::string& l) = 0;

	/**
	* Start thread which polls the descriptor
	*/
	void startPolling();

	/**
	* Stops and joins polling thread
	*/
	void stopPolling();

    public:
        /**
        * @param name A human-readable channel name in config file
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
    };
}
