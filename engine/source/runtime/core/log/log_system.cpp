#include "log_system.h"

#include <deque>
#include <mutex>
#include <vector>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>

namespace Hybrid
{
	namespace
	{
		class BufferedLogSink final : public spdlog::sinks::base_sink<std::mutex>
		{
		public:
			explicit BufferedLogSink(size_t capacity) : m_capacity(capacity) {}

			LogBufferSnapshot snapshot()
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				LogBufferSnapshot result{};
				result.entries.assign(m_entries.begin(), m_entries.end());
				result.next_sequence = m_next_sequence;
				return result;
			}

			void clear()
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				m_entries.clear();
			}

		protected:
			void sink_it_(const spdlog::details::log_msg& message) override
			{
				if (m_capacity == 0)
					return;

				LogEntry entry{};
				entry.sequence = m_next_sequence++;
				entry.timestamp = message.time;
				entry.level = message.level;
				entry.logger.assign(message.logger_name.data(), message.logger_name.size());
				entry.thread_id = static_cast<uint64_t>(message.thread_id);
				entry.message.assign(message.payload.data(), message.payload.size());
				m_entries.push_back(std::move(entry));
				while (m_entries.size() > m_capacity)
					m_entries.pop_front();
			}

			void flush_() override {}

		private:
			size_t m_capacity = 4096;
			uint64_t m_next_sequence = 1;
			std::deque<LogEntry> m_entries;
		};

		std::shared_ptr<BufferedLogSink> s_buffered_sink;
	}

	std::shared_ptr<spdlog::logger> LogSystem::s_core;
	std::shared_ptr<spdlog::logger> LogSystem::s_client;

	void LogSystem::initialize(const Config& cfg)
	{
		if (s_core && s_client) // guard: already initialized
			return;

		std::vector<spdlog::sink_ptr> sinks;

		// console sink
		sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		// file sink
		sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(cfg.logfile, cfg.truncate_file));
		s_buffered_sink = std::make_shared<BufferedLogSink>(cfg.buffered_entries);
		sinks.emplace_back(s_buffered_sink);

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
	void LogSystem::shutdown()
	{
		if (!s_core && !s_client)
			return;

		if (s_core)   spdlog::drop("HYBRID_CORE");
		if (s_client) spdlog::drop("HYBRID_CLIENT");

		s_core.reset();
		s_client.reset();
		s_buffered_sink.reset();
	}
	std::shared_ptr<spdlog::logger>& LogSystem::core()
	{
		return s_core;
	}
	std::shared_ptr<spdlog::logger>& LogSystem::client()
	{
		return s_client;
	}

	LogBufferSnapshot LogSystem::bufferedEntries()
	{
		return s_buffered_sink ? s_buffered_sink->snapshot() : LogBufferSnapshot{};
	}

	void LogSystem::clearBufferedEntries()
	{
		if (s_buffered_sink)
			s_buffered_sink->clear();
	}
} // namespace Hybrid
