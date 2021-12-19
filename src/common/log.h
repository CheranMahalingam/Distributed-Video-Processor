#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>
#include <ctime>
#include <iostream>
#include <mutex>
#include <string>

// class Log {
// public:
//     enum class LogLevel {
//         Error,
//         Warning,
//         Info,
//         Debug
//     };

//     Log(LogLevel level = LogLevel::Error);

//     ~Log();

//     std::string timestamp();

//     template <typename T>
//     Log& operator<<(T const& value);

// private:
//     std::ostringstream buffer_;
//     LogLevel level_;
// };

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
    Log& operator<<(T const& value);

private:
    LogLevel level_;
    std::stringstream buffer_;
};

class LoggerBuffer {
public:
    static LoggerBuffer& Logger();

    void Write(LogLevel severity, std::string message);

private:
    // Make into singleton
    LoggerBuffer();
    LoggerBuffer(LoggerBuffer const&);
    void operator=(LoggerBuffer const&);

    std::string Timestamp();

    std::string Severity(LogLevel severity);

    std::mutex log_mutex_;
};

#endif