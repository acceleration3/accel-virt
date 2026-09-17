import itertools
import sys
import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

sys.path.insert(0, str(ROOT / "tools"))
from scope_marks import scope_id  # noqa: E402

TOOL = ROOT / "tools" / "scope_marks.py"
SCOPE_HEADER = ("#ifndef AV_FUNCTION_SCOPE_HPP\n#define AV_FUNCTION_SCOPE_HPP\n"
                "#define AV_SCOPE(name) static constexpr const char *av_scope_name_ = name\n#endif\n")
INCLUDE = '#include "core/trace/function_scope.hpp"'


def colliding_names():
    """Two distinct names with one FNV-1a-32 id: a birthday search over short names."""
    seen = {}
    for number in itertools.count():
        name = f"n{number}"
        other = seen.setdefault(scope_id(name), name)
        if other != name:
            return other, name


class ScopeMarks(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("src/core/trace/function_scope.hpp", SCOPE_HEADER)

    def check(self):
        return run(TOOL, "--check", self.tree.root)

    def apply(self):
        return run(TOOL, "--apply", self.tree.root)

    def read(self, relative):
        return (self.tree.root / relative).read_text()

    def test_fnv1a_ids_match_the_published_vectors(self):
        self.assertEqual(scope_id(""), 0x811C9DC5)
        self.assertEqual(scope_id("a"), 0xE40C292C)
        self.assertEqual(scope_id("foobar"), 0xBF9CF968)

    def test_reports_an_unmarked_method_with_its_qualified_name(self):
        self.tree.write("src/core/thing.cpp",
                        "namespace av {\nclass Thing {\npublic:\n    int size() const;\nprivate:\n    int n_;\n};\n"
                        "int Thing::size() const\n{\n    return n_;\n}\n}  // namespace av\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/thing.cpp:8: Thing::size has no scope mark", result.stderr)

    def test_reports_a_mark_naming_another_function_and_apply_fixes_it(self):
        self.tree.write("src/core/thing.cpp",
                        f"{INCLUDE}\n\nstruct Thing {{\n    void run();\n    int n_;\n}};\n"
                        "void Thing::run()\n{\n    AV_SCOPE(\"Thing::walk\");\n    n_ = 1;\n}\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn('src/core/thing.cpp:7: Thing::run is marked "Thing::walk"', result.stderr)
        self.assertEqual(self.apply().returncode, 0)
        self.assertIn('AV_SCOPE("Thing::run");\n    n_ = 1;', self.read("src/core/thing.cpp"))
        self.assertNotIn("Thing::walk", self.read("src/core/thing.cpp"))

    def test_apply_marks_constructors_destructors_and_multi_line_bodies(self):
        self.tree.write("src/device/pool.hpp",
                        "#ifndef POOL_HPP\n#define POOL_HPP\n\nclass Pool {\npublic:\n    Pool(int a, int b);\n"
                        "    ~Pool();\n    int sum() const;\nprivate:\n    int a_;\n    int b_;\n};\n\n#endif\n")
        self.tree.write("src/device/pool.cpp",
                        '#include "device/pool.hpp"\n\n#include "device/zeta.hpp"\n\nPool::Pool(int a, int b)\n'
                        "    : a_(a), b_(b)\n{\n    a_ += 0;\n}\n\nPool::~Pool() {\n    a_ = 0;\n}\n\n"
                        "int Pool::sum() const { int total = a_;\n    return total + b_;\n}\n")
        self.tree.write("src/device/zeta.hpp", "#ifndef ZETA\n#define ZETA\n#endif\n")
        self.assertEqual(self.apply().returncode, 0, self.apply().stderr)
        text = self.read("src/device/pool.cpp")
        self.assertIn('    : a_(a), b_(b)\n{\n    AV_SCOPE("Pool::Pool");\n    a_ += 0;\n}', text)
        self.assertIn('Pool::~Pool() {\n    AV_SCOPE("Pool::~Pool");\n    a_ = 0;\n}', text)
        self.assertIn('int Pool::sum() const {\n    AV_SCOPE("Pool::sum");\n    int total = a_;\n', text)
        # The own header stays first; the include joins the project block in order.
        self.assertTrue(text.startswith('#include "device/pool.hpp"\n\n' + INCLUDE + '\n#include "device/zeta.hpp"\n'))
        self.assertEqual(self.check().returncode, 0, self.check().stderr)

    def test_apply_rewrites_a_one_line_body_in_the_file_s_style(self):
        self.tree.write("src/guest/getter.cpp",
                        "struct Getter {\n    int value() const;\n    void none();\n    int v_;\n};\n\n"
                        "int Getter::value() const { return v_; }\n"
                        "void Getter::none() {}\n")
        self.assertEqual(self.apply().returncode, 0)
        text = self.read("src/guest/getter.cpp")
        self.assertIn('int Getter::value() const\n{\n    AV_SCOPE("Getter::value");\n    return v_;\n}\n', text)
        self.assertIn('void Getter::none()\n{\n    AV_SCOPE("Getter::none");\n}\n', text)
        self.assertTrue(text.startswith(INCLUDE + "\n\n"))

    def test_a_template_in_a_header_is_marked_and_the_header_includes_the_scope(self):
        self.tree.write("src/core/holder.hpp",
                        '#ifndef HOLDER_HPP\n#define HOLDER_HPP\n\n#include "core/alpha.hpp"\n\n'
                        "template <typename T>\nclass Holder {\npublic:\n    Holder() {}\n"
                        "    T &get() { return value_; }\n    constexpr int size() const { return 1; }\n"
                        "private:\n    T value_;\n};\n\n#endif\n")
        self.tree.write("src/core/alpha.hpp", "#ifndef ALPHA\n#define ALPHA\n#endif\n")
        self.assertEqual(self.apply().returncode, 0)
        text = self.read("src/core/holder.hpp")
        self.assertIn('#include "core/alpha.hpp"\n' + INCLUDE + "\n", text)
        self.assertIn('    Holder()\n    {\n        AV_SCOPE("Holder::Holder");\n    }\n', text)
        self.assertIn('    T &get()\n    {\n        AV_SCOPE("Holder::get");\n        return value_;\n    }\n', text)
        self.assertIn("    constexpr int size() const { return 1; }\n", text)

    def test_exempt_definitions_are_left_alone_and_must_stay_unmarked(self):
        original = ("namespace {\n\nconstexpr int helper(int x)\n{\n    return x;\n}\n\n}  // namespace\n\n"
                    'extern "C" int AvEntry(int x)\n{ return helper(x); }\n\n'
                    'extern "C" {\nint AvOther(int x)\n{ return x; }\n}\n\n'
                    "constexpr int twice(int x)\n{\n    return 2 * x;\n}\n\n"
                    "struct Plain {\n    Plain() = default;\n    Plain(const Plain &) = delete;\n    int n_;\n};\n")
        self.tree.write("src/core/ddi/entry.cpp", original)
        self.tree.write("src/core/trace/table.cpp", "struct Table {\n    void f();\n    int n_;\n};\n"
                                                    "void Table::f()\n{\n}\n")
        self.tree.write("src/proto/generated/vulkan/wrap.cpp", "struct G {\n    void f();\n    int n_;\n};\n"
                                                               "void G::f()\n{\n}\n")
        self.tree.write("src/km/ddi_table_layout.cpp", "struct L {\n    void f();\n    int n_;\n};\n"
                                                       "void L::f()\n{\n}\n")
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.apply().returncode, 0)
        self.assertEqual(self.read("src/core/ddi/entry.cpp"), original)
        self.tree.write("src/core/ddi/entry.cpp", INCLUDE + "\n" + original.replace(
            "constexpr int twice(int x)\n{\n", 'constexpr int twice(int x)\n{\n    AV_SCOPE("twice");\n'))
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("twice is exempt and must not be marked", result.stderr)

    def test_wire_primitives_and_trace_hosts_are_never_marked(self):
        """The per-member wire reads and writes run dozens of times per command
        and cannot block, and a trace host is what every scope calls: neither
        needs a mark, apply never writes one, and one written by hand fails."""
        plain = "struct WireReader {\n    int read_u8();\n    int n_;\n};\nint WireReader::read_u8()\n{\n    return n_;\n}\n"
        for relative in ("src/proto/wire/wire_reader.cpp", "src/proto/wire/wire_writer.hpp",
                         "src/proto/wire/wire_arena.cpp", "src/device/shim/posix_trace_host.cpp",
                         "src/um/trace/interrupt_clock.cpp"):
            with self.subTest(relative=relative):
                self.tree.write(relative, plain)
                self.assertEqual(self.check().returncode, 0)
                self.assertEqual(self.apply().returncode, 0)
                self.assertEqual(self.read(relative), plain)
                self.tree.write(relative, INCLUDE + "\n" + plain.replace(
                    "{\n    return", '{\n    AV_SCOPE("WireReader::read_u8");\n    return'))
                result = self.check()
                self.assertEqual(result.returncode, 1)
                self.assertIn(f"{relative}:8: WireReader::read_u8 is in a file that must not be marked",
                              result.stderr)
                self.tree.write(relative, plain)
        # Only those files: a reader beside them is judged as any other.
        self.tree.write("src/proto/wire/wire_view.cpp", plain)
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("WireReader::read_u8 has no scope mark", result.stderr)

    def test_file_local_helpers_are_marked_with_their_bare_names(self):
        self.tree.write("src/core/local.cpp",
                        "namespace av {\n\nnamespace {\n\nint length_of(const char *text)\n{\n    return text != 0;\n}\n\n"
                        "}  // namespace\n\nstatic int twice(int x)\n{\n    return 2 * x;\n}\n\n"
                        "struct Local {\n    int f();\n    int n_;\n};\n\nint Local::f()\n{\n"
                        "    return twice(length_of(nullptr));\n}\n\n}  // namespace av\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/local.cpp:5: length_of has no scope mark", result.stderr)
        self.assertIn("src/core/local.cpp:12: twice has no scope mark", result.stderr)
        self.assertEqual(self.apply().returncode, 0)
        text = self.read("src/core/local.cpp")
        self.assertIn('int length_of(const char *text)\n{\n    AV_SCOPE("length_of");\n', text)
        self.assertIn('static int twice(int x)\n{\n    AV_SCOPE("twice");\n', text)
        self.assertIn('int Local::f()\n{\n    AV_SCOPE("Local::f");\n', text)
        self.assertEqual(self.check().returncode, 0, self.check().stderr)

    def test_apply_twice_changes_nothing_the_second_time(self):
        self.tree.write("src/icd/twice.cpp",
                        '#include "icd/twice.hpp"\n\nstruct Twice {\n    int a();\n    int b();\n    int n_;\n};\n'
                        "int Twice::a() { return n_; }\n\nint Twice::b()\n{\n    return n_;\n}\n")
        self.tree.write("src/icd/twice.hpp", "#ifndef TWICE\n#define TWICE\n#endif\n")
        self.assertEqual(self.apply().returncode, 0)
        once = self.read("src/icd/twice.cpp")
        self.assertEqual(self.apply().returncode, 0)
        self.assertEqual(self.read("src/icd/twice.cpp"), once)
        self.assertEqual(once.count(INCLUDE), 1)
        self.assertTrue(once.startswith('#include "icd/twice.hpp"\n\n' + INCLUDE + "\n\nstruct"))

    def test_a_mark_without_the_include_is_reported(self):
        self.tree.write("src/core/bare.cpp", 'struct Bare {\n    void f();\n    int n_;\n};\n'
                                             'void Bare::f()\n{\n    AV_SCOPE("Bare::f");\n}\n')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"src/core/bare.cpp: marks without {INCLUDE}", result.stderr)

    def test_a_file_that_cannot_be_parsed_is_reported_not_passed(self):
        self.tree.write("src/core/broken.cpp", '#include "core/missing.hpp"\n\nvoid f()\n{\n}\n')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/broken.cpp: cannot be parsed: 'core/missing.hpp' file not found", result.stderr)
        self.assertEqual(self.apply().returncode, 1)
        self.assertNotIn("AV_SCOPE", self.read("src/core/broken.cpp"))

    def test_two_names_with_one_id_fail_the_check(self):
        first, second = colliding_names()
        self.tree.write("src/core/pair.cpp",
                        f'{INCLUDE}\n\nvoid {first}()\n{{\n    AV_SCOPE("{first}");\n}}\n\n'
                        f'void {second}()\n{{\n    AV_SCOPE("{second}");\n}}\n')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn(f'scope id {scope_id(first):08x} is both "{min(first, second)}" and "{max(first, second)}"',
                      result.stderr)

    def test_names_lists_every_mark_generated_code_included(self):
        self.tree.write("src/core/one.cpp", f'{INCLUDE}\n\nvoid one()\n{{\n    AV_SCOPE("one");\n}}\n')
        self.tree.write("src/proto/generated/vulkan/wrap.cpp", 'void w()\n{\n    AV_SCOPE("Wrap_vkCreateInstance");\n}\n')
        result = run(TOOL, "--names", self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = sorted([f"{scope_id('one'):08x} one",
                           f"{scope_id('Wrap_vkCreateInstance'):08x} Wrap_vkCreateInstance"])
        self.assertEqual(result.stdout.splitlines(), expected)

    def test_the_repository_is_judged_and_its_unmarked_core_reported(self):
        result = run(TOOL, "--check", ROOT)
        findings = [line for line in result.stderr.splitlines() if line.startswith("src/core/")]
        self.assertNotIn("cannot be parsed", result.stderr)
        if result.returncode == 0:
            return
        self.assertGreaterEqual(len(findings), 500)


if __name__ == "__main__":
    unittest.main()
