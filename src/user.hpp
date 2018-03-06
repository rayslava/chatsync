#pragma once
#include <string>

namespace messaging {

  constexpr uint16_t system_user_id = 0xFFFF; /**< ID for messages from bot */

  /**
   * Internal user representation
   */
  class User {
    const std::string _name;                /**< User nickname */
  public:
    User(const std::string& name) : _name(name) {};
    const std::string& name() const { return _name; };
  };
}
