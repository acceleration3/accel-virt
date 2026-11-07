#ifndef AV_SPIRV_VALIDATOR_HPP
#define AV_SPIRV_VALIDATOR_HPP

#include <stddef.h>
#include <stdint.h>

struct spv_context_t;
struct spv_validator_options_t;

namespace av {

class SpirvValidator {
public:
    enum class Verdict : uint32_t { valid, invalid, unavailable };

    static constexpr uint32_t cache_slots = 512u;

    SpirvValidator();
    ~SpirvValidator();
    SpirvValidator(const SpirvValidator &) = delete;
    SpirvValidator &operator=(const SpirvValidator &) = delete;

    Verdict check(const uint32_t *code, size_t bytes);

    uint64_t validations() const;
    uint64_t cache_hits() const;
    uint64_t validated_bytes() const;
    bool     invariants_hold() const;

private:
    struct Entry {
        uint64_t digest_;
        uint64_t second_digest_;
        uint64_t bytes_;
        bool     used_;
        bool     valid_;
    };

    bool    open();
    Verdict run(const uint32_t *code, size_t bytes);

    spv_context_t           *context_;
    spv_validator_options_t *options_;
    Entry                    cache_[cache_slots];
    uint64_t                 validations_;
    uint64_t                 cache_hits_;
    uint64_t                 validated_bytes_;
};

}

#endif
