/* The OpenCL server dispatch with no OpenCL behind it.
 *
 * Holds handles the way OpenClBackend does - a ClHandleRegistry answering the
 * decoder's lookups - and makes objects out of its own storage, so a test or a
 * fuzz target can drive the generated decoder with a real object chain and no
 * GPU. What it records is what the decoder must never do: hand a call an
 * object this dispatch did not make (a stranger), or a span that runs outside
 * what the decode arena gave out (a stray). */
#ifndef AV_RECORDING_CL_DISPATCH_HPP
#define AV_RECORDING_CL_DISPATCH_HPP

#include "device/executor/cl_handle_registry.hpp"
#include "proto/generated/opencl/cl_default_dispatch.h"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingClDispatch final : public clproto::DefaultDispatch {
public:
    /* Enough for any test, and few enough that a fuzz input exhausts them. */
    static constexpr std::size_t max_objects = 256u;

    /* `block` is the start of the memory `arena` hands out from. */
    RecordingClDispatch(const std::uint8_t *block, const WireArena &arena);
    RecordingClDispatch(const RecordingClDispatch &) = delete;
    RecordingClDispatch &operator=(const RecordingClDispatch &) = delete;

    void *handle_for(clproto::HandleKind kind, std::uint64_t wire) const override;
    void  made(clproto::HandleKind kind, void *object) override;
    void  retained(clproto::HandleKind kind, void *object) override;
    void  released(clproto::HandleKind kind, void *object) override;

    /* The number a guest names `object` by. */
    std::uint64_t wire_of(const void *object) const;
    /* The number of the `index`th object this dispatch made, or 0 for one it
     * has not made yet: what a fuzz target writes where a guest would have
     * written a number it was answered. */
    std::uint64_t wire_of_made(std::size_t index) const;

    std::uint32_t calls() const;
    std::uint32_t last_called() const;
    std::uint32_t strangers() const;
    std::uint32_t strays() const;
    /* Objects made that the registry would not hold. */
    std::uint32_t refused() const;
    const ClHandleRegistry &handles() const;

protected:
    void  called(std::uint32_t command_id) override;
    void  handed(clproto::HandleKind kind, void *object) override;
    void  touched(const void *bytes, std::size_t length) override;
    void *produce(clproto::HandleKind kind) override;

private:
    const std::uint8_t *block_;
    const WireArena    &arena_;
    ClHandleRegistry    handles_;
    unsigned char       objects_[max_objects];
    std::size_t         produced_;
    std::uint32_t       calls_;
    std::uint32_t       last_called_;
    std::uint32_t       strangers_;
    std::uint32_t       strays_;
    std::uint32_t       refused_;
};

}  // namespace av

#endif  // AV_RECORDING_CL_DISPATCH_HPP
