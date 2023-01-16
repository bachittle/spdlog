// make attributes conform to spdlog requirements (thread-safe, exception-safe, etc)
#include "includes.h"
#include "test_sink.h"
#include <string>
#include <thread>

#define TEST_FILENAME "test_logs/attr_test.log"

// see if multiple async logs to a single file is thread-safe, i.e. produces coherent structured logs
TEST_CASE("async logfmt test ", "[attributes]")
{
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_mt>();

    constexpr int num_loggers = 3;
    constexpr int num_msgs = 100;
    size_t overrun_counter = 0;

    {
    auto tp = std::make_shared<spdlog::details::thread_pool>(num_msgs, 10);
    std::vector<std::shared_ptr<spdlog::logger>> loggers;
    for (int i = 0; i < num_loggers; ++i) {
        loggers.push_back(std::make_shared<spdlog::async_logger>(
            "attr_logger_"+std::to_string(i), test_sink, tp, spdlog::async_overflow_policy::block));
    }

    #if 0
    std::string logfmt_pattern = "time=%Y-%m-%dT%H:%M:%S.%f%z name=\"%n\" level=%^%l%$ process=%P thread=%t message=\"%v\" ";
    for (auto& lg : loggers) {
        lg->set_pattern(logfmt_pattern);
    }
    #endif

    std::vector<std::thread> threads;
    for (int i = 0; i < num_msgs; ++i) {
        for (auto& lg : loggers) {
            threads.emplace_back([&](){
                // push and pop context are not guaranteed to be thread safe
                // so pushing and popping of contexts are isolated for individual logger objects (so no sharing)
                lg->push_context({{"key_"+std::to_string(i), "val_"+std::to_string(i)}});
                lg->info("testing {}", i);
                lg->pop_context();
            });
        }
    }
    for (auto& th : threads) {
        th.join();
    }

    for (auto& lg : loggers) {
        lg->flush();
    }
    overrun_counter += tp->overrun_counter();
    }

    REQUIRE(test_sink->msg_counter() == num_loggers * num_msgs);
    REQUIRE(test_sink->flush_counter() == num_loggers);
    REQUIRE(overrun_counter == 0);

    // todo: parse logfmt or json, make a utils.cpp function to parse data from test file or buffer
}