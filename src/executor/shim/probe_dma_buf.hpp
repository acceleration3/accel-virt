/* One page of this process as a dma-buf, made to ask a driver what it would
 * import.
 *
 * The guest's pages reach a host GPU as a dma-buf over guest RAM, made by the
 * device module; but an application asks which memory types it may import into
 * before any guest page exists, and the answer is the driver's rather than the
 * buffer's. So the question is asked with a descriptor of this process's own
 * making: a memfd wrapped by udmabuf, which is the same kind of object the
 * device module hands over later.
 *
 * A host without /dev/udmabuf makes none, and that is an answer: the library
 * then reports no importable type for a dma-buf and the host pointer is used. */
#ifndef AV_PROBE_DMA_BUF_HPP
#define AV_PROBE_DMA_BUF_HPP

namespace av {

class ProbeDmaBuf {
public:
    /* What udmabuf's list entries are measured in, and the smallest buffer
     * that can be made. */
    static constexpr unsigned page_size = 4096u;

    ProbeDmaBuf();
    ~ProbeDmaBuf();
    ProbeDmaBuf(const ProbeDmaBuf &) = delete;
    ProbeDmaBuf &operator=(const ProbeDmaBuf &) = delete;

    /* One page as a descriptor, or false with nothing held. */
    [[nodiscard]] bool make();
    void               release();
    /* The descriptor, or -1 before one is made. */
    int                handle() const;

private:
    int memory_;
    int shared_;
};

}  // namespace av

#endif  // AV_PROBE_DMA_BUF_HPP
