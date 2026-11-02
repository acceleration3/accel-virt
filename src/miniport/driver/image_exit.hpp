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

}

#endif
