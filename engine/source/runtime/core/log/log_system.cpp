#include "log_system.h"

#include <vector>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Hybrid
{
	std::shared_ptr<spdlog::logger> LogSystem::s_core;
	std::shared_ptr<spdlog::logger> LogSystem::s_client;

	void LogSystem::Init(const Config& cfg)
	{
		if (s_core && s_client) // guard: already initialized
			return;

		std::vector<spdlog::sink_ptr> sinks;

		// console sink
		sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		// file sink
		sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(cfg.logfile, cfg.truncate_file));

		sinks[0]->set_pattern(cfg.console_pattern);
		sinks[1]->set_pattern(cfg.file_pattern);

		s_core = std::make_shared<spdlog::logger>("HYBRID_CORE", sinks.begin(), sinks.end());
		s_client = std::make_shared<spdlog::logger>("HYBRID_CLIENT", sinks.begin(), sinks.end());

		// register loggers
		spdlog::register_logger(s_core);
		spdlog::register_logger(s_client);

		// set log levels
		s_core->set_level(cfg.level);
		s_client->set_level(cfg.level);

		// set flush levels, flush means writing log messages to their targets immediately
		s_core->flush_on(cfg.flush_level);
		s_client->flush_on(cfg.flush_level);
	}
	void LogSystem::Shutdown()
	{
		if (!s_core && !s_client)
			return;

		if (s_core)   spdlog::drop("HYBRID_CORE");
		if (s_client) spdlog::drop("HYBRID_CLIENT");

		s_core.reset();
		s_client.reset();
	}
	std::shared_ptr<spdlog::logger>& LogSystem::Core()
	{
		return s_core;
	}
	std::shared_ptr<spdlog::logger>& LogSystem::Client()
	{
		return s_client;
	}
} // namespace Hybrid