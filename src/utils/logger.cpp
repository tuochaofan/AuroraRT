#include "aurorart/utils/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace aurorart {
namespace utils {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& logFile) {
    // 创建控制台和文件双输出的sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    // 使用滚动文件sink，避免日志文件过大
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, 
        1048576 * 5, // 5MB per file
        10           // 最多10个文件
    );
    
    // 设置日志格式
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
    
    // 创建logger
    logger_ = std::make_shared<spdlog::logger>("aurorart", spdlog::sinks_init_list{console_sink, file_sink});
    
    // 设置默认日志级别
    logger_->set_level(spdlog::level::info);
    
    // 注册为默认logger
    spdlog::set_default_logger(logger_);
    
    // 启用自动刷新
    logger_->flush_on(spdlog::level::info);
}

void Logger::setLevel(LogLevel level) {
    if (!logger_) return;
    
    switch (level) {
    case LogLevel::TRACE:
        logger_->set_level(spdlog::level::trace);
        break;
    case LogLevel::DEBUG:
        logger_->set_level(spdlog::level::debug);
        break;
    case LogLevel::INFO:
        logger_->set_level(spdlog::level::info);
        break;
    case LogLevel::WARN:
        logger_->set_level(spdlog::level::warn);
        break;
    case LogLevel::ERROR:
        logger_->set_level(spdlog::level::err);
        break;
    case LogLevel::CRITICAL:
        logger_->set_level(spdlog::level::critical);
        break;
    }
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::shutdown() {
    spdlog::shutdown();
}

} // namespace utils
} // namespace aurorart