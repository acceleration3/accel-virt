/* The Direct3D resources one host GPU's sessions share, by key. A key opens
 * for anyone while its creator holds the resource, and for released_for_ns
 * after the creator lets it go - a refreshed page destroys each surface as DWM
 * is still opening it - and no other session's destroy lets it go: DWM
 * destroying its handle to a window's surface once left the window's creator
 * with a key no restarted DWM could open. Resources here are counted
 * stand-ins, so a reference taken or given back is a count that moved; times
 * are the test's. */
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_shared_resources.hpp"
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

/* Two sessions, by address, as the backend names them: two elements of one
 * array, which no compiler may fold into one. */
const char sessions[2] = {};
const void *const creator_session = &sessions[0];
const void *const opener_session = &sessions[1];

constexpr std::uint64_t start = 1000000u;
constexpr std::uint64_t grace = D3dSharedResources::released_for_ns;

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
  CHECK(table.publish(0x120C000000E1u, &surface, creator_session, start));
  CHECK_EQ(surface.references, 2u);   /* the table's own */
  IUnknown *const found = table.find(0x120C000000E1u);
  CHECK(found == &surface);
  CHECK_EQ(surface.references, 3u);
  found->Release();
  CHECK(table.find(0x51u) == nullptr);
  CHECK(table.find(0u) == nullptr);
  CHECK_EQ(table.count(), 1u);
  table.clear();
  CHECK_EQ(surface.references, 1u); }

/* DWM opened the window's surface and destroys its handle: the window still
 * draws into it, and the next DWM must open it again, however long after. */
TEST(d3d_shared_resources, another_session_destroying_its_handle_leaves_the_key)
{ D3dSharedResources &table = fresh_table();
  Counted surface;
  CHECK(table.publish(7u, &surface, creator_session, start));
  table.withdraw(&surface, opener_session, start + 1u);
  table.withdraw_all(opener_session, start + 2u);
  table.withdraw(&surface, opener_session, start + 10u * grace);
  CHECK(opens(table, 7u, &surface));
  CHECK_EQ(surface.references, 2u);
  table.clear(); }

/* The page destroyed its surface as DWM was opening it: the open still finds
 * it, and it goes on the first change to the table once the while is over. */
TEST(d3d_shared_resources, a_released_key_opens_for_a_while_and_then_goes)
{ D3dSharedResources &table = fresh_table();
  Counted surface, later;
  CHECK(table.publish(7u, &surface, creator_session, start));
  table.withdraw(&surface, creator_session, start + 5u);
  CHECK(opens(table, 7u, &surface));
  CHECK_EQ(surface.references, 2u);
  CHECK(table.publish(8u, &later, creator_session, start + 5u + grace));   /* exactly the while: kept */
  CHECK(opens(table, 7u, &surface));
  CHECK(table.publish(9u, &later, creator_session, start + 6u + grace));   /* past it: gone */
  CHECK(table.find(7u) == nullptr);
  CHECK_EQ(surface.references, 1u);
  /* Released twice: the first time counts. */
  CHECK(table.publish(10u, &surface, creator_session, start + 7u * grace));
  table.withdraw(&surface, creator_session, start + 7u * grace);
  table.withdraw(&surface, creator_session, start + 7u * grace + grace);
  CHECK(opens(table, 10u, &surface));
  table.withdraw_all(creator_session, start + 7u * grace + grace + 1u);
  CHECK(table.find(10u) == nullptr);
  CHECK_EQ(surface.references, 1u);
  table.clear(); }

TEST(d3d_shared_resources, a_session_that_ends_releases_only_its_own_keys)
{ D3dSharedResources &table = fresh_table();
  Counted mine[3], theirs;
  CHECK(table.publish(1u, &mine[0], creator_session, start));
  CHECK(table.publish(2u, &theirs, opener_session, start));
  CHECK(table.publish(3u, &mine[1], creator_session, start));
  CHECK(table.publish(4u, &mine[2], creator_session, start));
  table.withdraw_all(creator_session, start + 1u);
  CHECK_EQ(table.count(), 4u);   /* released, still open for the while */
  CHECK(opens(table, 3u, &mine[1]));
  table.withdraw_all(opener_session, start + 2u + grace);
  CHECK_EQ(table.count(), 1u);
  for (const Counted &each : mine) CHECK_EQ(each.references, 1u);
  CHECK(opens(table, 2u, &theirs));
  table.clear(); }

/* A key its creator let go of names whatever is shared under it next; one it
 * holds names nothing else. */
TEST(d3d_shared_resources, a_released_key_can_be_shared_again_and_a_held_one_cannot)
{ D3dSharedResources &table = fresh_table();
  Counted first, second;
  CHECK(!table.publish(0u, &first, creator_session, start));
  CHECK(!table.publish(9u, nullptr, creator_session, start));
  CHECK(table.publish(9u, &first, creator_session, start));
  CHECK(!table.publish(9u, &second, opener_session, start));
  CHECK_EQ(second.references, 1u);
  table.withdraw(&first, creator_session, start + 1u);
  CHECK(table.publish(9u, &second, opener_session, start + 2u));
  CHECK_EQ(first.references, 1u);
  CHECK(opens(table, 9u, &second));
  CHECK_EQ(table.count(), 1u);
  table.clear(); }

/* Full, the table makes room by letting the longest-released resource go;
 * with nothing released it refuses. */
TEST(d3d_shared_resources, a_full_table_gives_up_the_oldest_released_and_otherwise_refuses)
{ D3dSharedResources &table = fresh_table();
  Counted held, early, late, extra;
  for (std::uint64_t key = 1u; table.count() < D3dSharedResources::capacity; key++)
      CHECK(table.publish(key, &held, creator_session, start));
  CHECK(!table.publish(0x10000u, &extra, opener_session, start));
  CHECK_EQ(extra.references, 1u);
  table.clear();
  CHECK_EQ(held.references, 1u);
  CHECK(table.publish(1u, &late, creator_session, start));
  CHECK(table.publish(2u, &early, opener_session, start));
  for (std::uint64_t key = 3u; table.count() < D3dSharedResources::capacity; key++)
      CHECK(table.publish(key, &held, opener_session, start));
  table.withdraw(&early, opener_session, start + 10u);
  table.withdraw(&late, creator_session, start + 20u);
  CHECK(table.publish(0x10000u, &extra, opener_session, start + 30u));
  CHECK(table.find(2u) == nullptr);   /* released first: given up */
  CHECK_EQ(early.references, 1u);
  CHECK(opens(table, 1u, &late));
  CHECK(opens(table, 0x10000u, &extra));
  table.clear();
  CHECK_EQ(held.references, 1u);
  CHECK_EQ(late.references, 1u);
  CHECK_EQ(extra.references, 1u); }
