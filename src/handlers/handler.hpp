#pragma once
#include <any>
#include <future>
#include <map>
#include "message.hpp"

namespace pipelining {
  using messaging::message_ptr;

  namespace handler_error {
    struct handler_error: public std::runtime_error {
      handler_error(std::string const& message) :
	std::runtime_error(message)
      {};
    };

    struct not_registered: public handler_error {
      not_registered(std::string const& name) :
	handler_error(name)
      {};
    };

    struct wrong_initializer: public handler_error {
      wrong_initializer(std::string const& name) :
	handler_error(name)
      {};
    };
  }

  /**
   * Parent class for single pipeline stage
   */
  class Handler {
  protected:
    const uint16_t _id;       /**< Unique handler id */
  public:
    /**
     * Individual data provided for handler creation, treated individually by
     * each handler.
     */
    Handler(std::any _initializer);
    virtual ~Handler() {};
    virtual std::future<message_ptr> processMessage(message_ptr&& msg) = 0;
  };

  class HandlerCreator
  {
  public:
    HandlerCreator(const std::string& classname);
    virtual Handler * create(std::any _initializer) = 0;
  };

  template <class T>
  class HandlerCreatorImpl: public HandlerCreator
  {
  public:
    HandlerCreatorImpl(const std::string& classname) : HandlerCreator(classname) {}
    virtual Handler * create(std::any _initializer) { return new T(_initializer); }
  };

  class HandlerFactory
  {
  public:
    static Handler * create(const std::string& classname,
                                std::any	   _initializer) {
      std::map<std::string, HandlerCreator *>::iterator i;
      i = get_table().find(classname);

      if (i != get_table().end())
	try {
	  return static_cast<Handler *>(i->second->create(_initializer));
	} catch (const std::bad_any_cast& e) {
	  throw
	    handler_error::wrong_initializer(std::string("Wrong initializer for ") +
					     classname);
	}
      else
	throw handler_error::not_registered(classname);
    }
    static void registerClass(const std::string& classname, HandlerCreator* creator);
    static uint16_t nextId() { return id++; }

  private:
    static std::atomic_int id;
    static std::map<std::string, HandlerCreator *>& get_table();
  };

  static inline auto CreateHandler(const std::string& classname,
				   std::any _initializer) {
    return std::unique_ptr<Handler>(HandlerFactory::create(classname, _initializer));
  }
}
