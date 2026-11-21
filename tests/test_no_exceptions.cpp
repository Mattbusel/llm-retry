// Built with exceptions disabled, the way Unreal compiles plugin modules.
// It also includes the header once without the implementation and then
// again with it, the order a unity/jumbo build produces.
#include "llm_retry.hpp"
#define LLM_RETRY_IMPLEMENTATION
#include "llm_retry.hpp"

#include <cstdio>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    static_assert(LLM_RETRY_EXCEPTIONS == 0, "this test must build with exceptions off");
    llm::RetryConfig cfg;
    CHECK(llm::detail::is_retryable_code(429, cfg.retry_on_http_codes));
    CHECK(llm::detail::is_retryable_code(529, cfg.retry_on_http_codes));
    CHECK(llm::detail::is_retryable_code(0, cfg.retry_on_http_codes));
    CHECK(!llm::detail::is_retryable_code(400, cfg.retry_on_http_codes));
    CHECK(!llm::detail::is_retryable_code(200, cfg.retry_on_http_codes));
    CHECK(llm::detail::compute_delay_ms(1, cfg) == 500.0);
    CHECK(llm::detail::compute_delay_ms(3, cfg) == 2000.0);
    CHECK(llm::detail::compute_delay_ms(20, cfg) == cfg.max_delay_ms);
    CHECK(llm::parse_http_code("HTTP/1.1 429 Too Many Requests") == 429);
    CHECK(llm::parse_http_code("HTTP/1.1 abc") == 0);
    CHECK(llm::parse_http_code("garbage") == 0);
    llm::CircuitBreaker cb("x");
    CHECK(cb.state() == llm::CircuitState::Closed);
    if (failures == 0) std::printf("test_no_exceptions: all passed\n");
    return failures == 0 ? 0 : 1;
}
