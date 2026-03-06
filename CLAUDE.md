# CLAUDE.md — llm-retry

## Build
```bash
cmake -B build && cmake --build build
```

## Run examples
```bash
./build/basic_retry
./build/circuit_breaker
```

## THE RULE: Single Header
`include/llm_retry.hpp` is the entire library. Never split it.

## API to maintain
- `with_retry<T>(fn, config)` -> `RetryResult<T>`
- `with_failover<T>(primary, fallback, config)` -> `RetryResult<T>`
- `CircuitBreaker::call<T>(fn)` -> `T` or throws
- `CircuitBreaker::state()`, `stats()`, `reset()`
- `parse_http_code(header)` -> int

## Common mistakes
- Forgetting `#define LLM_RETRY_IMPLEMENTATION` in exactly one TU
- Calling `sleep_for` with wrong units — always use `std::chrono::duration<double, std::milli>`
- Not holding mutex when reading circuit state
