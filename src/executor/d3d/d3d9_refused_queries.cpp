#include "executor/d3d/d3d9_refused_queries.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d9RefusedQueries::D3d9RefusedQueries(BlockSource &blocks) : refused_(blocks, Pool::paged)
{
    AV_SCOPE("D3d9RefusedQueries::D3d9RefusedQueries");
}

bool D3d9RefusedQueries::note(uint32_t query, uint32_t hresult)
{
    AV_SCOPE("D3d9RefusedQueries::note");
    if (query == 0u || refused_.count() >= most) return false;
    static_cast<void>(refused_.erase(query));
    uint32_t slot = 0u;
    return refused_.insert(query, hresult, slot) == Status::ok;
}

bool D3d9RefusedQueries::answer(uint32_t query, uint32_t &hresult) const
{
    AV_SCOPE("D3d9RefusedQueries::answer");
    const uint32_t *const found = refused_.find(query);
    if (found == nullptr) return false;
    hresult = *found;
    return true;
}

bool D3d9RefusedQueries::forget(uint32_t query)
{
    AV_SCOPE("D3d9RefusedQueries::forget");
    return refused_.erase(query);
}

void D3d9RefusedQueries::clear()
{
    AV_SCOPE("D3d9RefusedQueries::clear");
    refused_.clear();
}

uint32_t D3d9RefusedQueries::count() const
{
    AV_SCOPE("D3d9RefusedQueries::count");
    return refused_.count();
}

bool D3d9RefusedQueries::invariants_hold() const
{
    AV_SCOPE("D3d9RefusedQueries::invariants_hold");
    return refused_.invariants_hold() && refused_.count() <= most;
}

}
