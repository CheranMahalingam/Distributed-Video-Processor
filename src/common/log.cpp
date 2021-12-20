#include "log.h"

Log::Log(LogLevel level) : level_(level) {}

Log::~Log() {
    LogBuffer::Logger().Write(level_, buffer_.str());
}

LogBuffer& LogBuffer::Logger() {
    static LogBuffer instance;
    return instance;
}

void LogBuffer::Write(LogLevel severity, std::string message) {
    std::lock_guard<std::mutex> lockGuard(log_mutex_);
    std::cout << Timestamp() << " " << Severity(severity) << ": " << message << std::endl;
}

LogBuffer::LogBuffer() {}

std::string LogBuffer::Timestamp() {
    std::time_t raw_time;
    std::time(&raw_time);
    std::tm* curr_time = std::gmtime(&raw_time);
    char buffer[30];
    std::strftime(buffer, 30, "%Y-%m-%d %H:%M:%S %Z", curr_time);
    std::string time_string(buffer);
    return time_string;
}

std::string LogBuffer::Severity(LogLevel severity) {
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
