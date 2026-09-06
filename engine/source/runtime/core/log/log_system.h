#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>


namespace Hybrid
{
	struct LogEntry
	{
		uint64_t sequence = 0;
		std::chrono::system_clock::time_point timestamp{};
		spdlog::level::level_enum level = spdlog::level::info;
		std::string logger;
		uint64_t thread_id = 0;
		std::string message;
	};

	struct LogBufferSnapshot
	{
		std::vector<LogEntry> entries;
		uint64_t next_sequence = 1;
	};

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
			size_t buffered_entries = 4096;

			const char* console_pattern = "%^[%T] [tid=%t] %n: %v%$";
			const char* file_pattern = "[%Y-%m-%d %T.%e] [%l] [tid=%t] %n: %v";
		};

		static void initialize(const Config& cfg = {});
		static void shutdown();

		static std::shared_ptr<spdlog::logger>& core();
		static std::shared_ptr<spdlog::logger>& client();
		static LogBufferSnapshot bufferedEntries();
		static void clearBufferedEntries();

	private:
		static std::shared_ptr<spdlog::logger> s_core;
		static std::shared_ptr<spdlog::logger> s_client;

	};
}


