#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>

class Logger {
public:
    enum class LogLevel {
        error,
        warning,
        info,
        debug
    };

    Logger(LogLevel level = LogLevel::error);

    ~Logger();

    template <typename T>
    Logger& operator<<(T const& value);

private:
    std::ostringstream buffer_;
    LogLevel level_;
};

#endif