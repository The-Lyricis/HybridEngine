#pragma once
#include "runtime/core/log/log_system.h"


namespace Hybrid {
	// Core log macros
	#define HBD_CORE_TRACE(...)    ::Hybrid::LogSystem::core()->trace(__VA_ARGS__)
	#define HBD_CORE_INFO(...)     ::Hybrid::LogSystem::core()->info(__VA_ARGS__)
	#define HBD_CORE_WARN(...)     ::Hybrid::LogSystem::core()->warn(__VA_ARGS__)
	#define HBD_CORE_ERROR(...)    ::Hybrid::LogSystem::core()->error(__VA_ARGS__)
	#define HBD_CORE_CRITICAL(...) ::Hybrid::LogSystem::core()->critical(__VA_ARGS__)

	// Client log macros
	#define HBD_TRACE(...)         ::Hybrid::LogSystem::client()->trace(__VA_ARGS__)
	#define HBD_INFO(...)          ::Hybrid::LogSystem::client()->info(__VA_ARGS__)
	#define HBD_WARN(...)          ::Hybrid::LogSystem::client()->warn(__VA_ARGS__)
	#define HBD_ERROR(...)         ::Hybrid::LogSystem::client()->error(__VA_ARGS__)
	#define HBD_CRITICAL(...)      ::Hybrid::LogSystem::client()->critical(__VA_ARGS__)


	#define BIT(x) (1 << (x))
} // namespace Hybrid
