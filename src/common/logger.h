#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>

class Logger {
public:
    enum class LogLevel {
        Error,
        Warning,
        Info,
        Debug
    };

    Logger(LogLevel level = LogLevel::Error);

    ~Logger();

    template <typename T>
    Logger& operator<<(T const& value);

private:
    std::ostringstream buffer_;
    LogLevel level_;
};

#endif