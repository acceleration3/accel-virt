/* A ContextPlane that runs nothing and remembers what it was told: which
 * blobs were made and let go, which host memory was asked for and freed,
 * whether the interrupts were connected, and how often the renderer was lost.
 * Its host memory is a fresh memfd per allocation - a real descriptor the
 * device must place and close - unless a test has it allocate through an
 * executor library instead. For tests of the control plane's commands. */
#ifndef AV_RECORDING_CONTEXT_PLANE_HPP
#define AV_RECORDING_CONTEXT_PLANE_HPP

#include "device/context/context_plane.hpp"
#include "device/execution/executor_library.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

namespace av {

class RecordingContextPlane final : public ContextPlane {
public:
    static constexpr std::uint32_t capacity = 64u;

    Status        accept(const AvExecutorOps *) override { return Status::ok; }
    Status        initialize() override { return Status::ok; }
    Status        start(const std::uint8_t *, const CapsetInfo &) override { return Status::ok; }
    std::uint32_t sub_apis() const override { return 0u; }
    Status        allocate_memory(std::uint32_t context, std::uint64_t key, std::uint64_t size,
                                  std::uint32_t request, AvHostMemory &memory) override;
    void          free_memory(std::uint64_t key) override;
    /* An alias is made as the test said (alias_as): refused by default, and
     * else described as an allocation of `size` bytes is. */
    Status        alias_memory(std::uint64_t key, const ContextPlane &lender, std::uint64_t lender_key,
                               AvHostMemory &memory) override
    {
        last_alias_ = AliasAsked{key, &lender, lender_key};
        aliases_++;
        if (!alias_) return Status::no_memory;
        memory = alias_memory_;
        return Status::ok;
    }
    std::uint32_t memory_domain() const override { return memory_domain_; }
    void          alias_as(const AvHostMemory &memory) { alias_ = true; alias_memory_ = memory; }
    void          set_memory_domain(std::uint32_t domain) { memory_domain_ = domain; }
    struct AliasAsked {
        std::uint64_t       key;
        const ContextPlane *lender;
        std::uint64_t       lender_key;
    };
    const AliasAsked &last_alias() const { return last_alias_; }
    std::uint32_t     aliases() const { return aliases_; }
    Status        create(std::uint32_t) override { return Status::not_supported; }
    Status        destroy(std::uint32_t) override { return Status::not_found; }
    void          destroy_all() override {}
    /* Only the context a test said is live (hold_context): none by default. */
    bool          holds(std::uint32_t id) override { return id != 0u && id == held_context_; }
    void          hold_context(std::uint32_t id) { held_context_ = id; }
    Status        bind(std::uint32_t, const ContextSetup &, const CapsetInfo &) override { return Status::not_supported; }
    bool          holds_doorbell(std::uint32_t) override { return false; }
    bool          can_share(std::uint32_t, std::uint32_t) override { return false; }
    std::uint32_t live() const override { return 0u; }
    void          blob_created(std::uint32_t id) override { if (created_count_ < capacity) created_[created_count_++] = id; }
    void          blob_released(std::uint32_t id) override { if (released_count_ < capacity) released_[released_count_++] = id; }
    void          shared_released(const std::uint8_t *gpu_uuid, std::uint64_t object_id) override
    {
        shared_gpu_ = gpu_uuid;
        if (shared_count_ < capacity) shared_[shared_count_++] = object_id;
    }
    void          interrupts_changed(bool connected) override { connected_ = connected; changes_++; }
    void          renderer_lost() override { losses_++; }
    bool export_image(const std::uint8_t *, std::uint64_t, AvExportedImage &) override { return false; }
    RecordingContextPlane() = default;
    RecordingContextPlane(const RecordingContextPlane &) = delete;
    RecordingContextPlane &operator=(const RecordingContextPlane &) = delete;
    /* The image composed into, which the plane owns, is closed. */
    ~RecordingContextPlane();

