#pragma once
#include <string>

namespace messaging {
    class User {
	const std::string _name;        /**< Username */
    public:
	User(const std::string&& name): _name(name) {};
	const std::string& name() const { return _name; };
    };
}
