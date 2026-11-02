#include "harness/framework/fixture_file.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace av {

FixtureFile::FixtureFile(const char *name) : bytes_{}, size_(0u)
{
    char path[512];
    std::snprintf(path, sizeof path, "%s/%s", AV_FIXTURE_DIR, name);
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "missing fixture %s", path);
        return;
    }
    size_ = std::fread(bytes_, 1u, capacity, file);
    std::fclose(file);
}

const std::uint8_t *FixtureFile::bytes() const   { return bytes_; }
std::uint8_t       *FixtureFile::mutable_bytes() { return bytes_; }
std::size_t         FixtureFile::size() const    { return size_; }

}
