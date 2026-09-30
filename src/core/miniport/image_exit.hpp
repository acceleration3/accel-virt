/* What DxgkDdiUnload ends with: the driver image let go of whole - the
 * miniport, the trace table and the platform - before dxgkrnl unmaps the code
 * they run. The kernel's image and the harness's are the two that leave. */
#ifndef AV_IMAGE_EXIT_HPP
#define AV_IMAGE_EXIT_HPP

namespace av {

class ImageExit {
public:
    ImageExit(const ImageExit &) = delete;
    ImageExit &operator=(const ImageExit &) = delete;

    virtual void leave() = 0;

protected:
    ImageExit() = default;
    ~ImageExit() = default;
};

}  // namespace av

#endif  // AV_IMAGE_EXIT_HPP
