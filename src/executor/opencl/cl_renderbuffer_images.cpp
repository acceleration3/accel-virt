#include "executor/opencl/cl_renderbuffer_images.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClRenderbufferImages::ClRenderbufferImages() : mems_{}, renderbuffers_{}, textures_{}, images_{}, count_(0u)
{
    AV_SCOPE("ClRenderbufferImages::ClRenderbufferImages");
}

bool ClRenderbufferImages::remember(const void *mem, uint32_t renderbuffer, uint32_t texture, void *image)
{
    AV_SCOPE("ClRenderbufferImages::remember");
    if (mem == nullptr || renderbuffer == 0u || texture == 0u || count_ == max_images || slot_of(mem) != count_)
        return false;
    mems_[count_] = mem;
    renderbuffers_[count_] = renderbuffer;
    textures_[count_] = texture;
    images_[count_] = image;
    count_++;
    return true;
}

uint32_t ClRenderbufferImages::renderbuffer_of(const void *mem) const
{
    AV_SCOPE("ClRenderbufferImages::renderbuffer_of");
    const uint32_t slot = slot_of(mem);
    return slot != count_ ? renderbuffers_[slot] : 0u;
}

bool ClRenderbufferImages::forget(const void *mem, uint32_t &texture, void *&image)
{
    AV_SCOPE("ClRenderbufferImages::forget");
    const uint32_t slot = slot_of(mem);
    if (slot == count_) return false;
    forget_slot(slot, texture, image);
    return true;
}

bool ClRenderbufferImages::forget_any(uint32_t &texture, void *&image)
{
    AV_SCOPE("ClRenderbufferImages::forget_any");
    if (count_ == 0u) return false;
    forget_slot(0u, texture, image);
    return true;
}

uint32_t ClRenderbufferImages::count() const
{
    AV_SCOPE("ClRenderbufferImages::count");
    return count_;
}

uint32_t ClRenderbufferImages::slot_of(const void *mem) const
{
    AV_SCOPE("ClRenderbufferImages::slot_of");
    for (uint32_t slot = 0u; slot < count_; slot++)
        if (mems_[slot] == mem) return slot;
    return count_;
}

/* The last held moves into the hole, so the held stay the first count_. */
void ClRenderbufferImages::forget_slot(uint32_t slot, uint32_t &texture, void *&image)
{
    AV_SCOPE("ClRenderbufferImages::forget_slot");
    texture = textures_[slot];
    image = images_[slot];
    count_--;
    mems_[slot] = mems_[count_];
    renderbuffers_[slot] = renderbuffers_[count_];
    textures_[slot] = textures_[count_];
    images_[slot] = images_[count_];
    mems_[count_] = nullptr;
    renderbuffers_[count_] = 0u;
    textures_[count_] = 0u;
    images_[count_] = nullptr;
}

/* Every held slot names a mem, a renderbuffer and a texture, no mem twice;
 * every slot past them is empty. */
bool ClRenderbufferImages::invariants_hold() const
{
    AV_SCOPE("ClRenderbufferImages::invariants_hold");
    if (count_ > max_images) return false;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        if (mems_[slot] == nullptr || renderbuffers_[slot] == 0u || textures_[slot] == 0u) return false;
        for (uint32_t other = slot + 1u; other < count_; other++)
            if (mems_[other] == mems_[slot]) return false;
    }
    for (uint32_t slot = count_; slot < max_images; slot++)
        if (mems_[slot] != nullptr || renderbuffers_[slot] != 0u || textures_[slot] != 0u || images_[slot] != nullptr)
            return false;
    return true;
}

}  // namespace av
