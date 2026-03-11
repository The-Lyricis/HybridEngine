#pragma once
#include "runtime/core/log/log_system.h"


namespace Hybrid {
	// Core log macros
	#define HBD_CORE_TRACE(...)    do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->trace(__VA_ARGS__); } while (0)
	#define HBD_CORE_DEBUG(...)    do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->debug(__VA_ARGS__); } while (0)
	#define HBD_CORE_INFO(...)     do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->info(__VA_ARGS__); } while (0)
	#define HBD_CORE_WARN(...)     do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->warn(__VA_ARGS__); } while (0)
	#define HBD_CORE_ERROR(...)    do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->error(__VA_ARGS__); } while (0)
	#define HBD_CORE_CRITICAL(...) do { auto _logger = ::Hybrid::LogSystem::core(); if (_logger) _logger->critical(__VA_ARGS__); } while (0)

	// Client log macros
	#define HBD_TRACE(...)         do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->trace(__VA_ARGS__); } while (0)
	#define HBD_DEBUG(...)         do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->debug(__VA_ARGS__); } while (0)
	#define HBD_INFO(...)          do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->info(__VA_ARGS__); } while (0)
	#define HBD_WARN(...)          do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->warn(__VA_ARGS__); } while (0)
	#define HBD_ERROR(...)         do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->error(__VA_ARGS__); } while (0)
	#define HBD_CRITICAL(...)      do { auto _logger = ::Hybrid::LogSystem::client(); if (_logger) _logger->critical(__VA_ARGS__); } while (0)


	#define BIT(x) (1 << (x))
} // namespace Hybrid
