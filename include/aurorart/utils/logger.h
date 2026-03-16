#ifndef AURORART_LOGGER_H
#define AURORART_LOGGER_H

#include <string>
#include <spdlog/spdlog.h>

namespace aurorart {
namespace utils {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& instance();
    
    void init(const std::string& logFile = "aurorart.log");
    void setLevel(LogLevel level);
    void flush();
    void shutdown();
    
    template<typename... Args>
    void trace(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void critical(const std::string& format, Args&&... args);
    
private:
    Logger() = default;
    std::shared_ptr<spdlog::logger> logger_;
};

// 模板实现
template<typename... Args>
void Logger::trace(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->trace(format, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Logger::debug(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->debug(format, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Logger::info(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->info(format, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Logger::warn(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->warn(format, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Logger::error(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->error(format, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Logger::critical(const std::string& format, Args&&... args) {
    if (logger_) {
        logger_->critical(format, std::forward<Args>(args)...);
    }
}

// 便捷宏
#define AURORA_LOG_TRACE(...) ::aurorart::utils::Logger::instance().trace(__VA_ARGS__)
#define AURORA_LOG_DEBUG(...) ::aurorart::utils::Logger::instance().debug(__VA_ARGS__)
#define AURORA_LOG_INFO(...) ::aurorart::utils::Logger::instance().info(__VA_ARGS__)
#define AURORA_LOG_WARN(...) ::aurorart::utils::Logger::instance().warn(__VA_ARGS__)
#define AURORA_LOG_ERROR(...) ::aurorart::utils::Logger::instance().error(__VA_ARGS__)
#define AURORA_LOG_CRITICAL(...) ::aurorart::utils::Logger::instance().critical(__VA_ARGS__)

} // namespace utils
} // namespace aurorart

#endif // AURORART_LOGGER_H