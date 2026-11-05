#ifndef AV_GLICD_SLICE_PIN_HPP
#define AV_GLICD_SLICE_PIN_HPP

namespace av::glicd {

class SlicedBuffers;

class SlicePin {
public:
    SlicePin();
    ~SlicePin();
    SlicePin(const SlicePin &) = delete;
    SlicePin &operator=(const SlicePin &) = delete;

    void hold(SlicedBuffers *owner);

    bool invariants_hold() const;

private:
    SlicedBuffers *owner_;
};

}

#endif
