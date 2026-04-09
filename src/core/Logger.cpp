#include "core/Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace vega
{

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("vega.log", true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");

    s_logger = std::make_shared<spdlog::logger>(
        "vega", spdlog::sinks_init_list{console_sink, file_sink});

#ifdef _DEBUG
    s_logger->set_level(spdlog::level::trace);
#else
    s_logger->set_level(spdlog::level::info);
#endif

    s_logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(s_logger);
}

std::shared_ptr<spdlog::logger>& Logger::get()
{
    if (!s_logger) init();
    return s_logger;
}

} // namespace vega
