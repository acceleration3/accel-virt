/* The Direct3D resources one host GPU's sessions share, by the id the driver
 * minted over each one's allocation. A key opens for anyone until the guest
 * lets go of it everywhere - the driver's release - and nothing a session does
 * takes it out: DWM opens a window's surface as the window is destroying it.
 * Resources here are counted stand-ins, so a reference taken or given back is
 * a count that moved. */
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_shared_resources.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dSharedResources;

namespace {

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }

    ULONG references = 1u;
};

D3dSharedResources &fresh_table()
{
    static D3dSharedResources table;
    table.clear();
    return table;
}

/* Whether `key` opens now, giving the reference it took back. */
bool opens(const D3dSharedResources &table, std::uint64_t key, const IUnknown *expected)
{
    IUnknown *const found = table.find(key);
    if (found == nullptr) return false;
    found->Release();
    return found == expected;
}

}  // namespace

TEST(d3d_shared_resources, a_key_opens_with_a_reference_of_the_caller_s_own)
{ D3dSharedResources &table = fresh_table();
  Counted surface;
  CHECK(table.publish(0x2Au, &surface));
  CHECK_EQ(surface.references, 2u);   /* the table's own */
  IUnknown *const found = table.find(0x2Au);
  CHECK(found == &surface);
  CHECK_EQ(surface.references, 3u);
  found->Release();
  CHECK(table.find(0x51u) == nullptr);
  CHECK(table.find(0u) == nullptr);
  CHECK_EQ(table.count(), 1u);
  table.clear();
  CHECK_EQ(surface.references, 1u); }

/* The guest's release is the one thing that lets a key go: once, and for that
 * key alone. */
TEST(d3d_shared_resources, the_guest_s_release_lets_the_key_go_and_nothing_else)
{ D3dSharedResources &table = fresh_table();
  Counted first, second;
  CHECK(table.publish(7u, &first));
  CHECK(table.publish(8u, &second));
  table.release(9u);   /* never shared: nothing to do */
  CHECK_EQ(table.count(), 2u);
  table.release(7u);
  CHECK(table.find(7u) == nullptr);
  CHECK_EQ(first.references, 1u);
  CHECK(opens(table, 8u, &second));
  table.release(7u);   /* released twice: the second is nothing */
  CHECK_EQ(first.references, 1u);
  CHECK_EQ(table.count(), 1u);
  table.clear();
  CHECK_EQ(second.references, 1u); }

/* A key already shared names what it names; a new resource under it is
 * refused rather than taking its place. */
TEST(d3d_shared_resources, a_zero_key_a_second_share_and_a_full_table_are_refused)
{ D3dSharedResources &table = fresh_table();
  Counted surface, other;
  CHECK(!table.publish(0u, &surface));
  CHECK(!table.publish(9u, nullptr));
  CHECK(table.publish(9u, &surface));
  CHECK(!table.publish(9u, &other));
  CHECK_EQ(other.references, 1u);
  CHECK(opens(table, 9u, &surface));
  for (std::uint64_t key = 10u; table.count() < D3dSharedResources::capacity; key++) CHECK(table.publish(key, &other));
  CHECK(!table.publish(1u, &other));
  CHECK_EQ(other.references, 1u + D3dSharedResources::capacity - 1u);
  table.release(9u);
  CHECK(table.publish(1u, &surface));   /* room again */
  table.clear();
  CHECK_EQ(surface.references, 1u);
  CHECK_EQ(other.references, 1u); }

/* Copies into a shared surface are counted per key from the moment it is
 * shared, survive another key leaving the table (whose last row moves into
 * the gap) and another API's view replacing the row, and start again at 0
 * when the key comes back; a key the table does not hold counts nothing. */
TEST(d3d_shared_resources, copies_are_counted_per_key_from_the_moment_it_is_shared)
{ D3dSharedResources &table = fresh_table();
  Counted first, second, view;
  std::uint64_t count = 99u;
  CHECK(!table.writes(5u, count));
  CHECK_EQ(count, 0u);
  CHECK(table.publish(5u, &first));
  CHECK(table.publish(6u, &second));
  CHECK(table.writes(5u, count));
  CHECK_EQ(count, 0u);
  table.written(6u);
  table.written(6u);
  table.written(5u);
  table.written(7u);   /* not shared: nothing */
  table.written(0u);
  CHECK(!table.writes(7u, count));
  CHECK(!table.writes(0u, count));
  CHECK(table.writes(5u, count));
  CHECK_EQ(count, 1u);
  table.release(5u);   /* 6's row moves into 5's */
  CHECK(table.writes(6u, count));
  CHECK_EQ(count, 2u);
  CHECK(table.replace(6u, &view));
  table.written(6u);
  CHECK(table.writes(6u, count));
  CHECK_EQ(count, 3u);
  CHECK(!table.writes(5u, count));
  CHECK(table.publish(5u, &first));
  CHECK(table.writes(5u, count));
  CHECK_EQ(count, 0u);
  table.clear();
  CHECK_EQ(first.references, 1u);
  CHECK_EQ(second.references, 1u);
  CHECK_EQ(view.references, 1u); }
