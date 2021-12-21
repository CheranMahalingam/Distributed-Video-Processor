#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>
#include <ctime>
#include <iostream>
#include <mutex>
#include <string>

enum class LogLevel {
    Error,
    Warning,
    Info,
    Debug
};

class Log {
public:
    Log(LogLevel level = LogLevel::Error);

    ~Log();

    template <typename T>
    Log& operator<<(T const& value) {
        buffer_ << value << " ";
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream buffer_;
};

class LogBuffer {
public:
    static LogBuffer& Logger();

    void Write(LogLevel severity, std::string message);

private:
    // Make into singleton class
    LogBuffer();
    LogBuffer(LogBuffer const&);
    void operator=(LogBuffer const&);

    std::string Timestamp();

    std::string Severity(LogLevel severity);

    std::mutex log_mutex_;
};

#endif
