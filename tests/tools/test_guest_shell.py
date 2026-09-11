"""tools/guest/guest_shell.py against a scripted guest agent: programs and
PowerShell with their output and exit code, files both ways, and no agent."""
import base64
import pathlib
import shutil
import tempfile
import unittest

from test_guest_agent import ScriptedPeer, answer, synced
from tool_run import ROOT, run

SHELL = ROOT / "tools" / "guest" / "guest_shell.py"


def output(text):
    return base64.b64encode(text).decode()


class GuestShell(unittest.TestCase):
    def setUp(self):
        self.work = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.work)

    def peer(self, replies):
        (self.work / "agent.sock").unlink(missing_ok=True)
        return ScriptedPeer(str(self.work / "agent.sock"), [synced, answer({}), *replies])

    def shell(self, *arguments):
        return run(SHELL, "--work", self.work, "--agent-timeout", "2", *arguments)

    def test_ps_runs_powershell_prints_its_output_and_exits_with_its_code(self):
        peer = self.peer([answer({"pid": 4}), answer({"exited": True, "exitcode": 3, "out-data": output(b"adapter\n"),
                                                      "err-data": output(b"warning\n")})])
        result = self.shell("ps", "Get-PnpDevice -Class Display")
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertEqual(result.stdout, "adapter\n")
        self.assertEqual(result.stderr, "warning\n")
        started = peer.requests[2]["arguments"]
        self.assertTrue(started["path"].endswith("powershell.exe"))
        self.assertEqual(started["arg"][-2:], ["-Command", "Get-PnpDevice -Class Display"])

    def test_exec_passes_its_arguments_and_push_and_pull_move_files(self):
        peer = self.peer([answer({"pid": 4}), answer({"exited": True, "exitcode": 0, "out-data": ""})])
        self.assertEqual(self.shell("exec", "C:\\rvk\\rvktest.exe", "-x").returncode, 0)
        self.assertEqual(peer.requests[2]["arguments"]["arg"], ["-x"])
        local = self.work / "rvkgpu.sys"
        local.write_bytes(b"driver")
        peer = self.peer([answer(7), answer({"count": 6}), answer({})])
        self.assertEqual(self.shell("push", local, "C:\\rvk\\package\\rvkgpu.sys").returncode, 0)
        self.assertEqual(base64.b64decode(peer.requests[3]["arguments"]["buf-b64"]), b"driver")
        self.peer([answer(7), answer({"count": 4, "buf-b64": output(b"dump"), "eof": True}), answer({})])
        pulled = self.shell("pull", "C:\\Windows\\MEMORY.DMP", self.work / "MEMORY.DMP")
        self.assertEqual(pulled.returncode, 0, pulled.stderr)
        self.assertEqual(pulled.stdout, "4 bytes\n")
        self.assertEqual((self.work / "MEMORY.DMP").read_bytes(), b"dump")

    def test_without_an_agent_it_says_so(self):
        result = self.shell("ps", "hostname")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest_shell: the guest agent never answered", result.stderr)


if __name__ == "__main__":
    unittest.main()
