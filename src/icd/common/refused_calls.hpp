#ifndef AV_ICD_REFUSED_CALLS_HPP
#define AV_ICD_REFUSED_CALLS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class KeyedLog;
}

namespace av::icd {

class RefusedCalls {
public:
    explicit RefusedCalls(KeyedLog &lines);
    RefusedCalls(const RefusedCalls &) = delete;
    RefusedCalls &operator=(const RefusedCalls &) = delete;

    void unserved(const char *entry);
    void failed(const char *entry, int64_t code);
    void dropped(const char *api, const char *entry, size_t written);

private:
    static constexpr int64_t hresult_floor = -0x70000000ll;
    static constexpr int64_t dropped_code = INT64_MIN;

    KeyedLog &lines_;
};

}

#endif