    /* Planes composed as the test scripts them (compose_as): into one image
     * the plane keeps, as the executor keeps its scanout image - a memfd it
     * closes itself - described as `image` says. */
    bool composes() const override { return composes_; }
    bool compose_planes(const std::uint8_t *, const AvScanoutPlane *planes, std::uint32_t count, std::uint32_t width,
                        std::uint32_t height, AvExportedImage &image) override
    {
        composed_count_ = count;
        composed_width_ = width;
        composed_height_ = height;
        if (!composes_ || count == 0u || planes == nullptr) return false;
        if (composed_fd_ < 0) composed_fd_ = fresh_memfd();
        image = composed_image_;
        image.fd = composed_fd_;
        return true;
    }
    void          compose_as(const AvExportedImage &image) { composes_ = true; composed_image_ = image; }
    std::uint32_t composed_count() const { return composed_count_; }
    std::uint32_t composed_width() const { return composed_width_; }
    int           composed_fd() const { return composed_fd_; }
    bool read_image(const std::uint8_t *, std::uint64_t, void *, std::uint32_t, std::uint32_t, std::uint32_t) override
    { return false; }
    /* One frame served as `frame` from the test's own pixels, as an executor
     * maps one; mapped once, and every unmap counted. */
    bool reads_frames() const override { return served_; }
    bool map_frame(const std::uint8_t *, const SurfacePoint &frame, std::uint32_t, AvFrameView &view) override
    {
        if (!frame.names() || !(frame == frame_) || snapshot_mapped_) return false;
        snapshot_mapped_ = true;
        view = snapshot_;
        return true;
    }
    /* Let go, as an executor lets a frame go: it is not there to map again. */
    void unmap_frame(const std::uint8_t *, const SurfacePoint &frame) override
    {
        if (frame == frame_) frame_ = SurfacePoint();
        snapshot_mapped_ = false;
        unmaps_++;
    }
    /* A frame copied on the GPU answers `copy_answer` (not_supported unless a
     * test says otherwise, as a plane that cannot copy does); the last copy
     * asked for is kept. */
    Status copy_frame(const std::uint8_t *, const SurfacePoint &frame, std::uint32_t, const AvFrameCopy &copy) override
    {
        copies_++;
        last_copy_ = copy;
        last_copy_frame_ = frame;
        for (std::uint32_t at = 0u; at < copy.count && at < AV_FRAME_COPY_MOST_REGIONS; at++)
            last_copy_regions_[at] = copy.regions[at];
        last_copy_.regions = last_copy_regions_;
        return copy_answer_;
    }
    void           answer_copies_with(Status answer) { copy_answer_ = answer; }
    std::uint32_t  copies() const { return copies_; }
    const AvFrameCopy &last_copy() const { return last_copy_; }
    const SurfacePoint &last_copy_frame() const { return last_copy_frame_; }
    /* What allocate_memory was asked last, and how often; the keys freed. */
    struct MemoryAsked {
        std::uint32_t context;
        std::uint64_t key;
        std::uint64_t size;
        std::uint32_t request;
    };
    MemoryAsked   last_memory() const { return last_memory_; }
    std::uint32_t memory_allocations() const { return memory_allocations_; }
    std::uint64_t freed(std::uint32_t index) const { return freed_[index]; }
    std::uint32_t freed_count() const { return freed_count_; }
    /* The Nth allocation from now is refused. */
    void          refuse_memory_at(std::uint32_t nth) { refuse_memory_ = nth; }
    /* What every answer carries: its caching (write-back unless told), one
     * page less than asked, or no descriptor at all - the last two a library
     * breaking the ABI. */
    void          answer_caching(std::uint32_t caching) { caching_ = caching; }
    void          answer_short() { short_ = true; }
    /* One page more than asked, as a driver that rounds an allocation up. */
    void          answer_long() { long_ = true; }
    void          answer_without_descriptor() { without_descriptor_ = true; }
    /* Memory allocated and freed through `library` from now on, as the local
     * plane does, so an import of it reaches memory the library knows. */
    void          allocate_through(ExecutorLibrary &library) { library_ = &library; }
    void serve_frame(const SurfacePoint &frame, const AvFrameView &view)
    {
        frame_ = frame;
        snapshot_ = view;
        snapshot_mapped_ = false;
        served_ = true;
    }
    /* Surfaces exported as the test scripts them: `count` of them from
     * `surface`, each one's memory a fresh memfd - a real descriptor, which
     * the device must close or hand over - described as `image` says (its fd
     * ignored), and its syncs a fresh eventfd, already readable, with
     * `writes`. */
    bool exports_surfaces() const override { return exports_; }
    bool export_surface(const std::uint8_t *gpu_uuid, std::uint64_t surface, std::uint32_t flags, AvExportedImage &image) override
    {
        export_gpu_ = gpu_uuid;
        export_flags_ = flags;
        exports_asked_++;
        if (!exported(surface)) return false;
        image = exported_image_;
        image.fd = exported_fd_ ? fresh_memfd() : -1;
        return true;
    }
    bool sync_surface(const std::uint8_t *gpu_uuid, std::uint64_t surface, std::uint32_t flags, std::uint64_t &writes, int &fd) override
    {
        export_gpu_ = gpu_uuid;
        export_flags_ = flags;
        syncs_asked_++;
        if (!exported(surface)) return false;
        writes = writes_;
        fd = synced_fd_ ? fresh_eventfd() : -1;
        return true;
    }
    void export_as(std::uint64_t surface, const AvExportedImage &image, std::uint64_t writes,
                   std::uint64_t count = 1u)
    {
        exports_ = true;
        exported_surface_ = surface;
        exported_count_ = count;
        exported_image_ = image;
        writes_ = writes;
    }
    /* A library that says it executed and handed over no descriptor. */
    void export_without_descriptor() { exported_fd_ = false; }
    void sync_without_descriptor() { synced_fd_ = false; }
    std::uint32_t       exports_asked() const { return exports_asked_; }
    std::uint32_t       syncs_asked() const { return syncs_asked_; }
    const std::uint8_t *export_gpu() const { return export_gpu_; }
    /* The flags of the last export or sync asked. */
    std::uint32_t       export_flags() const { return export_flags_; }
    bool          snapshot_mapped() const { return snapshot_mapped_; }
    std::uint32_t unmaps() const { return unmaps_; }
    void report(StateWriter &) override {}
    bool invariants_hold() const override { return true; }

