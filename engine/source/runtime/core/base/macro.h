#pragma once
#include "runtime/core/log/log_system.h"


namespace Hybrid {
	// Core log macros
	#define HBD_CORE_TRACE(...)    ::Hybrid::LogSystem::Core()->trace(__VA_ARGS__)
	#define HBD_CORE_INFO(...)     ::Hybrid::LogSystem::Core()->info(__VA_ARGS__)
	#define HBD_CORE_WARN(...)     ::Hybrid::LogSystem::Core()->warn(__VA_ARGS__)
	#define HBD_CORE_ERROR(...)    ::Hybrid::LogSystem::Core()->error(__VA_ARGS__)
	#define HBD_CORE_CRITICAL(...) ::Hybrid::LogSystem::Core()->critical(__VA_ARGS__)

	// Client log macros
	#define HBD_TRACE(...)         ::Hybrid::LogSystem::Client()->trace(__VA_ARGS__)
	#define HBD_INFO(...)          ::Hybrid::LogSystem::Client()->info(__VA_ARGS__)
	#define HBD_WARN(...)          ::Hybrid::LogSystem::Client()->warn(__VA_ARGS__)
	#define HBD_ERROR(...)         ::Hybrid::LogSystem::Client()->error(__VA_ARGS__)
	#define HBD_CRITICAL(...)      ::Hybrid::LogSystem::Client()->critical(__VA_ARGS__)


	#define BIT(x) (1 << (x))
} // namespace Hybrid
