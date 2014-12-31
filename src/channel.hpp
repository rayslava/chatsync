#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <map>

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
    * Thrown during Channel::activate in case of activation problems
    */
    class activate_error: public std::runtime_error {
    public:
        activate_error(std::string const& message): std::runtime_error(message) {};
    };

    /**
    * A channel to connect to input or output
    *
    * The deriving class *must* implement:
    *   void parse(std::string&) const; --- for data input in case of output channel
    *   void activate(); --- to implement specific activities for channel work starting
    *
    * Input channel should send message to hub->newMessage()
    */
    class Channel {
	/* Polling functions */
	std::unique_ptr<std::thread> _thread;       /**< Pointer to reader thread in case of input channel */
	std::atomic_bool _pipeRunning;              /**< Pipe reading thread is running */

	void pollThread();                          /**< Thread which selects the descriptor and send messages when new ones come */

    protected:
        int _fd;                                /**< File descriptor to select */
        const std::string _name;                /**< The channel name in config file */
        const ChannelDirection _direction;      /**< The channel direction for the whole transmission task */
        Hub::Hub* const _hub;                   /**< Hub the channel is attached to */

	/**
	* Parse line and send it to needed output place in case of Output direction
	*
	* @param l An input line coming from hub
	*/
    virtual void parse(const std::string& l) = 0;

	/**
	* Start thread which polls the descriptor Channel::_fd
	*
	* Descriptor must be opened before running.
	* @throws std::runtime_error(ERR_FD) If descriptor is not opened
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
        * @throws std::runtime_error if config is empty
        */
        virtual void parseConfig(std::vector<std::string> const& lines);

        /**
        * Returns channel name
        *
        * @retval Channel name
        */
        virtual std::string const & name() const;


	/**
	* Returns channel "type" --- unique string to identify channel in config file
	* @retval std::string Type line
	*/
	virtual std::string type() const = 0;	

        /**
        * Returns a channel direction
        *
        * @retval Transmission direction
        */
        ChannelDirection direction() const {return _direction;};

        /**
        * Prepare all prerequisites for polling thread or prepare the output and start working
        *
        * Opening file descriptors, connecting to network must be done here.
        * After return from the function channel is considered to be ready for work.
        *
        * @throws activate_error in case of problems
        */
        virtual void activate() = 0;

        /**
        * Operator >> is used to push data into output channels
        *
        * Example:
        * @code{.cpp}
        * "Line" >> *output;
        * @endcode
        * @throws std::logic_error on writing to input channel
        */
        friend Channel& operator>> (const std::string &in, Channel& channel);
    };

    class ChannelCreator
    {
    public:
	ChannelCreator(const std::string& classname);
	virtual Channel* create() = 0;
    };

    template <class T>
    class ChannelCreatorImpl : public ChannelCreator
    {
    public:
	ChannelCreatorImpl(const std::string& classname) : ChannelCreator(classname) {}

	virtual Channel* create() { return new T; }
    };

    class ChannelFactory
    {
    public:
	static Channel* create(const std::string& classname);
	static void registerClass(const std::string& classname, ChannelCreator* creator);
    private:
	static std::map<std::string, ChannelCreator*>& get_table();
    };
}
