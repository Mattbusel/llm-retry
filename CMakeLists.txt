cmake_minimum_required(VERSION 3.16)
project(llm_retry_examples CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(EXAMPLE basic_retry circuit_breaker)
    add_executable(${EXAMPLE} examples/${EXAMPLE}.cpp)
    target_include_directories(${EXAMPLE} PRIVATE include)
    target_compile_options(${EXAMPLE} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
    )
endforeach()

enable_testing()
add_executable(test_llm_retry tests/test_llm_retry.cpp)
target_include_directories(test_llm_retry PRIVATE include)
target_compile_options(test_llm_retry PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
find_package(Threads REQUIRED)
target_link_libraries(test_llm_retry PRIVATE Threads::Threads)
add_test(NAME test_llm_retry COMMAND test_llm_retry)

# The header must also compile with exceptions off (Unreal, -fno-exceptions).
add_executable(test_no_exceptions tests/test_no_exceptions.cpp)
target_include_directories(test_no_exceptions PRIVATE include)
if(MSVC)
    target_compile_options(test_no_exceptions PRIVATE /EHs-c- /W4)
    target_compile_definitions(test_no_exceptions PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(test_no_exceptions PRIVATE -fno-exceptions -Wall -Wextra)
endif()
add_test(NAME test_no_exceptions COMMAND test_no_exceptions)
