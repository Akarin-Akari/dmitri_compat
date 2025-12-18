#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <ctime>
#include <iomanip>

namespace DmitriCompat {

enum class LogLevel {
    None = 0,
    Error = 1,
    Info = 2,
    Verbose = 3
};

class Logger {
public:
    static Logger& GetInstance();

    void Initialize(const std::string& logPath, LogLevel level);
    void SetLevel(LogLevel level);

    void Log(LogLevel level, const char* format, ...);
    void Error(const char* format, ...);
    void Info(const char* format, ...);
    void Verbose(const char* format, ...);

    void Flush();
    void Shutdown();

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void WriteLog(LogLevel level, const std::string& message);
    std::string GetTimestamp();
    const char* GetLevelString(LogLevel level);

    std::ofstream logFile_;
    LogLevel logLevel_ = LogLevel::Info;
    std::mutex mutex_;
    bool initialized_ = false;
};

// 便捷宏
#define LOG_ERROR(...) DmitriCompat::Logger::GetInstance().Error(__VA_ARGS__)
#define LOG_INFO(...) DmitriCompat::Logger::GetInstance().Info(__VA_ARGS__)
#define LOG_VERBOSE(...) DmitriCompat::Logger::GetInstance().Verbose(__VA_ARGS__)

} // namespace DmitriCompat
