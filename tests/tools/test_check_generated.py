"""The committed generated tree is what the generator writes, or the gate says so.

Never hand-edit a file under `generated/`: fix the emitter and regenerate. That
rule is only worth stating if something enforces it, which is this.
"""
import pathlib
import shutil
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_generated.py"


class CheckGenerated(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.registries = self.scratch / "khronos"
        self.registries.mkdir()
        (self.registries / "toy.xml").write_text("<registry/>\n")
        self.out = self.scratch / "generated" / "toy"
        self.out.mkdir(parents=True)
        # A stand-in generator: writes one file whose text comes from the
        # registry, and implements --check the way av_codegen does.
        self.generator = self.scratch / "toy_generator.py"
        self.generator.write_text(textwrap.dedent('''
            import argparse, filecmp, pathlib, sys, tempfile
            parser = argparse.ArgumentParser()
            parser.add_argument("--xml", type=pathlib.Path, required=True)
            parser.add_argument("--out", type=pathlib.Path, required=True)
            parser.add_argument("--check", action="store_true")
            a = parser.parse_args()
            body = "// from " + a.xml.read_text()
            if not a.check:
                a.out.mkdir(parents=True, exist_ok=True)
                (a.out / "toy.h").write_text(body)
                sys.exit(0)
            with tempfile.TemporaryDirectory() as fresh:
                (pathlib.Path(fresh) / "toy.h").write_text(body)
                same = filecmp.dircmp(str(a.out), fresh)
                if same.diff_files or same.left_only or same.right_only:
                    names = same.diff_files + same.left_only + same.right_only
                    print("differs: " + " ".join(sorted(names)), file=sys.stderr)
                    sys.exit(1)
            sys.exit(0)
        ''').strip())
        self.plan = self.scratch / "plan.txt"
        self.plan.write_text(f"toy {self.generator} {self.registries / 'toy.xml'} {self.out}\n")

    def generate(self):
        return run("python3", self.generator, "--xml", self.registries / "toy.xml", "--out", self.out)

    def check(self, *extra):
        return run(CHECK, "--plan", self.plan, *extra)

    def test_a_tree_that_matches_passes_and_names_what_it_checked(self):
        self.assertEqual(self.generate().returncode, 0)
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("toy", result.stdout)

    def test_an_edited_file_fails_and_names_it(self):
        self.assertEqual(self.generate().returncode, 0)
        (self.out / "toy.h").write_text("// someone typed this\n")
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("toy.h", result.stdout + result.stderr)

    def test_a_file_the_generator_would_not_write_fails_too(self):
        """Nothing hand-written hides in a generated directory: the gates that
        exclude those directories would stop judging it."""
        self.assertEqual(self.generate().returncode, 0)
        (self.out / "extra.cpp").write_text("// smuggled in\n")
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("extra.cpp", result.stdout + result.stderr)

    def test_without_the_registries_it_skips_and_says_why(self):
        self.assertEqual(self.generate().returncode, 0)
        (self.registries / "toy.xml").unlink()
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("skipped", result.stdout)
        self.assertIn("fetch", result.stdout)

    def test_the_repository_s_own_plan_names_every_api_it_generates(self):
        """What ships: a plan whose generators and profiles all exist."""
        plan = (ROOT / "tools" / "generated_sources.txt").read_text()
        named = [line.split() for line in plan.splitlines()
                 if line.strip() and not line.startswith("#")]
        self.assertTrue(named, "the plan names nothing to generate")
        for entry in named:
            self.assertGreaterEqual(len(entry), 4, entry)
            api = entry[0]
            for path in entry[2:]:
                if path.startswith("--"):
                    continue
                # `@` stands for the repository root in the plan's extra
                # arguments, which is how a path reaches the generator without
                # the plan naming a machine.
                path = path.removeprefix("@")
                # Registry paths live under third_party and may not be fetched;
                # everything else is committed and must be there.
                if "third_party" in path:
                    continue
                self.assertTrue((ROOT / path).exists(), f"{api}: {path} is not in the repository")
