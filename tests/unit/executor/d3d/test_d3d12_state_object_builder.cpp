#include "executor/d3d/d3d12_state_object_builder.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

using av::D3dObjectKind;
using av::D3dObjectTable;
using av::D3d12StateObjectBuilder;
using av::MockPlatform;
using av::Status;
using namespace av::d3d12proto;

namespace {

class Held final : public IUnknown {
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

struct Names {
    std::vector<std::uint16_t> units;

    std::uint32_t add(const char *text)
    {
        const std::uint32_t at = static_cast<std::uint32_t>(units.size() * 2u);
        for (const char *each = text; *each != '\0'; each++) units.push_back(static_cast<std::uint16_t>(*each));
        units.push_back(0u);
        return at;
    }
    const std::uint8_t *bytes() const { return reinterpret_cast<const std::uint8_t *>(units.data()); }
    std::uint64_t size() const { return units.size() * 2u; }
};

constexpr std::uint32_t none = 0xFFFFFFFFu;

struct Rig {
    MockPlatform platform;
    D3dObjectTable objects{platform};
    D3d12StateObjectBuilder builder{platform, objects};
    Held root, other, collection;
    std::vector<std::uint8_t> library = std::vector<std::uint8_t>(64u, 1u);
    Names names;
    std::vector<StateSubobject> subobjects;
    std::vector<StateExport> exports;
    std::vector<StateAssociation> associations;
    std::uint32_t type = 3u;

    Rig()
    {
        library[0] = 'D';
        library[1] = 'X';
        library[2] = 'B';
        library[3] = 'C';
        static_cast<void>(objects.insert(5u, D3dObjectKind::root_signature12, &root));
        static_cast<void>(objects.insert(6u, D3dObjectKind::root_signature12, &other));
        static_cast<void>(objects.insert(7u, D3dObjectKind::state_object12, &collection));
    }

