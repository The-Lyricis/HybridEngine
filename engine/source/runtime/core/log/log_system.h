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

			const char* console_pattern = "%^[%T] %n: %v%$";
			const char* file_pattern = "[%Y-%m-%d %T.%e] [%l] %n: %v";
		};

		static void Init(const Config& cfg = {});
		static void Shutdown();

		static std::shared_ptr<spdlog::logger>& Core();
		static std::shared_ptr<spdlog::logger>& Client();

	private:
		static std::shared_ptr<spdlog::logger> s_core;
		static std::shared_ptr<spdlog::logger> s_client;

	};
}


