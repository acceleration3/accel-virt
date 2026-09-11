/* A committed test fixture (tests/fixtures/), read whole into a fixed buffer. */
#ifndef AV_FIXTURE_FILE_HPP
#define AV_FIXTURE_FILE_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class FixtureFile {
public:
    static constexpr std::size_t capacity = 65536u;

    /* `name` is relative to tests/fixtures; a missing file fails the test. */
    explicit FixtureFile(const char *name);

    const std::uint8_t *bytes() const;
    std::uint8_t       *mutable_bytes();
    std::size_t         size() const;

private:
    std::uint8_t bytes_[capacity];
    std::size_t  size_;
};

}  // namespace av

#endif  // AV_FIXTURE_FILE_HPP
