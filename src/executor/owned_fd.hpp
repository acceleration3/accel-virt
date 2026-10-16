/* One file descriptor this library holds for a host object - a dma-buf of
 * memory, the opaque fd vkd3d-proton exports an image's memory as, a
 * timeline semaphore's payload - owned until something takes it: an import,
 * or a caller that owns it from then on. Closed when it goes otherwise.
 *
 * Every import here takes the descriptor only where it succeeds (Vulkan's
 * vkAllocateMemory over VkImportMemoryFdInfoKHR, vkImportSemaphoreFdKHR, a GL
 * memory object), so who must close it after a refusal was decided at each
 * call site, and a descriptor leaked or closed twice wherever one got it
 * wrong. Held here, a refusal leaves it held and the holder going closes it. */
#ifndef AV_OWNED_FD_HPP
#define AV_OWNED_FD_HPP

namespace av {

class OwnedFd {
public:
    OwnedFd();
    /* `fd` held from now on; -1 holds nothing. */
    explicit OwnedFd(int fd);
    ~OwnedFd();
    OwnedFd(const OwnedFd &) = delete;
    OwnedFd &operator=(const OwnedFd &) = delete;
    OwnedFd(OwnedFd &&) = delete;
    OwnedFd &operator=(OwnedFd &&) = delete;

    /* `fd` held instead of what was, which is closed; -1 holds nothing. */
    void               hold(int fd);
    /* A new descriptor of what `fd` names held instead of what was; `fd`
     * stays its owner's. False, holding nothing, when none can be made. */
    [[nodiscard]] bool hold_copy(int fd);
    /* A new descriptor of what is held here, into `copy`. */
    [[nodiscard]] bool copy_to(OwnedFd &copy) const;
    /* The descriptor, still held: what an import is handed. */
    int                fd() const;
    bool               held() const;
    /* The descriptor handed on - an import took it, or a caller that owns it
     * from now on - and nothing held. */
    int                take();
    /* What is held closed. */
    void               close();

private:
    int fd_;
};

}  // namespace av

#endif  // AV_OWNED_FD_HPP