    std::uint32_t created(std::uint32_t index) const { return created_[index]; }
    std::uint32_t created_count() const { return created_count_; }
    std::uint32_t released(std::uint32_t index) const { return released_[index]; }
    std::uint32_t released_count() const { return released_count_; }
    std::uint64_t shared(std::uint32_t index) const { return shared_[index]; }
    std::uint32_t shared_count() const { return shared_count_; }
    const std::uint8_t *shared_gpu() const { return shared_gpu_; }
    bool          connected() const { return connected_; }
    std::uint32_t changes() const { return changes_; }
    std::uint32_t losses() const { return losses_; }

private:
    int fresh_memfd(std::uint64_t size = 4096u) const;
    int fresh_eventfd() const;

    bool exported(std::uint64_t surface) const
    {
        return exports_ && surface >= exported_surface_ && surface - exported_surface_ < exported_count_;
    }

    bool            composes_ = false;
    AvExportedImage composed_image_ = {};
    int             composed_fd_ = -1;
    std::uint32_t   composed_count_ = 0u;
    std::uint32_t   composed_width_ = 0u;
    std::uint32_t   composed_height_ = 0u;
    bool            exports_ = false;
    bool            exported_fd_ = true;
    bool            synced_fd_ = true;
    std::uint64_t   exported_surface_ = 0u;
    std::uint64_t   exported_count_ = 0u;
    AvExportedImage exported_image_ = {};
    std::uint64_t   writes_ = 0u;
    std::uint32_t   exports_asked_ = 0u;
    std::uint32_t   syncs_asked_ = 0u;
    const std::uint8_t *export_gpu_ = nullptr;
    std::uint32_t       export_flags_ = 0u;
    SurfacePoint    frame_;
    AvFrameView snapshot_ = {};
    bool            snapshot_mapped_ = false;
    MemoryAsked     last_memory_ = {};
    std::uint32_t   memory_allocations_ = 0u;
    std::uint64_t   freed_[capacity] = {};
    std::uint32_t   freed_count_ = 0u;
    std::uint32_t   refuse_memory_ = 0u;
    bool            alias_ = false;
    AvHostMemory    alias_memory_ = {-1, 0u, 0u};
    AliasAsked      last_alias_ = {0u, nullptr, 0u};
    std::uint32_t   aliases_ = 0u;
    std::uint32_t   memory_domain_ = 1u;
    std::uint32_t   caching_ = AV_MEMORY_WRITE_BACK;
    bool            short_ = false;
    bool            long_ = false;
    bool            without_descriptor_ = false;
    ExecutorLibrary *library_ = nullptr;
    std::uint32_t   held_context_ = 0u;
    bool            served_ = false;
    std::uint32_t   unmaps_ = 0u;
    Status          copy_answer_ = Status::not_supported;
    std::uint32_t   copies_ = 0u;
    AvFrameCopy     last_copy_ = {};
    AvFrameRegion   last_copy_regions_[AV_FRAME_COPY_MOST_REGIONS] = {};
    SurfacePoint    last_copy_frame_;
    std::uint32_t created_[capacity] = {};
    std::uint32_t created_count_ = 0u;
    std::uint32_t released_[capacity] = {};
    std::uint32_t released_count_ = 0u;
    std::uint64_t shared_[capacity] = {};
    std::uint32_t shared_count_ = 0u;
    const std::uint8_t *shared_gpu_ = nullptr;
    bool          connected_ = false;
    std::uint32_t changes_ = 0u;
    std::uint32_t losses_ = 0u;
};

}  // namespace av

#endif  // AV_RECORDING_CONTEXT_PLANE_HPP
