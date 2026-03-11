#pragma once

#include <memory>
#include <spdlog/spdlog.h>


namespace Hybrid
{
	class LogSystem final
	{
	public:

		struct Config
		{
			const char* logfile = "hybrid_engine.log";
			bool truncate_file = true; // if true, overwrite existing log file; if false, append to it

			// log level for both core and client loggers
			spdlog::level::level_enum level = spdlog::level::trace;
			spdlog::level::level_enum flush_level = spdlog::level::err;

			const char* console_pattern = "%^[%T] [tid=%t] %n: %v%$";
			const char* file_pattern = "[%Y-%m-%d %T.%e] [%l] [tid=%t] %n: %v";
		};

		static void initialize(const Config& cfg = {});
		static void shutdown();

		static std::shared_ptr<spdlog::logger>& core();
		static std::shared_ptr<spdlog::logger>& client();

	private:
		static std::shared_ptr<spdlog::logger> s_core;
		static std::shared_ptr<spdlog::logger> s_client;

	};
}


