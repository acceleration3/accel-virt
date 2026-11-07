#include "executor/vulkan/spirv_validator.hpp"

#include "core/support/fnv1a_digest.hpp"
#include "core/trace/function_scope.hpp"

#include <spirv-tools/libspirv.h>

namespace av {

namespace {

constexpr uint64_t second_salt = 0x9E3779B97F4A7C15ull;
constexpr size_t   word_bytes = 4u;
constexpr size_t   header_words = 5u;

}

SpirvValidator::SpirvValidator()
    : context_(nullptr), options_(nullptr), cache_{}, validations_(0u), cache_hits_(0u), validated_bytes_(0u)
{
    AV_SCOPE("SpirvValidator::SpirvValidator");
}

SpirvValidator::~SpirvValidator()
{
    AV_SCOPE("SpirvValidator::~SpirvValidator");
    if (options_ != nullptr) spvValidatorOptionsDestroy(options_);
    if (context_ != nullptr) spvContextDestroy(context_);
}

bool SpirvValidator::open()
{
    AV_SCOPE("SpirvValidator::open");
    if (context_ != nullptr) return true;
    context_ = spvContextCreate(SPV_ENV_VULKAN_1_3);
    options_ = spvValidatorOptionsCreate();
    if (context_ == nullptr || options_ == nullptr) {
        if (options_ != nullptr) spvValidatorOptionsDestroy(options_);
        if (context_ != nullptr) spvContextDestroy(context_);
        options_ = nullptr;
        context_ = nullptr;
        return false;
    }
    spvValidatorOptionsSetSkipBlockLayout(options_, true);
    spvValidatorOptionsSetRelaxLogicalPointer(options_, true);
    spvValidatorOptionsSetRelaxStoreStruct(options_, true);
    return true;
}

SpirvValidator::Verdict SpirvValidator::run(const uint32_t *code, size_t bytes)
{
    AV_SCOPE("SpirvValidator::run");
    if (!open()) return Verdict::unavailable;
    spv_const_binary_t binary = {code, bytes / word_bytes};
    spv_diagnostic     diagnostic = nullptr;
    const spv_result_t result = spvValidateWithOptions(context_, options_, &binary, &diagnostic);
    if (diagnostic != nullptr) spvDiagnosticDestroy(diagnostic);
    validations_++;
    validated_bytes_ += bytes;
    return result == SPV_SUCCESS ? Verdict::valid : Verdict::invalid;
}

SpirvValidator::Verdict SpirvValidator::check(const uint32_t *code, size_t bytes)
{
    AV_SCOPE("SpirvValidator::check");
    if (code == nullptr || bytes < header_words * word_bytes || bytes % word_bytes != 0u) return Verdict::invalid;
    Fnv1aDigest first;
    first.add(code, bytes);
    Fnv1aDigest second(second_salt);
    second.add_word(bytes);
    second.add(code, bytes);
    Entry &slot = cache_[first.value() % cache_slots];
    if (slot.used_ && slot.digest_ == first.value() && slot.second_digest_ == second.value() && slot.bytes_ == bytes) {
        cache_hits_++;
        return slot.valid_ ? Verdict::valid : Verdict::invalid;
    }
    const Verdict verdict = run(code, bytes);
    if (verdict == Verdict::unavailable) return verdict;
    slot = {first.value(), second.value(), bytes, true, verdict == Verdict::valid};
    return verdict;
}

uint64_t SpirvValidator::validations() const
{
    AV_SCOPE("SpirvValidator::validations");
    return validations_;
}

uint64_t SpirvValidator::cache_hits() const
{
    AV_SCOPE("SpirvValidator::cache_hits");
    return cache_hits_;
}

uint64_t SpirvValidator::validated_bytes() const
{
    AV_SCOPE("SpirvValidator::validated_bytes");
    return validated_bytes_;
}

bool SpirvValidator::invariants_hold() const
{
    AV_SCOPE("SpirvValidator::invariants_hold");
    if ((context_ == nullptr) != (options_ == nullptr)) return false;
    if (validated_bytes_ < validations_ * header_words * word_bytes) return false;
    for (const Entry &slot : cache_)
        if (slot.used_ && slot.bytes_ % word_bytes != 0u) return false;
    return true;
}

}
