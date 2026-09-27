#include <support/logger.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>

#include <termfmt/termfmt.h>
#include <utl/format_time.hpp>
#include <utl/streammanip.hpp>

using namespace mosaic;

static void announce_message(std::ostream& str, Logger::Level level) {
    using enum Logger::Level;
    switch (level) {
    case Trace:
        break;
    case Info:
        str << "Info: ";
        break;
    case Debug:
        str << "Debug: ";
        break;
    case Warning:
        str << "Warning: ";
        break;
    case Error:
        str << "Error: ";
        break;
    case Fatal:
        str << "Fatal: ";
        break;
    }
}

static tfmt::Modifier const& mod(Logger::Level level) {
    using namespace tfmt::modifiers;
    static tfmt::Modifier const mods[] = {
        /* Trace:   */ None,
        /* Info:    */ BrightGreen,
        /* Debug:   */ BrightBlue,
        /* Warning: */ Yellow | Bold,
        /* Error:   */ Red | Bold,
        /* Fatal:   */ BGRed | BrightWhite | Bold,
    };
    return mods[(size_t)level];
};

namespace {

struct LoggerOptions {
    bool timestamps;
};

LoggerOptions const options = [] {
    /// TODO: Implement logger control through environment variables
    return LoggerOptions{.timestamps = true};
}();

} // namespace

static void formatTime(std::ostream& str) {
    // FIXME: This does not compute current daytime correctly
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    auto dur = now.time_since_epoch();
    dur -= duration_cast<days>(dur);
    auto h = duration_cast<hours>(dur);
    dur -= h;
    auto min = duration_cast<minutes>(dur);
    dur -= min;
    auto sec = duration_cast<seconds>(dur);
    dur -= sec;
    auto mil = duration_cast<milliseconds>(dur);
    dur -= mil;
    str << "[" << std::setfill(' ') << std::setw(2) << h.count()
        << "h:" << std::setfill(' ') << std::setw(2) << min.count()
        << "m:" << std::setfill(' ') << std::setw(2) << sec.count()
        << "s:" << std::setfill(' ') << std::setw(3) << mil.count() << "ms] ";
}

namespace {

struct TerminalLogger : Logger::Impl {
    void begin_log(Logger::Level level) final {
        tfmt::pushModifier(mod(level), std::cout);
        if (options.timestamps) {
            formatTime(std::cout);
        }
        if (!tfmt::isTermFormattable(std::cout)) {
            announce_message(std::cout, level);
        }
    }

    void end_log() final {
        tfmt::popModifier(std::cout);
        std::cout << "\n";
    }

    std::ostream& get_ostream() final {
        return std::cout;
    }
};

struct NullLogger : Logger::Impl {
    void begin_log(Logger::Level) final {}
    void end_log() final {}
    std::ostream& get_ostream() final {
        return f;
    }
    std::fstream f;
};

} // namespace

static std::mutex& g_logger_mutex() {
    static std::mutex m;
    return m;
}

TerminalLogger& term_logger() {
    static TerminalLogger l;
    return l;
}

static Logger::Impl& user_logger() {
    static auto const l = make_global_logger_impl();
    return *l;
}

void Logger::do_locked(utl::function_view<void()> callback) {
    std::lock_guard lock(g_logger_mutex());
    callback();
}

void Logger::begin_log(Level level) {
    /// We use RAII to invoke `end_log()` which unlocks the mutex so no need to
    /// use `lock_guard` or `unique_lock` here.
    /// We need the lock because
    ///  - this way messages from different threads won't be interleaved
    ///  - otherwise pushing and popping `tfmt` modifiers from different threads
    ///    does not work correctly
    g_logger_mutex().lock();
    term_logger().begin_log(level);
    user_logger().begin_log(level);
}

void Logger::end_log() {
    term_logger().end_log();
    user_logger().end_log();
    g_logger_mutex().unlock();
}

std::ostream& Logger::def_ostream() {
    return term_logger().get_ostream();
}

std::ostream& Logger::user_ostream() {
    return user_logger().get_ostream();
}

std::unique_ptr<Logger::Impl> mosaic::make_global_logger_impl() {
    return std::make_unique<NullLogger>();
}