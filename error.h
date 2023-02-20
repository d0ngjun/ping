#pragma once

#include <exception>
#include <sstream>

class PingError : public std::exception {
public:
    PingError(const std::string &host, const std::string &function, int error_code) {
        std::stringstream ss;

        ss << "host: " << host << ", function: " << function << ", error: " << strerror(error_code);

        _what = ss.str();
    }

	const char *what() const noexcept {
        return _what.c_str();
    }

private:
    std::string _what;
};
