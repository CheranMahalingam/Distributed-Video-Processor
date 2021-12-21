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
    auto current_time = std::chrono::system_clock::now();
    char buffer[80];
    auto transformed = current_time.time_since_epoch().count()/1000000;
    auto millis = transformed%1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(current_time);
    auto time_info = std::localtime(&tt);
    std::strftime(buffer, 80, "%F %H:%M:%S", time_info);
    std::sprintf(buffer, "%s.%03d", buffer, (int)millis);
    std::string time_string(buffer);
    return time_string;
}

std::string LogBuffer::Severity(LogLevel severity) {
    switch(severity) {
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Debug:
            return "DEBUG";
        default:
            return "UNEXPECTED SEVERITY";
    }
}
