#ifndef AV_BENCH_TIMER_HPP
#define AV_BENCH_TIMER_HPP

#include <cstdint>

namespace av {

class BenchTimer {
public:
    explicit BenchTimer(std::uint32_t rounds);

    template <typename Body>
    double nanoseconds_per_operation(std::uint64_t operations, Body body) const
    {
        double best = 0.0;
        for (std::uint32_t round = 0u; round < rounds_; round++) {
            const std::uint64_t began = now();
            body();
            const double each = static_cast<double>(now() - began) / static_cast<double>(operations);
            if (round == 0u || each < best) best = each;
        }
        return best;
    }

    template <typename Setup, typename Body>
    double nanoseconds_per_operation_after(std::uint64_t operations, Setup setup, Body body) const
    {
        double best = 0.0;
        for (std::uint32_t round = 0u; round < rounds_; round++) {
            setup();
            const std::uint64_t began = now();
            body();
            const double each = static_cast<double>(now() - began) / static_cast<double>(operations);
            if (round == 0u || each < best) best = each;
        }
        return best;
    }

    void report(const char *name, std::uint64_t operations, double nanoseconds) const;

private:
    static std::uint64_t now();

    std::uint32_t rounds_;
};

}

#endif
