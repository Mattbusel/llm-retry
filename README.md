# llm-retry

Retry + circuit breaker for C++ LLM API calls. Single header, no deps.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![License MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Single Header](https://img.shields.io/badge/single-header-orange.svg)

## Quickstart

```cpp
#define LLM_RETRY_IMPLEMENTATION
#include "llm_retry.hpp"

llm::RetryConfig cfg;
cfg.max_attempts  = 4;
cfg.base_delay_ms = 500.0;

auto result = llm::with_retry<std::string>([&]() {
    return call_my_llm_api();
}, cfg);

std::cout << result.value << "\n";
std::cout << "Took " << result.attempts_used << " attempt(s)\n";
```

## Installation

Copy `include/llm_retry.hpp` into your project. No other files needed.
No external dependencies — pure C++17 stdlib.

## API

### RetryConfig
```cpp
struct RetryConfig {
    int max_attempts          = 3;
    double base_delay_ms      = 500.0;
    double max_delay_ms       = 30000.0;
    double backoff_multiplier = 2.0;
    double jitter_factor      = 0.1;       // +-10% random jitter
    std::vector<int> retry_on_http_codes = {429, 500, 502, 503, 504};
};
```

### with_retry
```cpp
template<typename T>
RetryResult<T> llm::with_retry(std::function<T()> fn, const RetryConfig& config = {});
```
Retries `fn` on `LLMError` with retryable HTTP codes. Exponential backoff + jitter.

### with_failover
```cpp
template<typename T>
RetryResult<T> llm::with_failover(
    std::function<T()> primary,
    std::function<T()> fallback,
    const RetryConfig& config = {}
);
```
Tries `primary` with full retry. If all attempts fail, calls `fallback` once.
`RetryResult::from_fallback` is `true` if fallback was used.

### CircuitBreaker
```cpp
llm::CircuitBreaker breaker("openai", cfg);

auto response = breaker.call<std::string>([&]() {
    return call_openai();
});

breaker.state();  // Closed / Open / HalfOpen
breaker.stats();  // consecutive_failures, failure_rate, last_failure, last_success
breaker.reset();  // manually close the circuit
```

State machine: `Closed -> Open` (after N consecutive failures) `-> HalfOpen` (after timeout) `-> Closed/Open`.

### LLMError
```cpp
struct LLMError {
    int         http_code;    // 0 = network error
    std::string message;
    bool        is_retryable;
};
```
Throw this from your `fn` lambda. `with_retry` catches it and decides whether to retry.

## Examples

- [`examples/basic_retry.cpp`](examples/basic_retry.cpp) — retry a flaky call that fails twice
- [`examples/circuit_breaker.cpp`](examples/circuit_breaker.cpp) — drive a circuit open, observe fast-fail, reset

## Building Examples

```bash
cmake -B build && cmake --build build
./build/basic_retry
./build/circuit_breaker
```

## Requirements

C++17. No external dependencies.

## License

MIT — see [LICENSE](LICENSE).
