#pragma once
#include <memory>
#include <iostream>
#include "user.hpp"

namespace messaging {
    /**
     * Message type to distinguish different classes
     *
     * Lifecycle:
     * 1. message_ptr is created inside an input channel and std::move()'d into hub
     * 2. Hub creates message_ptr copies and sends them to each output channel asyncronously
     * 3. After every output sends data to receiver Message dies with last message_ptr
     */
    enum class MessageType  {
        Text                                                        /**< Plaintext message, TextMessage class conforms this one */
    };

    /**
     * Base class for all messages, needed for general architecture planning
     */
    class Message {
    public:
        /**
         * Created to avoid typeid() calls
         */
        virtual MessageType type() const = 0;
    };

    /**
     * General type to be passed through between internal functions
     */
    typedef std::shared_ptr<const Message> message_ptr;

    /**
     * Plaintext message representation
     *
     * Now it works with std::string but looks like unicode characters are supported
     */
    class TextMessage: public Message {
        const std::string _data;                                    /**< Message text */
        const std::shared_ptr<const messaging::User> _user;         /**< Message author */
    public:
        TextMessage(const std::shared_ptr<const messaging::User>&& user, const std::string& data):
            _data(data),
            _user(std::move(user)) {};

        const std::string& data() const { return _data; };
        const std::shared_ptr<const messaging::User> user() const { return _user; };	

        MessageType type() const override { return MessageType::Text;};

        /**
         * Convertor from general Message class
         */
        static auto fromMessage(const message_ptr msg) {
            return static_cast<typename std::shared_ptr<const TextMessage>::element_type*>(msg.get());
        }
    };
}
