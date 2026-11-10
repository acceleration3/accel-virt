#ifndef AV_CLICD_CL_OBJECT_HPP
#define AV_CLICD_CL_OBJECT_HPP

#include "icd/common/id_mint.hpp"
#include "icd/opencl/cl_info_answers.hpp"
#include "icd/opencl/cl_mem_extent.hpp"

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stdint.h>

namespace av {
class HostAllocation;
}

namespace av::clicd {

class ClKernelShape;
class ClProgramSource;
class GuestClContext;

struct ClObjectProbe;

class ClObject {
public:
    enum class Kind : uint8_t {
        platform, device, context, command_queue, program, mem, sampler, kernel, event
    };

    ClObject(const cl_icd_dispatch &table, BlockSource &memory, Kind kind, uint64_t wire, GuestClContext *context,
             ClObject *parent);
    ClObject(const ClObject &) = delete;
    ClObject &operator=(const ClObject &) = delete;

    Kind            kind() const;
    uint64_t        wire() const;
    GuestClContext *context() const;
    ClObject       *parent() const;

    void     back_with(uint64_t offset, uint64_t bytes);
    bool     backed() const;
    uint64_t backing_offset() const;
    uint64_t backing_bytes() const;

    void     use_application_memory(void *memory);
    uint8_t *application_memory() const;
    void            lend(HostAllocation &lent);
    HostAllocation *lent() const;

    void      slice_of(ClObject &buffer);
    ClObject *sliced_from() const;

    ClObject *nearest(Kind kind);

    static constexpr uint32_t program_typed_arguments = 1u;
    static constexpr uint32_t program_arguments_asked = 2u;
    static constexpr uint32_t program_non_uniform = 4u;
    static constexpr uint32_t kernel_unshaped = 8u;
    static constexpr uint32_t program_from_source = 16u;

    void            keep_source(ClProgramSource *source);
    ClProgramSource *source() const;
    void            rewire(uint64_t wire);
    void            keep_shape(ClKernelShape *shape);
    ClKernelShape  *shape() const;
    ClMemExtent    &extent();
    const ClMemExtent &extent() const;
    void            keep_companion(ClObject *companion);
    ClObject       *companion() const;
    void            keep_flags(uint64_t flags);
    uint64_t        flags() const;
    void            keep_command_type(uint32_t type);
    uint32_t        command_type() const;
    void            keep_traits(uint32_t traits);
    uint32_t        traits() const;

    void     retain();
    bool     release();
    uint32_t references() const;
    void     adopt();
    void     orphan();
    bool     unheld() const;

    void     busy();
    void     idle();
    bool     known_idle() const;
    uint64_t take_ordinal();
    void     stamp(uint64_t ordinal);
    uint64_t ordinal() const;

    [[nodiscard]] bool kept_answer(cl_mem_info name, const ClInfoRequest &asked, cl_int &result) const;
    void               keep_answer(cl_mem_info name, const void *value, size_t bytes);

    bool invariants_hold() const;

private:
    friend struct ClObjectProbe;

    const cl_icd_dispatch *dispatch_;
    GuestClContext        *context_;
    ClObject              *parent_;
    uint64_t               wire_;
    uint64_t               backing_offset_;
    uint64_t               backing_bytes_;
    uint8_t               *application_memory_;
    HostAllocation        *lent_;
    ClObject              *sliced_from_;
    ClKernelShape         *shape_;
    ClProgramSource       *source_;
    ClObject              *companion_;
    uint64_t               flags_;
    ClMemExtent            extent_;
    uint32_t               command_type_;
    uint32_t               traits_;
    uint32_t               references_;
    uint32_t               children_;
    Kind                   kind_;
    bool                   idle_;
    icd::IdMint<uint64_t>  ordinals_;
    uint64_t               ordinal_;
    ClInfoAnswers<sizeof(uint64_t)> fixed_;
};

}

#endif
