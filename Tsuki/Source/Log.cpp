#include "Log.hpp"

#if TSUKI_DEBUG_VULKAN
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>

namespace tk {
std::shared_ptr<spdlog::logger> Log::_mainLogger;

void Log::Initialize() {
	auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

	// Set the format for our logs. The two formats are identical, except for the lack of color output for the file sink.
	// e.g. [13:33:12] Luna-I: [Engine] Initializing Luna Engine.
	consoleSink->set_pattern("%^[%T] %n-%L: %v%$");

	_mainLogger = std::make_shared<spdlog::logger>("Tsuki", spdlog::sinks_init_list{consoleSink});
	spdlog::register_logger(_mainLogger);

	// Default to Info level logging.
	SetLevel(Level::Info);
}

void Log::Shutdown() {
	_mainLogger.reset();
	spdlog::drop_all();
}
}  // namespace tk
#endif