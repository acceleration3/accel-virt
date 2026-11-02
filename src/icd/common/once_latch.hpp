#ifndef AV_ICD_ONCE_LATCH_HPP
#define AV_ICD_ONCE_LATCH_HPP

namespace av::icd {

class OnceLatch {
public:
    OnceLatch();

    [[nodiscard]] bool first();

private:
    bool seen_;
};

}

#endif
