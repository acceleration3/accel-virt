#ifndef AV_D3D9_LANES_HPP
#define AV_D3D9_LANES_HPP

#include "core/support/object_pool.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_lane.hpp"

#include <stdint.h>

struct IDirect3DDevice9;

namespace av {

class BlockSource;
class DriverLog;
class EventfdWake;

class D3d9Lanes {
public:
    D3d9Lanes(BlockSource &blocks, EventfdWake &wake, DriverLog &lines);
    D3d9Lanes(const D3d9Lanes &) = delete;
    D3d9Lanes &operator=(const D3d9Lanes &) = delete;

    D3d9Lane *lane(IDirect3DDevice9 *device);
    D3d9Lane *find(const IDirect3DDevice9 *device);
    D3d9Lane *find(const SliceImporter *importer);
    void      touch(IDirect3DDevice9 *device);
    void      mark_touched();
    void      forget(IDirect3DDevice9 *device);
    void      forget_all();
    void      begin_session();
    template <typename Visitor>
    void      for_each(Visitor visit) const;

    bool     marking_failed() const;
    bool     polled() const;
    uint64_t next() const;
    uint64_t marked() const;
    uint64_t reached() const;

    bool invariants_hold() const;

private:
    ObjectPool<D3d9Lane> lanes_;
    EventfdWake         &wake_;
    DriverLog           &lines_;
    uint64_t             marked_;
    bool                 marking_failed_;
};

template <typename Visitor>
void D3d9Lanes::for_each(Visitor visit) const
{
    AV_SCOPE("D3d9Lanes::for_each");
    lanes_.for_each(visit);
}

}

#endif
