/* Self-registering tests.
 *
 * The registry is the one place in this repo with process-lifetime state that
 * is not injected, because a test must register itself before main() runs.
 * It is a class with state, not a pile of file-scope variables, and nothing
 * outside the harness touches it. */
#ifndef AV_TEST_REGISTRY_HPP
#define AV_TEST_REGISTRY_HPP

namespace av {

class TestCase {
public:
    using Body = void (*)();

    /* Registers itself. Doing this from a separate __attribute__((constructor))
     * is a bug: the two run in unspecified order, and the constructor then
     * resets next_ and drops the rest of the list. */
    TestCase(const char *suite, const char *name, Body body);

    const char *suite() const;
    const char *name() const;
    void        run() const;
    TestCase   *next() const;
    void        set_next(TestCase *next);

private:
    const char *suite_;
    const char *name_;
    Body        body_;
    TestCase   *next_;
};

class TestRegistry {
public:
    TestRegistry();

    void add(TestCase *test);
    int  run(int argc, char **argv);

    void record_failure(const char *file, int line, const char *format, ...);

    static TestRegistry &instance();

private:
    TestCase *head_;
    int       failures_;
    bool      current_failed_;
};

}  // namespace av

#define TEST(suite_name, test_name)                                            \
    static void av_body_##suite_name##_##test_name();                          \
    static ::av::TestCase av_case_##suite_name##_##test_name{                  \
        #suite_name, #test_name, av_body_##suite_name##_##test_name};          \
    static void av_body_##suite_name##_##test_name()

#define CHECK(condition)                                                       \
    do { if (!(condition))                                                     \
        ::av::TestRegistry::instance().record_failure(                         \
            __FILE__, __LINE__, "CHECK(%s)", #condition); } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do { const long long a_ = static_cast<long long>(actual);                  \
         const long long e_ = static_cast<long long>(expected);                \
         if (a_ != e_) ::av::TestRegistry::instance().record_failure(          \
             __FILE__, __LINE__, "CHECK_EQ(%s, %s): %lld != %lld",             \
             #actual, #expected, a_, e_); } while (0)

#define CHECK_STATUS(expression, expected)                                     \
    do { const ::av::Status s_ = (expression);                                 \
         if (s_ != (expected)) ::av::TestRegistry::instance().record_failure(  \
             __FILE__, __LINE__, "%s: status %u, want %u", #expression,        \
             static_cast<unsigned>(s_),                                        \
             static_cast<unsigned>(expected)); } while (0)

#endif  // AV_TEST_REGISTRY_HPP
