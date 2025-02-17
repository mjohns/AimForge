#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace aim {

class Logger {
 public:
  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  static spdlog::logger* get() {
    return getInstance().logger();
  }

  spdlog::logger* logger() {
    return logger_.get();
  }

  void SetLogger(std::shared_ptr<spdlog::logger> logger) {
    logger_ = std::move(logger);
  }

  void ResetToDefault() {
    logger_ = default_logger_;
  }

  Logger(Logger const&) = delete;
  Logger operator=(Logger const&) = delete;

 private:
  Logger() {
    default_logger_ = spdlog::stdout_color_st("console");
    ResetToDefault();
  }

  std::shared_ptr<spdlog::logger> default_logger_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace aim
