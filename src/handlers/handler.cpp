#include "handler.hpp"
#include "logging.hpp"
#include <regex>
#include <list>

namespace pipelining {
  std::atomic_int HandlerFactory::id {ATOMIC_FLAG_INIT};

  std::map<std::string, HandlerCreator *>& HandlerFactory::get_table() {
    static std::map<std::string, HandlerCreator *> table;
    return table;
  }

  void HandlerFactory::registerClass(const std::string& classname, HandlerCreator* creator) {
    get_table()[classname] = creator;
  }

  HandlerCreator::HandlerCreator(const std::string& classname) {
    HandlerFactory::registerClass(classname, this);
  }

  Handler::Handler(std::any _initializer):
    _id(HandlerFactory::nextId())
  {
    DEBUG << "Creating handler " << _id << (_initializer.has_value() ? " with initializer" : " without initializer");
  }
}
