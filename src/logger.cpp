#include "logger.h"
#include <cstdarg>
#include <windows.h>

namespace DmitriCompat {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::string& logPath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    logLevel_ = level;

    if (level == LogLevel::None) {
        return;
    }

    logFile_.open(logPath, std::ios::out | std::ios::app);
    if (logFile_.is_open()) {
        initialized_ = true;
        logFile_ << "\n========================================\n";
        logFile_ << "DmitriCompat Hook Initialized\n";
        logFile_ << "Time: " << GetTimestamp() << "\n";
        logFile_ << "========================================\n";
        logFile_.flush();
    }
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

void Logger::Log(LogLevel level, const char* format, ...) {
    if (level > logLevel_ || logLevel_ == LogLevel::None) {
        return;
    }

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    WriteLog(level, buffer);
}

void Logger::Error(const char* format, ...) {
    if (LogLevel::Error > logLevel_ || logLevel_ == LogLevel::None) {
        return;
    }

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    WriteLog(LogLevel::Error, buffer);
}

void Logger::Info(const char* format, ...) {
    if (LogLevel::Info > logLevel_ || logLevel_ == LogLevel::None) {
        return;
    }

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    WriteLog(LogLevel::Info, buffer);
}

void Logger::Verbose(const char* format, ...) {
    if (LogLevel::Verbose > logLevel_ || logLevel_ == LogLevel::None) {
        return;
    }

    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    WriteLog(LogLevel::Verbose, buffer);
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.flush();
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return;
    }

    if (logFile_.is_open()) {
        logFile_ << "\n========================================\n";
        logFile_ << "DmitriCompat Hook Shutdown\n";
        logFile_ << "Time: " << GetTimestamp() << "\n";
        logFile_ << "========================================\n\n";
        logFile_.close();
    }

    initialized_ = false;
}

Logger::~Logger() {
    Shutdown();
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !logFile_.is_open()) {
        return;
    }

    logFile_ << "[" << GetTimestamp() << "] "
             << "[" << GetLevelString(level) << "] "
             << message << "\n";

    // 错误级别立即刷新
    if (level == LogLevel::Error) {
        logFile_.flush();
    }
}

std::string Logger::GetTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear << "-"
        << std::setw(2) << st.wMonth << "-"
        << std::setw(2) << st.wDay << " "
        << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds;

    return oss.str();
}

const char* Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Verbose: return "VERB ";
        default: return "???? ";
    }
}

} // namespace DmitriCompat
