#include "executor/opencl/cl_renderbuffer_images.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t key_of(const void *mem)
{
    AV_SCOPE("key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(mem));
}

}

ClRenderbufferImages::ClRenderbufferImages(BlockSource &blocks) : images_(blocks, Pool::paged)
{
    AV_SCOPE("ClRenderbufferImages::ClRenderbufferImages");
}

Status ClRenderbufferImages::remember(const void *mem, uint32_t renderbuffer, uint32_t texture, void *image)
{
    AV_SCOPE("ClRenderbufferImages::remember");
    if (mem == nullptr || renderbuffer == 0u || texture == 0u) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return images_.insert(key_of(mem), RenderbufferImage{renderbuffer, texture, image}, slot);
}

uint32_t ClRenderbufferImages::renderbuffer_of(const void *mem) const
{
    AV_SCOPE("ClRenderbufferImages::renderbuffer_of");
    const RenderbufferImage *const held = images_.find(key_of(mem));
    return held != nullptr ? held->renderbuffer_ : 0u;
}

bool ClRenderbufferImages::forget(const void *mem, uint32_t &texture, void *&image)
{
    AV_SCOPE("ClRenderbufferImages::forget");
    RenderbufferImage gone{};
    if (!images_.take(key_of(mem), gone)) return false;
    texture = gone.texture_;
    image = gone.image_;
    return true;
}

bool ClRenderbufferImages::forget_any(uint32_t &texture, void *&image)
{
    AV_SCOPE("ClRenderbufferImages::forget_any");
    const uint32_t slot = images_.next_live(0u);
    if (slot == KeyedSlots<uint64_t, RenderbufferImage>::no_slot) return false;
    texture = images_.at(slot)->texture_;
    image = images_.at(slot)->image_;
    images_.erase_slot(slot);
    return true;
}

uint32_t ClRenderbufferImages::count() const
{
    AV_SCOPE("ClRenderbufferImages::count");
    return images_.count();
}

bool ClRenderbufferImages::invariants_hold() const
{
    AV_SCOPE("ClRenderbufferImages::invariants_hold");
    bool holds = true;
    images_.for_each([&holds](uint64_t mem, const RenderbufferImage &held) {
        holds = holds && mem != 0u && held.renderbuffer_ != 0u && held.texture_ != 0u;
    });
    return holds;
}

}
