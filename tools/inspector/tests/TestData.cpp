#include "TestFramework.h"

#include "data/History.h"
#include "data/ScopeNames.h"
#include "data/StateDump.h"
#include "data/TraceTable.h"

#include <string.h>

using namespace inspector;

namespace {

template <typename T>
void Put(DynamicArray<u8>& bytes, usize offset, T value) {
    memcpy(bytes.GetBuffer() + offset, &value, sizeof value);
}

DynamicArray<u8> EmptyTable(u32 slots, u32 frames) {
    DynamicArray<u8> bytes;
    bytes.Resize(96 + static_cast<usize>(slots) * (56 + frames * 24));
    memset(bytes.GetBuffer(), 0, bytes.GetSize());
    memcpy(bytes.GetBuffer(), "AVTRACE", 8);
    Put<u32>(bytes, 8, 1);
    Put<u32>(bytes, 12, 96);
    Put<u32>(bytes, 16, slots);
    Put<u32>(bytes, 20, 56 + frames * 24);
    Put<u32>(bytes, 24, frames);
    Put<u32>(bytes, 28, 24);
    memcpy(bytes.GetBuffer() + 32, "device", 6);
    Put<u32>(bytes, 48, 1234);
    Put<u64>(bytes, 64, 10'000'000);
    return bytes;
}

}

TEST_CASE("TraceTable/decodes an active slot's frames and skips free slots") {
    DynamicArray<u8> bytes = EmptyTable(4, 3);
    const usize slot = 96 + 2 * (56 + 3 * 24);
    Put<u64>(bytes, slot, 0x55ull | (1ull << 63));
    Put<u32>(bytes, slot + 8, 5);
    Put<u32>(bytes, slot + 12, 4321);
    Put<u64>(bytes, slot + 16, 99);
    Put<u64>(bytes, slot + 40, 0xABC);
    for (u32 f = 0; f < 3; ++f) {
        Put<u32>(bytes, slot + 56 + f * 24, 100 + f);
        Put<u64>(bytes, slot + 56 + f * 24 + 8, 1000 + f);
    }
    String why;
    const Optional<TraceTable> table = ParseTraceTable(bytes.AsSpan(), why);
    REQUIRE(table.HasValue());
    CHECK(table->module == StringView("device"));
    CHECK_EQ(table->pid, 1234u);
    CHECK_EQ(table->slot_count, 4u);
    REQUIRE(table->slots.GetSize() == 1);
    const TraceSlot& s = table->slots[0];
    CHECK(s.active);
    CHECK_EQ(s.key, 0x55ull);
    CHECK_EQ(s.thread, 4321u);
    CHECK_EQ(s.depth, 5u);
    CHECK_EQ(s.waiting_on, 0xABCull);
    REQUIRE(s.frames.GetSize() == 3);
    CHECK_EQ(s.frames[2].name_id, 102u);
    CHECK_EQ(s.frames[2].entered, 1002ull);
}

TEST_CASE("TraceTable/an idle slot keeps no frames") {
    DynamicArray<u8> bytes = EmptyTable(1, 2);
    Put<u64>(bytes, 96, 0x77ull);
    Put<u32>(bytes, 96 + 8, 2);
    Put<u32>(bytes, 96 + 24, 555);
    String why;
    const Optional<TraceTable> table = ParseTraceTable(bytes.AsSpan(), why);
    REQUIRE(table.HasValue());
    REQUIRE(table->slots.GetSize() == 1);
    CHECK(!table->slots[0].active);
    CHECK(table->slots[0].frames.IsEmpty());
    CHECK_EQ(table->slots[0].last_left, 555u);
}

