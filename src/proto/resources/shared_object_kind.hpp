/* What kind of thing a shared object is (feature 053, data-model.md).
 *
 * A kind is part of an object's identity and is never inferred from anything
 * else: a guest that could have an image opened as a fence could have the host
 * read a VkSemaphore as memory. So the kind is carried in the record the
 * miniport writes, carried again in every transport command, and compared on
 * both sides before anything is exported or imported.
 *
 * A value class, like Alignment: it owns the number it is, and every question
 * about it is a question of its own. */
#ifndef AV_SHARED_OBJECT_KIND_HPP
#define AV_SHARED_OBJECT_KIND_HPP

#include <stdint.h>

namespace av {

class SharedObjectKind {
public:
    /* The wire values, which are append-only: a kind added later takes 4. */
    static constexpr uint32_t memory = 1u;
    static constexpr uint32_t monitored_fence = 2u;
    static constexpr uint32_t native_fence = 3u;
    static constexpr uint64_t page_size = 4096u;

    /* An unknown kind, which nothing accepts. */
    SharedObjectKind();
    explicit SharedObjectKind(uint32_t value);

    uint32_t value() const;
    /* Whether this protocol version has this kind at all. 0 and everything
     * above native_fence are refused wherever a kind is read. */
    bool     known() const;
    /* Whether it is memory, which is the one kind every host API imports; the
     * two fence kinds are a host semaphore and only Vulkan exports one. */
    bool     is_memory() const;
    /* Either fence kind: what the host holds for both is one timeline
     * semaphore, so the two differ to the guest and not to the host. */
    bool     is_fence() const;
    bool     same_as(const SharedObjectKind &other) const;
    /* Whether an object of this kind can be `size` bytes: memory is whole
     * non-zero pages, and a fence has no size at all (feature 053, T031). The
     * miniport's record and the wire's request both keep this one rule,
     * because an opener compares the two. */
    bool     sized(uint64_t size) const;

private:
    uint32_t value_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_KIND_HPP
