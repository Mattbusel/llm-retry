# AGENTS.md — llm-retry

## Purpose
Single-header C++ retry + circuit breaker for LLM API calls.
Wraps any callable with exponential backoff, jitter, and circuit breaking.

## Architecture
Everything lives in `include/llm_retry.hpp`. No source files.
Implementation guard: `#ifdef LLM_RETRY_IMPLEMENTATION`.

## Build & Test
```bash
cmake -B build && cmake --build build
cd build && ctest
```

## Constraints
- Single header only
- No external dependencies (pure C++17 stdlib)
- namespace `llm` for all public API
- Thread-safe CircuitBreaker via std::mutex
- No exceptions in hot path — LLMError carries error info