TEST_CASE("TraceTable/refuses what is not a whole version 1 table") {
    String why;
    DynamicArray<u8> bytes = EmptyTable(2, 2);
    CHECK(!ParseTraceTable(bytes.AsSpan().Subspan(0, 50), why).HasValue());
    CHECK(!ParseTraceTable(bytes.AsSpan().Subspan(0, bytes.GetSize() - 1), why).HasValue());
    DynamicArray<u8> wrong = EmptyTable(2, 2);
    Put<u32>(wrong, 8, 2);
    CHECK(!ParseTraceTable(wrong.AsSpan(), why).HasValue());
    DynamicArray<u8> lying = EmptyTable(2, 2);
    Put<u32>(lying, 20, 999);
    CHECK(!ParseTraceTable(lying.AsSpan(), why).HasValue());
    DynamicArray<u8> magic = EmptyTable(2, 2);
    magic[0] = 'X';
    CHECK(!ParseTraceTable(magic.AsSpan(), why).HasValue());
}

TEST_CASE("StateDump/builds the tree from paths, making parents never written") {
    const StateDump dump = StateDump::Parse(
        "module/0 devices=2\n"
        "module/0/device/0/contexts/0/context/193 bound=true ring_order=22 last_fence=20\n"
        "module/0/device/0 realized=true gpu=0\n"
        "executor/0/gpu/1 uuid=000000007B sub_apis=0x0F\n");
    const StateNode* context = dump.Find("module/0/device/0/contexts/0/context/193");
    REQUIRE(context != nullptr);
    CHECK(context->kind == StringView("context"));
    CHECK(context->index == StringView("193"));
    CHECK(context->Flag("bound"));
    CHECK_EQ(context->Integer("last_fence").ValueOr(-1), 20);
    const StateNode* contexts = dump.Find("module/0/device/0/contexts/0");
    REQUIRE(contexts != nullptr);
    CHECK(contexts->fields.IsEmpty());
    const StateNode* device = dump.Ancestor(*context, "device");
    REQUIRE(device != nullptr);
    CHECK_EQ(device->Integer("gpu").ValueOr(-1), 0);
    CHECK_EQ(dump.GetRoots().GetSize(), 2u);
    CHECK_EQ(dump.Find("executor/0/gpu/1")->Integer("sub_apis").ValueOr(0), 15);
    CHECK_EQ(dump.OfKind("context").GetSize(), 1u);
}

TEST_CASE("StateDump/keeps a binary's refusal as its error") {
    const StateDump dump = StateDump::Parse("error no dump before the device is realized\n");
    CHECK(dump.IsEmpty());
    CHECK(dump.GetError() == StringView("no dump before the device is realized"));
}

TEST_CASE("ScopeNames/hashes as the tables do and finds every AV_SCOPE literal") {
    CHECK_EQ(ScopeNames::Hash("a"), 0xE40C292Cu);
    ScopeNames names;
    names.AddSource("void A::b() {\n    AV_SCOPE(\"A::b\");\n}\nvoid C() { AV_SCOPE(\"C\"); }");
    CHECK_EQ(names.GetSize(), 2u);
    CHECK(names.Find(ScopeNames::Hash("A::b")) == StringView("A::b"));
    CHECK(names.Find(1u) == StringView("?00000001"));
}

TEST_CASE("History/a counter's rate over a window") {
    History history;
    for (int i = 0; i <= 10; ++i) {
        history.Record("k", 100.0 * i, 0.5 * i);
    }
    const Series* series = history.Find("k");
    REQUIRE(series != nullptr);
    CHECK_NEAR(series->Rate(2.0).ValueOr(0.0), 200.0, 1e-9);
    CHECK_NEAR(series->Last().ValueOr(0.0), 1000.0, 1e-9);
    History one;
    one.Record("k", 1.0, 0.0);
    CHECK(!one.Find("k")->Rate(1.0).HasValue());
}

TEST_CASE("History/prune forgets series that stopped being recorded") {
    History history;
    history.Record("gone", 1.0, 0.0);
    history.Record("live", 1.0, 0.0);
    history.Record("live", 2.0, History::kWindowSeconds + 10.0);
    CHECK(history.GetSize() == 2);
    CHECK(history.Prune(History::kWindowSeconds + 10.0) == 1);
    CHECK(history.GetSize() == 1);
    CHECK(history.Find("live") != nullptr);
    CHECK(history.Find("gone") == nullptr);
}
