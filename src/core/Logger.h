#pragma once
#include <spdlog/spdlog.h>
#include <memory>

namespace vega
{

class Logger
{
public:
    static void init();
    static std::shared_ptr<spdlog::logger>& get();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace vega

#define VEGA_LOG_TRACE(...) ::vega::Logger::get()->trace(__VA_ARGS__)
#define VEGA_LOG_DEBUG(...) ::vega::Logger::get()->debug(__VA_ARGS__)
#define VEGA_LOG_INFO(...)  ::vega::Logger::get()->info(__VA_ARGS__)
#define VEGA_LOG_WARN(...)  ::vega::Logger::get()->warn(__VA_ARGS__)
#define VEGA_LOG_ERROR(...) ::vega::Logger::get()->error(__VA_ARGS__)
#define VEGA_LOG_FATAL(...) ::vega::Logger::get()->critical(__VA_ARGS__)
