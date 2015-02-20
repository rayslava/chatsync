#pragma once
#include <memory>
#include <iostream>
#include "user.hpp"

namespace messaging {
    /**
     * Internal message representation
     *
     * Lifecycle:
     * 1. message_ptr is created inside an input channel and std::move()'d into hub
     * 2. Hub creates message_ptr copies and sends them to each output channel asyncronously
     * 3. After every output sends data to receiver Message dies with last message_ptr
     */
    class Message {
        const std::string _data;                                    /**< Message text */
        const std::shared_ptr<const messaging::User> _user;         /**< Message author */
    public:
        Message(const std::shared_ptr<const messaging::User>&& user, const std::string& data):
            _data(data),
            _user(std::move(user)) {};
        const std::string& data() const { return _data; };
        const std::shared_ptr<const messaging::User> user() const { return _user; };	
    };

    typedef std::shared_ptr<const Message> message_ptr;
}
