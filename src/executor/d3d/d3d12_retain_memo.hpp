#ifndef AV_D3D12_RETAIN_MEMO_HPP
#define AV_D3D12_RETAIN_MEMO_HPP

#include <stdint.h>

struct IUnknown;

namespace av {

class D3d12BlockHeap;

class D3d12RetainMemo {
public:
    D3d12RetainMemo();
    D3d12RetainMemo(const D3d12RetainMemo &) = delete;
    D3d12RetainMemo &operator=(const D3d12RetainMemo &) = delete;

    bool covers(uint32_t list, const IUnknown *object, const D3d12BlockHeap *pin) const;
    bool whole(uint32_t list, const IUnknown *object) const;
    void note(uint32_t list, const IUnknown *object, bool pinned);
    void note_whole(uint32_t list, const IUnknown *object);
    void clear();

private:
    static constexpr uint32_t ways = 8u;

    struct Entry {
        uint32_t        list_;
        const IUnknown *object_;
        bool            pinned_;
        bool            whole_;
    };

    uint32_t way_of(const IUnknown *object) const;

    Entry entries_[ways];
};

}

#endif
