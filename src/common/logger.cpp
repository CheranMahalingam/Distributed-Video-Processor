#include <string>

#include "logger.h"

Logger::Logger(LogLevel level = LogLevel::error)
    : level_(level) {}

Logger::~Logger() {
    buffer_ << std::endl;
    std::cerr << buffer_.str();
}

template <typename T> Logger& Logger::operator<<(T const& value) {
    buffer << value;
    return *this;
}
