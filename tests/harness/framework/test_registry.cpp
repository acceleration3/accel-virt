#include "harness/framework/test_registry.hpp"

#include <pthread.h>

#include "core/platform/platform.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace av {

TestCase::TestCase(const char *suite, const char *name, Body body)
    : suite_(suite), name_(name), body_(body), next_(nullptr)
{
    TestRegistry::instance().add(this);
}

const char *TestCase::suite() const { return suite_; }
const char *TestCase::name() const  { return name_; }
void        TestCase::run() const   { body_(); }
TestCase   *TestCase::next() const  { return next_; }
void        TestCase::set_next(TestCase *next) { next_ = next; }

TestRegistry::TestRegistry() : head_(nullptr), failures_(0), current_failed_(false) {}

TestRegistry &TestRegistry::instance()
{
    static TestRegistry registry;
    return registry;
}

void TestRegistry::add(TestCase *test)
{
    test->set_next(head_);
    head_ = test;
}

void TestRegistry::record_failure(const char *file, int line, const char *format, ...)
{
    std::va_list args;
    std::fprintf(stderr, "\n    %s:%d: ", file, line);
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fprintf(stderr, "\n");
    current_failed_ = true;
}

bool TestRegistry::take_current_failure()
{
    const bool failed = current_failed_;
    current_failed_ = false;
    return failed;
}

namespace {

constexpr std::size_t test_stack_bytes = std::size_t{512} << 20u;

void *run_case(void *test)
{
    static_cast<const TestCase *>(test)->run();
    return nullptr;
}

void run_on_wide_stack(const TestCase &test)
{
    pthread_attr_t attributes;
    pthread_t      thread;
    if (pthread_attr_init(&attributes) != 0 || pthread_attr_setstacksize(&attributes, test_stack_bytes) != 0 ||
        pthread_create(&thread, &attributes, run_case, const_cast<TestCase *>(&test)) != 0) {
        test.run();
        return;
    }
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
}

}

int TestRegistry::run(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : nullptr;
    int executed = 0;
    for (TestCase *test = head_; test != nullptr; test = test->next()) {
        if (filter != nullptr && std::strstr(test->suite(), filter) == nullptr &&
            std::strstr(test->name(), filter) == nullptr)
            continue;
        current_failed_ = false;
        std::printf("%-34s %-54s", test->suite(), test->name());
        std::fflush(stdout);
        run_on_wide_stack(*test);
        std::printf("%s\n", current_failed_ ? "FAIL" : "ok");
        failures_ += current_failed_ ? 1 : 0;
        executed++;
    }
    std::printf("\n%d test(s), %d failure(s)\n", executed, failures_);
    return failures_ != 0 ? 1 : 0;
}

}

