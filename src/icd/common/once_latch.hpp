/* Whether something is happening for the first time. A refusal that recurs -
 * a window refusing every frame, a kernel refusing every present - is logged
 * the first time and not again, so the log says what went wrong rather than
 * how often. Every ICD has such refusals; KeyedLog remembers many keys, and
 * costs its table, where one refusal needs one bit. */
#ifndef AV_ICD_ONCE_LATCH_HPP
#define AV_ICD_ONCE_LATCH_HPP

namespace av::icd {

class OnceLatch {
public:
    OnceLatch();

    /* True the first time it is asked, false every time after. */
    [[nodiscard]] bool first();

private:
    bool seen_;
};

}  // namespace av::icd

#endif  // AV_ICD_ONCE_LATCH_HPP
