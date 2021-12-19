#include "log.h"

// Log::Log(LogLevel level)
//     : level_(level) {}

// Log::~Log() {
//     buffer_ << std::endl;
//     std::cerr << buffer_.str();
// }

// template <typename T> Log& Log::operator<<(T const& value) {
//     buffer_ << value << " ";
//     return *this;
// }

Log::Log(LogLevel level) : level_(level) {}

Log::~Log() {
    LoggerBuffer::Logger().Write(level_, buffer_.str());
}

template <typename T>
Log& Log::operator<<(T const& value) {
    buffer_ << value << " ";
    return *this;
}

LoggerBuffer& LoggerBuffer::Logger() {
    static LoggerBuffer instance;
    return instance;
}

void LoggerBuffer::Write(LogLevel severity, std::string message) {
    std::lock_guard<std::mutex> lockGuard(log_mutex_);
    std::stringstream out;
    out << Timestamp() << " " << Severity(severity) << ": " << message << std::endl;
    std::cout << out.str();
}

std::string Timestamp() {
    std::time_t raw_time;
    std::time(&raw_time);
    std::tm* curr_time = std::gmtime(&raw_time);
    char buffer[30];
    std::strftime(buffer, 30, "%Y-%m-%d %H:%M:%S %Z", curr_time);
    std::string time_string(buffer);
    return time_string;
}

std::string Severity(LogLevel severity) {
    switch(severity) {
        case LogLevel::Error:
            return "Error";
        case LogLevel::Warning:
            return "Warning";
        case LogLevel::Info:
            return "Info";
        case LogLevel::Debug:
            return "Debug";
        default:
            return "Unexpected Severity";
    }
}
