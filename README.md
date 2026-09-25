# llm-retry

[![CI](https://github.com/Mattbusel/llm-retry/actions/workflows/ci.yml/badge.svg)](https://github.com/Mattbusel/llm-retry/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**Retries, failover and a circuit breaker for LLM API calls.** One header, `llm_retry.hpp`. Needs nothing beyond the C++17 standard library.

LLM APIs fail in ordinary ways: 429 rate limits, 5xx overloads, dropped connections. Handling that well means exponential backoff with jitter, knowing which errors are worth retrying, falling back to another provider and not hammering a service that is down. llm-retry wraps any callable with those policies. It knows nothing about HTTP clients, so it works with any of them.

## Features

- `with_retry<T>(fn, cfg)`: exponential backoff with jitter, max attempts and max delay
- Retries `LLMError`s whose HTTP code is in `retry_on_http_codes` (default 429, 500, 502, 503, 504) or is 0 (network error); other exceptions are retried until the last attempt
- `with_failover<T>(primary, fallback, cfg)`: retry the primary, then call the fallback once
- `CircuitBreaker`: opens after N consecutive failures, rejects calls while open, half-opens after a timeout; thread safe
- `RetryResult<T>` reports attempts used, elapsed time and whether the fallback answered
- Standard library only

## Quick start

Copy the header into your project:

```bash
curl -fsSLO https://raw.githubusercontent.com/Mattbusel/llm-retry/main/include/llm_retry.hpp
```

Define `LLM_RETRY_IMPLEMENTATION` in exactly one `.cpp` file before including it; every other file just includes the header. Save this as `main.cpp` next to the header:

```cpp
#define LLM_RETRY_IMPLEMENTATION
#include "llm_retry.hpp"
#include <iostream>

std::string call_model() {
    static int n = 0;
    if (++n < 3) throw llm::LLMError{429, "rate limited", true};   // retryable
    return "hello from attempt " + std::to_string(n);
}

int main() {
    llm::RetryConfig cfg;
    cfg.max_attempts  = 5;
    cfg.base_delay_ms = 200;   // 200, 400, 800 ms ... with +/-10% jitter

    auto r = llm::with_retry<std::string>(call_model, cfg);
    std::cout << r.value << " (" << r.attempts_used << " attempts, "
              << r.total_elapsed_ms << " ms)\n";

    // Primary provider with a fallback.
    auto f = llm::with_failover<std::string>(
        []() -> std::string { throw llm::LLMError{503, "down", true}; },
        []() -> std::string { return "answer from backup model"; }, cfg);
    std::cout << f.value << (f.from_fallback ? " [fallback]" : "") << "\n";

    // Circuit breaker: opens after 5 consecutive failures, half-opens after 60 s.
    llm::CircuitBreaker breaker("openai");
    std::string out = breaker.call<std::string>([]() -> std::string { return "ok"; });
    std::cout << out << ", failure rate " << breaker.stats().failure_rate << "\n";
}
```

```bash
g++ -std=c++17 -O2 main.cpp -o demo
```

## API at a glance

| Call | Purpose |
|---|---|
| `with_retry<T>(fn, RetryConfig)` | Retry a callable returning `T` |
| `with_failover<T>(primary, fallback, RetryConfig)` | Retry, then fall back |
| `CircuitBreaker(name, CircuitBreakerConfig)` | `call<T>(fn)`, `state()`, `stats()`, `reset()` |
| `LLMError{http_code, message, is_retryable}` | Throw this from your call to control retry decisions |
| `parse_http_code(header)` | Pull the status code out of an HTTP status line |

## Notes and limitations

- Delays use `std::this_thread::sleep_for`, so `with_retry` blocks the calling thread.
- An open circuit throws `std::runtime_error`; catch it to serve a cached or degraded answer.

## Build the examples

The repo builds `examples/basic_retry.cpp`, `examples/circuit_breaker.cpp` with CMake:

```bash
cmake -B build
cmake --build build
```

## Part of llm-cpp

llm-retry is one of 26 single-header C++ libraries in [llm-cpp](https://github.com/Mattbusel/llm-cpp), a toolkit for building LLM features into native code. Each library stands alone; combine them by giving each `*_IMPLEMENTATION` define its own `.cpp` file. See the [llm-cpp README](https://github.com/Mattbusel/llm-cpp#using-several-together) for the full list and examples of using several together.

## License

MIT. See [LICENSE](LICENSE).
