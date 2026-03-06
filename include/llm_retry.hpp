#pragma once

// llm-retry: single-header C++ retry + circuit breaker for LLM API calls
// Usage: #define LLM_RETRY_IMPLEMENTATION in ONE .cpp file before including.

#include <chrono>
#include <ctime>
#include <functional>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace llm {

// ─── Config ──────────────────────────────────────────────────────────────────

struct RetryConfig {
    int max_attempts          = 3;
    double base_delay_ms      = 500.0;
    double max_delay_ms       = 30000.0;
    double backoff_multiplier = 2.0;
    double jitter_factor      = 0.1;
    std::vector<int> retry_on_http_codes = {429, 500, 502, 503, 504};
};

struct CircuitBreakerConfig {
    int    failure_threshold  = 5;
    double success_threshold  = 0.8;
    double timeout_seconds    = 60.0;
};

// ─── Types ───────────────────────────────────────────────────────────────────

enum class CircuitState { Closed, Open, HalfOpen };

template<typename T>
struct RetryResult {
    T      value;
    int    attempts_used;
    double total_elapsed_ms;
    bool   from_fallback;
};

struct LLMError {
    int         http_code;     // 0 if not HTTP error
    std::string message;
    bool        is_retryable;
};

// ─── Helpers (declarations) ──────────────────────────────────────────────────

int parse_http_code(const std::string& response_header);

namespace detail {
    bool is_retryable_code(int code, const std::vector<int>& codes);
    double compute_delay_ms(int attempt, const RetryConfig& cfg);
} // namespace detail

// ─── with_retry ──────────────────────────────────────────────────────────────

template<typename T>
RetryResult<T> with_retry(
    std::function<T()> fn,
    const RetryConfig& config = {}
) {
    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    std::mt19937 rng(std::random_device{}());

    for (int attempt = 1; attempt <= config.max_attempts; ++attempt) {
        try {
            T result = fn();
            double elapsed = std::chrono::duration<double, std::milli>(
                clock::now() - start).count();
            return RetryResult<T>{std::move(result), attempt, elapsed, false};
        } catch (const LLMError& err) {
            bool last = (attempt == config.max_attempts);
            bool retryable = err.is_retryable &&
                             detail::is_retryable_code(err.http_code, config.retry_on_http_codes);
            if (last || !retryable) throw;

            double delay = detail::compute_delay_ms(attempt, config);
            std::uniform_real_distribution<double> jitter_dist(
                1.0 - config.jitter_factor, 1.0 + config.jitter_factor);
            delay *= jitter_dist(rng);
            std::this_thread::sleep_for(
                std::chrono::duration<double, std::milli>(delay));
        } catch (...) {
            if (attempt == config.max_attempts) throw;
            // non-LLMError: retry only if not last attempt, no http code check
            double delay = detail::compute_delay_ms(attempt, config);
            std::this_thread::sleep_for(
                std::chrono::duration<double, std::milli>(delay));
        }
    }
    throw LLMError{0, "with_retry: exhausted all attempts", false};
}

// ─── with_failover ───────────────────────────────────────────────────────────

template<typename T>
RetryResult<T> with_failover(
    std::function<T()> primary,
    std::function<T()> fallback,
    const RetryConfig& config = {}
) {
    try {
        return with_retry<T>(primary, config);
    } catch (...) {
        using clock = std::chrono::steady_clock;
        auto start = clock::now();
        T result = fallback();
        double elapsed = std::chrono::duration<double, std::milli>(
            clock::now() - start).count();
        return RetryResult<T>{std::move(result), 1, elapsed, true};
    }
}

// ─── CircuitBreaker ──────────────────────────────────────────────────────────

class CircuitBreaker {
public:
    struct Stats {
        CircuitState state;
        int          consecutive_failures;
        double       failure_rate;
        std::time_t  last_failure;
        std::time_t  last_success;
    };

    explicit CircuitBreaker(std::string name, CircuitBreakerConfig config = {})
        : name_(std::move(name)), config_(config) {}

    template<typename T>
    T call(std::function<T()> fn) {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (state_ == CircuitState::Open) {
                auto now = std::chrono::steady_clock::now();
                double secs = std::chrono::duration<double>(
                    now - last_failure_time_).count();
                if (secs >= config_.timeout_seconds) {
                    state_ = CircuitState::HalfOpen;
                } else {
                    throw std::runtime_error(
                        "CircuitBreaker[" + name_ + "]: circuit is Open");
                }
            }
        }

        try {
            T result = fn();
            record_success();
            return result;
        } catch (...) {
            record_failure();
            throw;
        }
    }

    CircuitState state() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return state_;
    }

    Stats stats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double rate = (total_calls_ == 0) ? 0.0 :
            static_cast<double>(failure_calls_) / total_calls_;
        return Stats{
            state_,
            consecutive_failures_,
            rate,
            last_failure_ts_,
            last_success_ts_
        };
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        state_ = CircuitState::Closed;
        consecutive_failures_ = 0;
        total_calls_ = 0;
        failure_calls_ = 0;
    }

private:
    void record_success() {
        std::lock_guard<std::mutex> lk(mutex_);
        ++total_calls_;
        consecutive_failures_ = 0;
        last_success_ts_ = std::time(nullptr);
        // If HalfOpen and success rate recovers, close circuit
        double rate = static_cast<double>(failure_calls_) / total_calls_;
        if (state_ == CircuitState::HalfOpen &&
            (1.0 - rate) >= config_.success_threshold) {
            state_ = CircuitState::Closed;
            failure_calls_ = 0;
            total_calls_ = 0;
        }
    }

    void record_failure() {
        std::lock_guard<std::mutex> lk(mutex_);
        ++total_calls_;
        ++failure_calls_;
        ++consecutive_failures_;
        last_failure_ts_  = std::time(nullptr);
        last_failure_time_ = std::chrono::steady_clock::now();
        if (consecutive_failures_ >= config_.failure_threshold) {
            state_ = CircuitState::Open;
        }
    }

    std::string           name_;
    CircuitBreakerConfig  config_;
    mutable std::mutex    mutex_;
    CircuitState          state_             = CircuitState::Closed;
    int                   consecutive_failures_ = 0;
    int                   total_calls_       = 0;
    int                   failure_calls_     = 0;
    std::time_t           last_failure_ts_   = 0;
    std::time_t           last_success_ts_   = 0;
    std::chrono::steady_clock::time_point last_failure_time_;
};

} // namespace llm

// ─── Implementation ──────────────────────────────────────────────────────────

#ifdef LLM_RETRY_IMPLEMENTATION

namespace llm {

int parse_http_code(const std::string& header) {
    // Expects "HTTP/1.1 429 Too Many Requests" style
    auto pos = header.find("HTTP/");
    if (pos == std::string::npos) return 0;
    auto space = header.find(' ', pos);
    if (space == std::string::npos) return 0;
    try { return std::stoi(header.substr(space + 1, 3)); }
    catch (...) { return 0; }
}

namespace detail {

bool is_retryable_code(int code, const std::vector<int>& codes) {
    if (code == 0) return true; // network error, always retry
    for (int c : codes) if (c == code) return true;
    return false;
}

double compute_delay_ms(int attempt, const RetryConfig& cfg) {
    double delay = cfg.base_delay_ms;
    for (int i = 1; i < attempt; ++i) delay *= cfg.backoff_multiplier;
    if (delay > cfg.max_delay_ms) delay = cfg.max_delay_ms;
    return delay;
}

} // namespace detail
} // namespace llm

#endif // LLM_RETRY_IMPLEMENTATION
