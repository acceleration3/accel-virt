/* Memory whose written pages Windows keeps track of (MEM_WRITE_WATCH): the
 * shadow of a large dynamic buffer, so an unmap carries to the host the pages
 * the application wrote rather than the whole buffer. A text overlay that
 * discards a 192 KiB vertex buffer to write a few kilobytes of it sent all of
 * it every frame, and the host copied all of it again. */
#ifndef AV_UM_D3D_WRITTEN_PAGES_HPP
#define AV_UM_D3D_WRITTEN_PAGES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d11 {

class WrittenPages {
public:
    WrittenPages();
    WrittenPages(const WrittenPages &) = delete;
    WrittenPages &operator=(const WrittenPages &) = delete;

    /* Zeroed and watched; nullptr when the system has none to give. */
    [[nodiscard]] uint8_t *allocate(size_t bytes);
    void                   release(uint8_t *block);
    /* The part of [at, at + bytes) written since it was last asked about, as
     * byte offsets from `at`: from the first written page to the end of the
     * last, cut to the range; `end` is 0 when nothing was. The whole range
     * when the pages written are more than this can list. */
    void written(uint8_t *at, size_t bytes, size_t &first, size_t &end);

private:
    static constexpr size_t   page_bytes = 4096u;
    static constexpr uint32_t max_pages = 512u;

    void *pages_[max_pages];
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D_WRITTEN_PAGES_HPP
