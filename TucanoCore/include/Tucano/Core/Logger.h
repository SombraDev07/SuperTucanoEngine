#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace Tucano {

class Logger {
public:
    static void Init();

    inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
    inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
};

} // namespace Tucano

// Core log macros
#define TUCANO_CORE_TRACE(...)    ::Tucano::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define TUCANO_CORE_INFO(...)     ::Tucano::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define TUCANO_CORE_WARN(...)     ::Tucano::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define TUCANO_CORE_ERROR(...)    ::Tucano::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define TUCANO_CORE_CRITICAL(...) ::Tucano::Logger::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define TUCANO_TRACE(...)         ::Tucano::Logger::GetClientLogger()->trace(__VA_ARGS__)
#define TUCANO_INFO(...)          ::Tucano::Logger::GetClientLogger()->info(__VA_ARGS__)
#define TUCANO_WARN(...)          ::Tucano::Logger::GetClientLogger()->warn(__VA_ARGS__)
#define TUCANO_ERROR(...)         ::Tucano::Logger::GetClientLogger()->error(__VA_ARGS__)
#define TUCANO_CRITICAL(...)      ::Tucano::Logger::GetClientLogger()->critical(__VA_ARGS__)
