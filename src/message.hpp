#pragma once
#include <memory>
#include <iostream>
#include "user.hpp"

namespace messaging {
  /**
   * Message type to distinguish different classes
   */
  enum class MessageType {
    Text,                                                           /**< Plain text message, TextMessage class conforms this one */
    Action                                                          /**< Action message (created with /me), ActionMessage class conforms this one */
  };

  /**
   * Base class for all messages, needed for general architecture planning
   *
   * Lifecycle:
   * 1. message_ptr is created inside an input channel and std::move()'d into hub
   * 2. Hub creates message_ptr copies and sends them to each output channel asyncronously
   * 3. After every output sends data to receiver Message dies with last message_ptr
   */
  class Message {
  public:
    const uint16_t _originId;                                       /**< Id of channel produced the message */
    Message(const uint16_t id) : _originId(id) {};
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
    const std::string _data;                                        /**< Message text */
    const std::shared_ptr<const messaging::User> _user;             /**< Message author */
  public:
    TextMessage(const uint16_t origin, std::shared_ptr<const messaging::User>&& user, const std::string& data) :
      Message(origin),
      _data(data),
      _user(std::move(user)) {};

    const std::string& data() const { return _data; };
    const std::shared_ptr<const messaging::User> user() const { return _user; };

    MessageType type() const override { return MessageType::Text; };

    /**
     * Convertor from general Message class
     */
    static auto fromMessage(const message_ptr msg) {
      return static_cast<typename std::shared_ptr<const TextMessage>::element_type *>(msg.get());
    }
  };

  /**
   * Action message representation
   *
   * /me messages
   */
  class ActionMessage: public Message {
    const std::string _data;                                        /**< Message text */
    const std::shared_ptr<const messaging::User> _user;             /**< Message author */
  public:
    ActionMessage(const uint16_t origin, std::shared_ptr<const messaging::User>&& user, const std::string& data) :
      Message(origin),
      _data(data),
      _user(std::move(user)) {};

    const std::string& data() const { return _data; };
    const std::shared_ptr<const messaging::User> user() const { return _user; };

    MessageType type() const override { return MessageType::Action; };

    /**
     * Convertor from general Message class
     */
    static auto fromMessage(const message_ptr msg) {
      return static_cast<typename std::shared_ptr<const ActionMessage>::element_type *>(msg.get());
    }
  };
}
