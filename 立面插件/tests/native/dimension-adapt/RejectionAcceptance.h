#pragma once
#include <functional>
#include <stdexcept>
#include <string>

namespace n8a {
// Catch only an exception originating in the operation. A normal return is a
// failed rejection test, even when its name happens to equal the expected text.
inline std::string expected_rejection(const std::string& name,const std::function<void()>& operation,const std::string& expected) {
    try { operation(); }
    catch(const std::exception& failure) {
        const std::string message=failure.what();
        if(message.find(expected)==std::string::npos)throw std::runtime_error("fault failed before intended stage "+name+": "+message);
        return message;
    }
    throw std::runtime_error("fault_was_not_rejected:"+name);
}
}
