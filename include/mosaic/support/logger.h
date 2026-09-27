#pragma once

#include <ostream>

#include <utl/function_view.hpp>

#include <mosaic/support/export.h>

namespace mosaic {

/// Static logger that provides several "severity" modes
class MOSAIC_EXPORT Logger {
public:
    enum class Level { Trace, Info, Debug, Warning, Error, Fatal };

    /// Functions for different severity levels @{
    static void trace(auto const&... args) noexcept {
        do_log(Level::Trace, args...);
    }
    static void info(auto const&... args) noexcept {
        do_log(Level::Info, args...);
    }
    static void debug(auto const&... args) noexcept {
        do_log(Level::Debug, args...);
    }
    static void warn(auto const&... args) noexcept {
        do_log(Level::Warning, args...);
    }
    static void error(auto const&... args) noexcept {
        do_log(Level::Error, args...);
    }
    static void fatal(auto const&... args) noexcept {
        do_log(Level::Fatal, args...);
    }
    /// @}

    /// Customization point
    struct Impl {
        virtual ~Impl() = default;
        virtual void begin_log(Level level) = 0;
        virtual void end_log() = 0;
        virtual std::ostream& get_ostream() = 0;
    };

    /// Executes \p callback under the global logger lock
    static void do_locked(utl::function_view<void()> callback);

private:
    struct LogGuard {
        ~LogGuard() {
            Logger::end_log();
        }
    };

    static void do_log(Level level, auto const&... args) noexcept {
        begin_log(level);
        LogGuard guard;
        (..., (def_ostream() << args));
        (..., (user_ostream() << args));
    }

    static void begin_log(Level level);

    static void end_log();

    static std::ostream& def_ostream();
    static std::ostream& user_ostream();
};

/// Users can replace this function to return their own logger implementation
__attribute__((weak)) std::unique_ptr<Logger::Impl> make_global_logger_impl();

} // namespace mosaic
