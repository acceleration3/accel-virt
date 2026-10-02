import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_design.py"


class DesignRules(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)

    def test_reports_a_static_member_function(self):
        self.tree.write("src/core/bad.hpp",
                        "class Loose {\npublic:\n    static int f();\nprivate:\n    int x_;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/bad.hpp: Loose::f is static", result.stderr)

    def test_reports_a_class_with_no_data_members(self):
        self.tree.write("src/device/bad.hpp", "class Empty {\npublic:\n    void g();\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/device/bad.hpp: class Empty has no data members", result.stderr)

    def test_a_pure_abstract_contract_is_not_stateless(self):
        self.tree.write("src/proto/contract.hpp",
                        "class Contract {\npublic:\n    virtual ~Contract();\n"
                        "    virtual int f() = 0;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_ddi_record_holds_state_in_wdk_named_fields(self):
        self.tree.write("src/miniport/ddi/thing_record.hpp",
                        "struct ThingRecord {\n    uint32_t Size;\n    void *DeviceHandle;\n"
                        "    DxgkCbQueueDpc DxgkCbQueueDpc;\n    void *LaterCallbacks[49];\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_ddi_record_with_a_member_function_is_reported(self):
        self.tree.write("src/miniport/ddi/thing_record.hpp",
                        "struct ThingRecord {\n    uint32_t Size;\n    uint32_t size() const;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/miniport/ddi/thing_record.hpp: record ThingRecord declares member functions",
                      result.stderr)

    def test_wdk_named_fields_are_not_state_outside_a_ddi_record(self):
        self.tree.write("src/core/loose.hpp", "struct LooseRecord {\n    uint32_t Size;\n};\n")
        self.tree.write("src/miniport/ddi/loose.hpp", "struct Loose {\n    uint32_t Size;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/loose.hpp: class LooseRecord has no data members", result.stderr)
        self.assertIn("src/miniport/ddi/loose.hpp: class Loose has no data members", result.stderr)

    def test_a_record_of_lowercase_wdk_field_names_has_state(self):
        self.tree.write("src/miniport/ddi/table_record.hpp",
                        "using GetTopology = int (*)(void *vidpn);\n"
                        "struct TableRecord {\n    GetTopology pfnGetTopology;\n    void *hAllocation;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_an_array_of_objects_is_state(self):
        self.tree.write("src/core/list.hpp",
                        "class List {\npublic:\n    int count() const;\nprivate:\n    Mode modes_[count_limit];\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_members_after_a_nested_type_are_the_enclosing_class_s_state(self):
        """A table of complete slots is declared after its slot type, so a
        class's members often follow a nested struct."""
        self.tree.write("src/device/table.hpp",
                        "class Table {\npublic:\n    int count() const;\nprivate:\n"
                        "    struct Slot {\n        int id_;\n    };\n"
                        "    Slot slots_[4];\n    int count_;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_nested_type_is_judged_as_a_class_of_its_own(self):
        self.tree.write("src/device/table.hpp",
                        "class Table {\nprivate:\n    struct Slot {\n        int id;\n    };\n"
                        "    struct Pair { int a_; int b_; };\n    struct Bare { int a; };\n    static int f();\n    Slot slots_[4];\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(sorted(result.stderr.strip().splitlines()),
                         ["src/device/table.hpp: Table::f is static",
                          "src/device/table.hpp: class Bare has no data members",
                          "src/device/table.hpp: class Slot has no data members"])

    def test_does_not_judge_whether_symbols_are_tested(self):
        self.tree.write("src/core/fine.hpp",
                        "class Fine {\npublic:\n    int never_named();\nprivate:\n    int v_;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("design_rules: ok", result.stdout)

    def test_reports_a_new_expression_outside_object_storage(self):
        self.tree.write("src/core/pool.cpp", "void *f(void *p) { return ::new (p) int(3); }\n")
        self.tree.write("src/device/heap.cpp", "int *g() { return new int; }\nvoid h(int *p) { delete p; }\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/pool.cpp:1: new expression outside src/core/support/object_storage.hpp "
                      "and src/core/support/object_pool.hpp", result.stderr)
        self.assertIn("src/device/heap.cpp:1: new expression outside src/core/support/object_storage.hpp "
                      "and src/core/support/object_pool.hpp", result.stderr)
        self.assertIn("src/device/heap.cpp:2: delete expression", result.stderr)

    def test_object_storage_may_construct_in_place_and_comments_do_not_count(self):
        self.tree.write("src/core/support/object_storage.hpp",
                        "class ObjectStorage {\n    int v_;\n};\ninline void c(void *p) { ::new (p) int(1); }\n")
        self.tree.write("src/core/other.hpp",
                        "/* no new or delete here */\nclass Other {\n    Other(const Other &) = delete;\n    int v_; // new ideas\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_object_pool_may_construct_in_place(self):
        self.tree.write("src/core/support/object_pool.hpp",
                        "class ObjectPool {\n    int v_;\n};\ninline void c(void *p) { ::new (p) int(1); }\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_repository_passes(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("design_rules: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()


class GeneratedDirectories(unittest.TestCase):
    """What a generator wrote is not this project's design, and is not judged
    as such - but nothing hand-written may hide there either, which is what
    check_generated.py asserts and what this asserts it relies on (feature
    063)."""

    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)

    def test_a_class_a_generator_wrote_is_not_judged_by_these_rules(self):
        """A registry decides the shape of generated code: static methods and
        stateless classes in it are the API's doing, not a design mistake."""
        self.tree.write("src/proto/generated/vulkan/vk_serialize.hpp",
                        "namespace av::vkproto {\n"
                        "class Commands {\n"
                        "public:\n"
                        "    static void encode();\n"
                        "};\n"
                        "}\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_same_class_outside_a_generated_directory_is_judged(self):
        """The exclusion is the directory, not the name: move it and the rules
        apply again."""
        self.tree.write("src/proto/hand_written/vk_serialize.hpp",
                        "namespace av::vkproto {\n"
                        "class Commands {\n"
                        "public:\n"
                        "    static void encode();\n"
                        "};\n"
                        "}\n")
        result = run(CHECK, self.tree.root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Commands", result.stdout + result.stderr)
