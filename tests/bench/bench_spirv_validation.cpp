#include "bench_timer.hpp"
#include "executor/vulkan/spirv_validator.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

bool load(const char *path, std::vector<std::uint32_t> &words)
{ FILE *file = std::fopen(path, "rb");
  if (file == nullptr) return false;
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  words.assign(static_cast<std::size_t>(size) / 4u, 0u);
  const std::size_t read = std::fread(words.data(), 4u, words.size(), file);
  std::fclose(file);
  return read == words.size(); }

}

int main(int argc, char **argv)
{
    av::BenchTimer timer(5u);
    for (int index = 1; index < argc; index++) {
        std::vector<std::uint32_t> words;
        if (!load(argv[index], words)) continue;
        const std::size_t bytes = words.size() * 4u;
        const std::uint64_t runs = 20u;
        const double cold = timer.nanoseconds_per_operation(runs, [&] {
            av::SpirvValidator validator;
            for (std::uint64_t run = 0u; run < runs; run++) {
                words[2] = static_cast<std::uint32_t>(run + 1u);
                static_cast<void>(validator.check(words.data(), bytes));
            }
        });
        av::SpirvValidator warm;
        static_cast<void>(warm.check(words.data(), bytes));
        const std::uint64_t hits = 20000u;
        const double cached = timer.nanoseconds_per_operation(hits, [&] {
            for (std::uint64_t run = 0u; run < hits; run++) static_cast<void>(warm.check(words.data(), bytes));
        });
        std::printf("%s: %zu bytes, cold %.1f us (%.2f us/KB), cached %.1f us (%.3f us/KB)\n", argv[index], bytes,
                    cold / 1000.0, cold / 1000.0 / (static_cast<double>(bytes) / 1024.0), cached / 1000.0,
                    cached / 1000.0 / (static_cast<double>(bytes) / 1024.0));
    }
    return 0;
}