    D3d12StateObjectBuilder::Wire wire() const
    {
        return D3d12StateObjectBuilder::Wire{type, static_cast<std::uint32_t>(subobjects.size()), subobjects.data(),
                                             static_cast<std::uint32_t>(exports.size()), exports.data(),
                                             static_cast<std::uint32_t>(associations.size()), associations.data(),
                                             names.bytes(), names.size(), library.data(), library.size()};
    }
};

StateSubobject make(std::uint32_t type, std::uint32_t object = 0u, std::uint32_t a = 0u, std::uint32_t b = 0u)
{
    return StateSubobject{type, object, 0u, 0u, a, b, 0u, 0u};
}

}

TEST(d3d12_state_object_builder_read, builds_a_pipeline_of_a_library_a_hit_group_configs_and_root_signatures)
{ Rig rig;
  StateSubobject library = make(5u, 0u, 0u, 64u);
  library.first = 0u;
  library.count = 2u;
  rig.exports.push_back(StateExport{rig.names.add("raygen"), none, 0u, 0u});
  rig.exports.push_back(StateExport{rig.names.add("miss"), rig.names.add("miss_main"), 0u, 0u});
  StateSubobject hit = make(11u, 0u, 0u, none);
  hit.first = rig.names.add("HG");
  hit.c = rig.names.add("chit");
  hit.d = none;
  rig.subobjects = {library, hit, make(9u, 0u, 16u, 8u), make(10u, 0u, 3u, 0u), make(1u, 5u), make(2u, 6u)};
  rig.associations.push_back(StateAssociation{5u, rig.names.add("HG")});
  CHECK(rig.builder.read(rig.wire()));
  const auto &desc = rig.builder.desc();
  CHECK_EQ(desc.type, 3);
  CHECK_EQ(desc.count, 7u);
  CHECK_EQ(desc.subobjects[0].type, 5);
  CHECK_EQ(desc.subobjects[1].type, 11);
  CHECK_EQ(desc.subobjects[3].type, 10);
  CHECK_EQ(desc.subobjects[6].type, 7);
  const auto *made_library = static_cast<const av::d3d12new::LibraryDesc *>(desc.subobjects[0].desc);
  CHECK_EQ(made_library->export_count, 2u);
  CHECK_EQ(made_library->code.bytes, 64u);
  CHECK(made_library->exports[1].rename != nullptr);
  CHECK(made_library->exports[0].rename == nullptr);
  const auto *made_hit = static_cast<const av::d3d12new::HitGroupDesc *>(desc.subobjects[1].desc);
  CHECK(made_hit->any_hit == nullptr);
  CHECK(made_hit->closest_hit != nullptr);
  CHECK(made_hit->closest_hit[0] == u'c');
  const auto *made_association = static_cast<const av::d3d12new::AssociationDesc *>(desc.subobjects[6].desc);
  CHECK_EQ(made_association->export_count, 1u);
  CHECK(made_association->subobject == &desc.subobjects[5]);
  CHECK(made_association->exports[0][0] == u'H'); }

TEST(d3d12_state_object_builder_read, a_pipeline_config_with_flags_is_the_second_revision)
{ Rig rig;
  rig.subobjects = {make(10u, 0u, 2u, 0x100u)};
  CHECK(rig.builder.read(rig.wire()));
  CHECK_EQ(rig.builder.desc().subobjects[0].type, 12);
  rig.subobjects = {make(10u, 0u, 2u, 0x1u)};
  CHECK(!rig.builder.read(rig.wire())); }

TEST(d3d12_state_object_builder_read, groups_the_associations_of_one_subobject_into_one_association)
{ Rig rig;
  rig.subobjects = {make(2u, 6u), make(2u, 5u)};
  rig.associations = {StateAssociation{1u, rig.names.add("a")}, StateAssociation{0u, rig.names.add("b")},
                      StateAssociation{1u, rig.names.add("c")}};
  CHECK(rig.builder.read(rig.wire()));
  CHECK_EQ(rig.builder.desc().count, 4u);
  const auto *first = static_cast<const av::d3d12new::AssociationDesc *>(rig.builder.desc().subobjects[2].desc);
  const auto *second = static_cast<const av::d3d12new::AssociationDesc *>(rig.builder.desc().subobjects[3].desc);
  CHECK_EQ(first->export_count, 1u);
  CHECK_EQ(second->export_count, 2u);
  CHECK(first->subobject == &rig.builder.desc().subobjects[0]);
  CHECK(second->exports[1][0] == u'c'); }

TEST(d3d12_state_object_builder_read, refuses_an_object_the_session_does_not_hold)
{ Rig rig;
  rig.subobjects = {make(1u, 99u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(6u, 5u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(6u, 7u)};
  CHECK(rig.builder.read(rig.wire())); }

TEST(d3d12_state_object_builder_read, refuses_a_library_or_name_outside_what_the_wire_carries)
{ Rig rig;
  rig.subobjects = {make(5u, 0u, 8u, 64u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(5u, 0u, 0u, 8u)};
  CHECK(!rig.builder.read(rig.wire()));
  StateSubobject library = make(5u, 0u, 0u, 64u);
  library.count = 1u;
  rig.subobjects = {library};
  CHECK(!rig.builder.read(rig.wire()));
  rig.exports.push_back(StateExport{rig.names.add("x"), 400u, 0u, 0u});
  CHECK(!rig.builder.read(rig.wire()));
  rig.exports[0].rename = none;
  CHECK(rig.builder.read(rig.wire()));
  rig.associations.push_back(StateAssociation{0u, 2u * 40u});
  CHECK(!rig.builder.read(rig.wire())); }

TEST(d3d12_state_object_builder_read, refuses_names_that_are_odd_unterminated_or_misplaced)
{ Rig rig;
  rig.subobjects = {make(3u, 0u, 0u)};
  rig.names.units = {u'a', u'b'};
  CHECK(!rig.builder.read(rig.wire()));
  rig.names.units = {u'a', 0u};
  CHECK(rig.builder.read(rig.wire()));
  rig.associations.push_back(StateAssociation{0u, 1u});
  CHECK(!rig.builder.read(rig.wire())); }

TEST(d3d12_state_object_builder_read, refuses_a_type_a_subobject_count_or_value_out_of_range)
{ Rig rig;
  rig.subobjects = {};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(3u, 0u, 0u)};
  rig.type = 4u;
  CHECK(!rig.builder.read(rig.wire()));
  rig.type = 0u;
  CHECK(rig.builder.read(rig.wire()));
  rig.subobjects = {make(7u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(9u, 0u, 1u << 24u, 8u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(9u, 0u, 16u, 64u)};
  CHECK(!rig.builder.read(rig.wire()));
  rig.subobjects = {make(10u, 0u, 99u, 0u)};
  CHECK(!rig.builder.read(rig.wire())); }

TEST(d3d12_state_object_builder_read, a_library_that_is_a_bare_dxil_program_is_wrapped_in_a_container)
{ Rig rig;
  rig.library.assign(64u, 0u);
  const std::uint32_t head[3] = {0x00060060u, 16u, 0x4c495844u};
  std::memcpy(rig.library.data(), head, sizeof head);
  rig.subobjects = {make(5u, 0u, 0u, 64u)};
  CHECK(rig.builder.read(rig.wire()));
  const auto *made = static_cast<const av::d3d12new::LibraryDesc *>(rig.builder.desc().subobjects[0].desc);
  const auto *bytes = static_cast<const std::uint8_t *>(made->code.code);
  CHECK(bytes[0] == 'D' && bytes[1] == 'X' && bytes[2] == 'B' && bytes[3] == 'C');
  CHECK(made->code.bytes > 64u);
  rig.library.assign(64u, 7u);
  CHECK(!rig.builder.read(rig.wire())); }
