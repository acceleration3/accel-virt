#!/usr/bin/env python3
"""The in-guest suite (docs/testing.md, L5): one command from a build to a
verdict from a real Windows guest.

A copy-on-write overlay of a prepared image boots on the patched QEMU with
virtio-gpu-rvk-pci, the guest agent's channel, QMP and the driver's debug
port. The package is pushed, test-signed and installed with Driver Verifier's
standard checks, each reboot waited through; the adapter must have started on
rvkgpu; rvktest.exe runs. The report, the device check, the verifier's
settings and the debug-port log land in the work directory; the guest is
powered off and the overlay deleted, so the image is never written.

  run_guest_suite.py --image windows.qcow2 [--work build-guest] [options]

A prepared image: Windows 11 24H2 or later, x64, UEFI without Secure Boot,
the QEMU guest agent installed and running. Exits 0 only when every step
passed and the report counts no failure.
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guest_agent import AgentError, Qmp, wait_for_agent  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUEST_DIRECTORY = "C:\\rvk"
PACKAGE_DIRECTORY = GUEST_DIRECTORY + "\\package"
POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
REBOOT_REQUIRED = 3010
SUMMARY = re.compile(rb"^rvktest: (\d+) passed, (\d+) failed$", re.M)


class StepFailed(Exception):
    """A named step of the run did not do what it had to."""

    def __init__(self, step, reason):
        super().__init__(f"{step}: {reason}")
        self.step = step
        self.reason = reason


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", default=os.environ.get("AV_GUEST_IMAGE"),
                        help="the prepared Windows qcow2 (default: $AV_GUEST_IMAGE)")
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest")
    parser.add_argument("--qemu", default=str(ROOT / "build-qemu" / "qemu" / "qemu-system-x86_64"))
    parser.add_argument("--qemu-img", default="qemu-img")
    parser.add_argument("--executor", default=str(ROOT / "build-qemu" / "librvk_echo_executor.so"))
    parser.add_argument("--driver", default=str(ROOT / "build" / "driver" / "rvkgpu.sys"))
    parser.add_argument("--inf", default=str(ROOT / "src" / "km" / "rvkgpu.inf"))
    parser.add_argument("--test", default=str(ROOT / "build" / "guest" / "rvktest.exe"))
    parser.add_argument("--provision", default=str(ROOT / "tools" / "guest" / "provision.ps1"))
    parser.add_argument("--firmware", default="/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    parser.add_argument("--firmware-vars", default="/usr/share/edk2/x64/OVMF_VARS.4m.fd")
    parser.add_argument("--memory", type=int, default=4096, help="MiB")
    parser.add_argument("--cpus", type=int, default=4)
    parser.add_argument("--accel", default="kvm")
    parser.add_argument("--boot-timeout", type=float, default=900.0)
    parser.add_argument("--step-timeout", type=float, default=600.0)
    parser.add_argument("--agent-timeout", type=float, default=30.0)
    parsed = parser.parse_args()
    if not parsed.image:
        parser.error("no image: pass --image or set AV_GUEST_IMAGE")
    return parsed


def qemu_command(args, work):
    return [
        args.qemu, "-machine", f"q35,accel={args.accel},memory-backend=ram",
        "-cpu", "host" if args.accel == "kvm" else "max",
        "-smp", str(args.cpus), "-m", f"{args.memory}M",
        "-object", f"memory-backend-memfd,id=ram,size={args.memory}M,share=on",
        "-drive", f"if=pflash,format=raw,readonly=on,file={args.firmware}",
        "-drive", f"if=pflash,format=raw,file={work / 'vars.fd'}",
        "-drive", f"if=none,id=disk,format=qcow2,file={work / 'overlay.qcow2'}",
        "-device", "ide-hd,drive=disk,bus=ide.0",
        "-device", f"virtio-gpu-rvk-pci,host-gpu=auto,executor={args.executor}",
        "-device", "virtio-serial-pci",
        "-chardev", f"socket,id=agent,path={work / 'agent.sock'},server=on,wait=off",
        "-device", "virtserialport,chardev=agent,name=org.qemu.guest_agent.0",
        "-chardev", f"file,id=debugcon,path={work / 'rvkgpu.log'}",
        "-device", "isa-debugcon,iobase=0x402,chardev=debugcon",
        "-qmp", f"unix:{work / 'qmp.sock'},server=on,wait=off",
        "-vga", "none", "-display", "none", "-nic", "none",
    ]


def fingerprint(path):
    """Size and modification time: hashing a guest image would take minutes."""
    status = os.stat(path)
    return status.st_size, status.st_mtime_ns


class GuestRun:
    """One run: its work directory, its QEMU, and the sockets into the guest."""

    def __init__(self, args):
        self.args = args
        self.work = args.work.resolve()
        self.qemu = None
        self.qmp = None
        self.agent = None
        self.log = []

    def say(self, line):
        self.log.append(line)
        print(f"guest: {line}", flush=True)

    def step(self, name, action):
        try:
            result = action()
        except AgentError as error:
            raise StepFailed(name, str(error)) from error
        self.say(f"{name} ok")
        return result

    def prepare(self):
        for required in (self.args.image, self.args.driver, self.args.inf, self.args.test, self.args.provision):
            if not os.path.isfile(required):
                raise StepFailed("prepare", f"{required} does not exist")
        for stale in ("overlay.qcow2", "vars.fd", "agent.sock", "qmp.sock", "rvkgpu.log"):
            (self.work / stale).unlink(missing_ok=True)
        shutil.copyfile(self.args.firmware_vars, self.work / "vars.fd")
        created = subprocess.run([self.args.qemu_img, "create", "-f", "qcow2", "-F", "qcow2",
                                  "-b", os.path.abspath(self.args.image), str(self.work / "overlay.qcow2")],
                                 capture_output=True, text=True)
        if created.returncode != 0:
            raise StepFailed("overlay", created.stderr.strip() or f"qemu-img exited {created.returncode}")
        self.say("overlay ok")

    def boot(self):
        output = open(self.work / "qemu.log", "wb")
        self.qemu = subprocess.Popen(qemu_command(self.args, self.work), stdout=output, stderr=subprocess.STDOUT)
        output.close()
        deadline = time.monotonic() + self.args.agent_timeout
        while not (self.work / "qmp.sock").exists():
            if self.qemu.poll() is not None or time.monotonic() > deadline:
                raise StepFailed("boot", f"QEMU exited {self.qemu.poll()} before QMP listened; see qemu.log")
            time.sleep(0.1)
        self.qmp = self.step("qmp", lambda: Qmp(str(self.work / "qmp.sock"), self.args.agent_timeout))
        self.connect_agent("boot")

    def connect_agent(self, name):
        if self.agent is not None:
            self.agent.close()
        deadline = time.monotonic() + self.args.boot_timeout
        self.agent = self.step(name, lambda: wait_for_agent(str(self.work / "agent.sock"), deadline,
                                                            self.args.agent_timeout))

    def reboot(self, after):
        self.qmp.forget_events()
        self.agent.notify("guest-shutdown", {"mode": "reboot"})
        self.step(f"{after}.reset", lambda: self.qmp.wait_event("RESET", self.args.boot_timeout))
        self.connect_agent(f"{after}.reboot")

    def push(self):
        files = [(self.args.driver, PACKAGE_DIRECTORY + "\\rvkgpu.sys"),
                 (self.args.inf, PACKAGE_DIRECTORY + "\\rvkgpu.inf"),
                 (self.args.test, GUEST_DIRECTORY + "\\rvktest.exe"),
                 (self.args.provision, GUEST_DIRECTORY + "\\provision.ps1")]

        def send():
            make = f"New-Item -ItemType Directory -Force -Path {PACKAGE_DIRECTORY}"
            code, _, error = self.agent.run(POWERSHELL, ["-NoProfile", "-Command", make], self.args.step_timeout)
            if code != 0:
                raise AgentError(f"cannot create {PACKAGE_DIRECTORY}: {error.decode(errors='replace')}")
            for local, remote in files:
                self.agent.push(pathlib.Path(local).read_bytes(), remote)

        self.step("push", send)

    def stage(self, name, saved=None):
        """Runs one provisioning stage: whether it asked for a reboot."""
        code, out, error = self.step(name, lambda: self.agent.run(
            POWERSHELL, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", GUEST_DIRECTORY + "\\provision.ps1",
                         "-Stage", name], self.args.step_timeout))
        if saved is not None:
            (self.work / saved).write_bytes(out + error)
        if code not in (0, REBOOT_REQUIRED):
            raise StepFailed(name, f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        return code == REBOOT_REQUIRED

    def suite(self):
        code, out, error = self.step("rvktest", lambda: self.agent.run(GUEST_DIRECTORY + "\\rvktest.exe", [],
                                                                         self.args.step_timeout))
        (self.work / "report.txt").write_bytes(out)
        summary = SUMMARY.search(out)
        if summary is None:
            raise StepFailed("report", f"no summary line (exit {code}): {error.decode(errors='replace')[-200:]}")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("report", f"{failed} step(s) failed, exit {code}; see report.txt")
        self.say(f"report ok: {int(summary.group(1))} passed")

    def power_off(self):
        if self.agent is not None and self.qemu is not None and self.qemu.poll() is None:
            try:
                self.agent.notify("guest-shutdown", {"mode": "powerdown"})
                self.qemu.wait(timeout=self.args.step_timeout)
                self.say("powerdown ok")
            except (AgentError, subprocess.TimeoutExpired):
                self.say("powerdown timed out: QEMU killed")

    def finish(self):
        for channel in (self.agent, self.qmp):
            if channel is not None:
                channel.close()
        if self.qemu is not None and self.qemu.poll() is None:
            self.qemu.kill()
            self.qemu.wait()
        for leftover in ("overlay.qcow2", "vars.fd", "agent.sock", "qmp.sock"):
            (self.work / leftover).unlink(missing_ok=True)

    def run(self):
        self.work.mkdir(parents=True, exist_ok=True)
        before = fingerprint(self.args.image) if os.path.isfile(self.args.image) else None
        try:
            self.prepare()
            self.boot()
            self.push()
            if self.stage("sign"):
                self.reboot("sign")
            if self.stage("install"):
                self.reboot("install")
            self.stage("check", saved="device.txt")
            self.suite()
            self.power_off()
            verdict = 0
        except StepFailed as failure:
            self.say(f"{failure.step} failed: {failure.reason}")
            verdict = 1
        finally:
            self.finish()
        if before is not None and fingerprint(self.args.image) != before:
            self.say("image failed: the base image changed")
            verdict = 1
        (self.work / "verdict.txt").write_text("\n".join(self.log) + "\n")
        return verdict


def main():
    sys.exit(GuestRun(arguments()).run())


if __name__ == "__main__":
    main()
