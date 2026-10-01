#include "icd/opengl/gl_context_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

GlContextTable::GlContextTable(BlockSource &memory, icd::IcdConnection &connection, icd::ThreadLock &send_lock,
                               LogSink &log, const GlExtensionPolicy &extensions,
                               icd::SharedHandles &handles)
    : memory_(memory), connection_(connection), send_lock_(send_lock), log_(log), extensions_(extensions),
      handles_(handles), pool_(memory), contexts_{},
      generations_{}, live_(0u)
{
    AV_SCOPE("GlContextTable::GlContextTable");
    for (uint32_t &generation : generations_) generation = 1u;
}

Status GlContextTable::create(int32_t pixel_format, const GlContextOptions &options, uint64_t &handle)
{
    AV_SCOPE("GlContextTable::create");
    handle = 0u;
    uint32_t slot = 0u;
    while (slot < max_contexts && contexts_[slot] != nullptr) slot++;
    if (slot == max_contexts) return Status::no_space;
    GuestGlContext *made = nullptr;
    const Status status = pool_.create(made, connection_, memory_, send_lock_, log_, extensions_, handles_,
                                       pixel_format, options);
    if (status != Status::ok) return status;
    contexts_[slot] = made;
    live_++;
    handle = (static_cast<uint64_t>(generations_[slot]) << generation_shift) | (slot + 1u);
    return Status::ok;
}

GuestGlContext *GlContextTable::resolve(uint64_t handle) const
{
    AV_SCOPE("GlContextTable::resolve");
    const uint32_t slot = slot_of(handle);
    return slot == max_contexts ? nullptr : contexts_[slot];
}

uint64_t GlContextTable::handle_of(const GuestGlContext &context) const
{
    AV_SCOPE("GlContextTable::handle_of");
    for (uint32_t slot = 0u; slot < max_contexts; slot++)
        if (contexts_[slot] == &context)
            return (static_cast<uint64_t>(generations_[slot]) << generation_shift) | (slot + 1u);
    return 0u;
}

Status GlContextTable::destroy(uint64_t handle)
{
    AV_SCOPE("GlContextTable::destroy");
    const uint32_t slot = slot_of(handle);
    if (slot == max_contexts) return Status::invalid_parameter;
    const Status status = pool_.destroy(contexts_[slot]);
    contexts_[slot] = nullptr;
    live_--;
    generations_[slot] = generations_[slot] == max_generation ? 1u : generations_[slot] + 1u;
    return status;
}

uint32_t GlContextTable::live() const
{
    AV_SCOPE("GlContextTable::live");
    return live_;
}

void GlContextTable::report(StateWriter &writer) const
{
    AV_SCOPE("GlContextTable::report");
    writer.open("contexts", 0u);
    writer.number("live", live_);
    for (uint32_t slot = 0u; slot < max_contexts; slot++)
        if (contexts_[slot] != nullptr)
            contexts_[slot]->report(writer, (static_cast<uint64_t>(generations_[slot]) << generation_shift) | (slot + 1u));
    writer.close();
}

bool GlContextTable::invariants_hold() const
{
    AV_SCOPE("GlContextTable::invariants_hold");
    uint32_t counted = 0u;
    for (uint32_t slot = 0u; slot < max_contexts; slot++) {
        if (generations_[slot] == 0u || generations_[slot] > max_generation) return false;
        if (contexts_[slot] == nullptr) continue;
        counted++;
        if (!contexts_[slot]->invariants_hold()) return false;
    }
    return counted == live_;
}

/* A live slot whose generation the handle carries, and no bit past both. */
uint32_t GlContextTable::slot_of(uint64_t handle) const
{
    AV_SCOPE("GlContextTable::slot_of");
    const uint64_t slot_bits = handle & slot_mask;
    const uint64_t generation = handle >> generation_shift;
    if (slot_bits == 0u || slot_bits > max_contexts) return max_contexts;
    const uint32_t slot = static_cast<uint32_t>(slot_bits) - 1u;
    if (contexts_[slot] == nullptr || generation != generations_[slot]) return max_contexts;
    return slot;
}

}  // namespace av::glicd
