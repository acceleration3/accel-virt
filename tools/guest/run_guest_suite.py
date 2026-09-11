#!/usr/bin/env python3
"""The in-guest suite (docs/testing.md, L5): one command from a build to a
verdict from a real Windows guest, with the evidence to debug it.

A copy-on-write overlay of a prepared image boots on the patched QEMU with
virtio-gpu-rvk-pci, the guest agent's channel, QMP and the driver's debug
port. The package is pushed, test-signed and installed with Driver Verifier's
standard checks, each reboot waited through; the adapter must have started on
rvkgpu; rvktest.exe runs. Whatever happened, the guest's event logs, device
install log, device and verifier state and live kernel dumps are pulled into
diag/. A bugcheck is named from the Hyper-V crash MSRs, and its kernel dump is
pulled after the guest reboots - or, when it bugchecks on every boot, from the
same overlay booted again without the device. The guest is powered off and the
overlay deleted, so the image is never written.

  run_guest_suite.py --image windows.qcow2 [--work build-guest] [options]
  run_guest_suite.py --image windows.qcow2 --prepare-image

--prepare-image boots the image itself, once, and readies it for runs: test
signing, kernel dumps, and nothing that interrupts a run. --hold keeps a failed
guest running for tools/guest/guest_shell.py, --gdb, --vnc and QMP.

A prepared image: Windows 11 24H2 or later, x64, UEFI without Secure Boot,
the QEMU guest agent installed and running. Exits 0 only when every step
passed and the report counts no failure.
"""
import argparse
import os
import pathlib
import re
import shutil
import signal
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
MANIFEST = re.compile(r"^(file|dump) (.+?)\r?$", re.M)
# The enlightenments Windows expects of KVM, and hv-crash: Windows writes its
# bugcheck code and parameters to the crash MSRs, which QEMU reports as
# GUEST_PANICKED while the guest goes on to write its dump.
HYPERV = ("hv-relaxed,hv-vapic,hv-spinlocks=0x1fff,hv-vpindex,hv-runtime,hv-synic,hv-stimer,hv-time,"
          "hv-frequencies,hv-tlbflush,hv-ipi,hv-crash")
# Events already queued arrive at once; a second of silence means none is coming.
EVENT_QUIET = 1.0
EVENT_POLL = 0.05
# QMP closing means QEMU is on its way out; its exit status follows at once.
QEMU_EXIT_GRACE = 5.0
BUGCHECKS = {
    0x0A: "IRQL_NOT_LESS_OR_EQUAL", 0x1E: "KMODE_EXCEPTION_NOT_HANDLED", 0x3B: "SYSTEM_SERVICE_EXCEPTION",
    0x50: "PAGE_FAULT_IN_NONPAGED_AREA", 0x7B: "INACCESSIBLE_BOOT_DEVICE",
    0x7E: "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED", 0x7F: "UNEXPECTED_KERNEL_MODE_TRAP",
    0xC4: "DRIVER_VERIFIER_DETECTED_VIOLATION", 0xC9: "DRIVER_VERIFIER_IOMANAGER_VIOLATION",
    0xD1: "DRIVER_IRQL_NOT_LESS_OR_EQUAL", 0xEF: "CRITICAL_PROCESS_DIED", 0x10E: "VIDEO_MEMORY_MANAGEMENT_INTERNAL",
    0x113: "VIDEO_DXGKRNL_FATAL_ERROR", 0x116: "VIDEO_TDR_FAILURE", 0x119: "VIDEO_SCHEDULER_INTERNAL_ERROR",
    0x133: "DPC_WATCHDOG_VIOLATION", 0x139: "KERNEL_SECURITY_CHECK_FAILURE",
}


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
    parser.add_argument("--prepare-image", action="store_true",
                        help="boot the image itself and ready it for runs, instead of running the suite")
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
    parser.add_argument("--on-panic", choices=("reboot", "pause"), default="reboot",
                        help="reboot: let the guest write its dump and reboot (default); "
                             "pause: stop the guest at the bugcheck for --gdb")
    parser.add_argument("--hold", action="store_true",
                        help="on failure, keep the guest running until Enter is pressed")
    parser.add_argument("--gdb", type=int, metavar="PORT", help="QEMU's gdbstub on 127.0.0.1:PORT")
    parser.add_argument("--vnc", type=int, metavar="DISPLAY", help="the guest's display on 127.0.0.1:5900+DISPLAY")
    parser.add_argument("--trace", action="append", default=[], metavar="PATTERN",
                        help="enable QEMU trace events matching PATTERN into qemu.log, e.g. 'virtio_gpu_*'")
    parsed = parser.parse_args()
    if not parsed.image:
        parser.error("no image: pass --image or set AV_GUEST_IMAGE")
    return parsed


def qemu_command(args, work, disk, rescue):
    """QEMU for a run; a rescue boot leaves the device out so the driver never loads."""
    suffix = "-rescue" if rescue else ""
    device = [] if rescue else ["-device", f"virtio-gpu-rvk-pci,host-gpu=auto,executor={args.executor}"]
    command = [
        args.qemu, "-machine", f"q35,accel={args.accel},memory-backend=ram",
        "-cpu", f"host,{HYPERV}" if args.accel == "kvm" else "max",
        "-smp", str(args.cpus), "-m", f"{args.memory}M",
        "-object", f"memory-backend-memfd,id=ram,size={args.memory}M,share=on",
        "-drive", f"if=pflash,format=raw,readonly=on,file={args.firmware}",
        "-drive", f"if=pflash,format=raw,file={work / 'vars.fd'}",
        "-drive", f"if=none,id=disk,format=qcow2,file={disk}",
        "-device", "ide-hd,drive=disk,bus=ide.0",
        *device,
        "-device", "virtio-serial-pci",
        "-chardev", f"socket,id=agent,path={work / 'agent.sock'},server=on,wait=off",
        "-device", "virtserialport,chardev=agent,name=org.qemu.guest_agent.0",
        "-chardev", f"file,id=debugcon,path={work / f'rvkgpu{suffix}.log'}",
        "-device", "isa-debugcon,iobase=0x402,chardev=debugcon",
        "-qmp", f"unix:{work / 'qmp.sock'},server=on,wait=off",
        "-action", f"panic={'pause' if args.on_panic == 'pause' else 'none'}",
        "-msg", "timestamp=on", "-d", "guest_errors",
        "-vga", "none", "-display", "none", "-nic", "none",
    ]
    if args.vnc is not None:
        command += ["-vnc", f"127.0.0.1:{args.vnc}"]
    if args.gdb is not None:
        command += ["-gdb", f"tcp:127.0.0.1:{args.gdb}"]
    for pattern in args.trace:
        command += ["-trace", f"enable={pattern}"]
    return command


def fingerprint(path):
    """Size and modification time: hashing a guest image would take minutes."""
    status = os.stat(path)
    return status.st_size, status.st_mtime_ns


def describe_panic(data):
    info = data.get("info") or {}
    if info.get("type") != "hyper-v":
        return f"the guest panicked (action {data.get('action', 'unknown')}) without bugcheck parameters"
    code = info.get("arg1", 0)
    parameters = ", ".join(f"{info.get(f'arg{number}', 0):#x}" for number in range(2, 6))
    return f"bugcheck {code:#x} {BUGCHECKS.get(code, 'unnamed')} ({parameters})"


class GuestRun:
    """One run: its work directory, its QEMU, and the sockets into the guest."""

    def __init__(self, args):
        self.args = args
        self.work = args.work.resolve()
        self.disk = None
        self.qemu = None
        self.qemu_log = None
        self.qmp = None
        self.qmp_lost = False
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
        required = [self.args.image, self.args.provision]
        if not self.args.prepare_image:
            required += [self.args.driver, self.args.inf, self.args.test]
        for path in required:
            if not os.path.isfile(path):
                raise StepFailed("prepare", f"{path} does not exist")
        for stale in ("overlay.qcow2", "vars.fd", "agent.sock", "qmp.sock", "rvkgpu.log", "rvkgpu-rescue.log",
                      "qemu-rescue.log", "panic.txt"):
            (self.work / stale).unlink(missing_ok=True)
        for stale in ("diag", "symbols"):
            shutil.rmtree(self.work / stale, ignore_errors=True)
        shutil.copyfile(self.args.firmware_vars, self.work / "vars.fd")
        if self.args.prepare_image:
            self.disk = os.path.abspath(self.args.image)
            return
        self.keep_symbols()
        self.disk = self.work / "overlay.qcow2"
        created = subprocess.run([self.args.qemu_img, "create", "-f", "qcow2", "-F", "qcow2",
                                  "-b", os.path.abspath(self.args.image), str(self.disk)],
                                 capture_output=True, text=True)
        if created.returncode != 0:
            raise StepFailed("overlay", created.stderr.strip() or f"qemu-img exited {created.returncode}")
        self.say("overlay ok")

    def keep_symbols(self):
        """The driver under test and its PDB beside the dumps it produces: a rebuild replaces both."""
        symbols = self.work / "symbols"
        symbols.mkdir()
        driver = pathlib.Path(self.args.driver)
        for path in (driver, driver.with_suffix(".pdb")):
            if path.is_file():
                shutil.copyfile(path, symbols / path.name)

    def boot(self, rescue=False):
        prefix = "rescue." if rescue else ""
        self.qemu_log = self.work / ("qemu-rescue.log" if rescue else "qemu.log")
        self.qmp_lost = False
        output = open(self.qemu_log, "wb")
        environment = dict(os.environ, QEMU_MODULE_DIR=os.path.dirname(os.path.abspath(self.args.qemu)))
        self.qemu = subprocess.Popen(qemu_command(self.args, self.work, self.disk, rescue), stdout=output,
                                     stderr=subprocess.STDOUT, env=environment)
        output.close()
        deadline = time.monotonic() + self.args.agent_timeout
        while not (self.work / "qmp.sock").exists():
            if self.qemu.poll() is not None or time.monotonic() > deadline:
                raise StepFailed(f"{prefix}boot", f"QEMU exited {self.qemu.poll()} before QMP listened; see qemu.log")
            time.sleep(0.1)
        self.qmp = self.step(f"{prefix}qmp", lambda: Qmp(str(self.work / "qmp.sock"), self.args.agent_timeout))
        self.connect_agent(f"{prefix}boot")

    def panicked_while_booting(self):
        if self.qmp is None:
            return None
        self.qmp.drain(EVENT_POLL)
        return "the guest panicked while booting" if self.qmp.seen("GUEST_PANICKED") else None

    def connect_agent(self, name, timeout=None):
        if self.agent is not None:
            self.agent.close()
            self.agent = None
        deadline = time.monotonic() + (self.args.boot_timeout if timeout is None else timeout)
        self.agent = self.step(name, lambda: wait_for_agent(str(self.work / "agent.sock"), deadline,
                                                            self.args.agent_timeout,
                                                            abandoned=self.panicked_while_booting))

    def reboot(self, after):
        self.qmp.forget_events("RESET")
        self.agent.notify("guest-shutdown", {"mode": "reboot"})
        self.step(f"{after}.reset", lambda: self.qmp.wait_event("RESET", self.args.boot_timeout))
        self.connect_agent(f"{after}.reboot")

    def push(self):
        files = [(self.args.provision, GUEST_DIRECTORY + "\\provision.ps1")]
        if not self.args.prepare_image:
            files += [(self.args.driver, PACKAGE_DIRECTORY + "\\rvkgpu.sys"),
                      (self.args.inf, PACKAGE_DIRECTORY + "\\rvkgpu.inf"),
                      (self.args.test, GUEST_DIRECTORY + "\\rvktest.exe")]

        def send():
            make = f"New-Item -ItemType Directory -Force -Path {PACKAGE_DIRECTORY}"
            code, _, error = self.agent.run(POWERSHELL, ["-NoProfile", "-Command", make], self.args.step_timeout)
            if code != 0:
                raise AgentError(f"cannot create {PACKAGE_DIRECTORY}: {error.decode(errors='replace')}")
            for local, remote in files:
                self.agent.push(pathlib.Path(local).read_bytes(), remote)

        self.step("push", send)

    def provision(self, name, extra=()):
        return self.step(name, lambda: self.agent.run(
            POWERSHELL, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", GUEST_DIRECTORY + "\\provision.ps1",
                         "-Stage", name, *extra], self.args.step_timeout))

    def stage(self, name, saved=None):
        """Runs one provisioning stage: whether it asked for a reboot."""
        code, out, error = self.provision(name)
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

    def install_and_test(self):
        if self.stage("sign"):
            self.reboot("sign")
        if self.stage("install"):
            self.reboot("install")
        self.stage("check", saved="device.txt")
        self.suite()

    def prepare_guest(self):
        if self.stage("prepare"):
            self.reboot("prepare")
            if self.stage("prepare"):
                raise StepFailed("prepare", "still asks for a reboot after rebooting")

    def harvest_panics(self):
        """Names every bugcheck QEMU reported; whether there was one."""
        if self.qmp is None:
            return False
        try:
            self.qmp.drain(EVENT_QUIET)
        except AgentError as error:
            self.say(f"qmp failed: {error}")
            self.qmp_lost = True
        panics = [describe_panic(message.get("data") or {}) for message in self.qmp.take_events("GUEST_PANICKED")]
        for panic in panics:
            self.say(f"panic: {panic}")
        if panics:
            (self.work / "panic.txt").write_text("\n".join(panics) + "\n")
        return bool(panics)

    def reach_guest(self, panicked):
        """An agent to collect through: after a bugcheck, the guest's reboot, or a rescue boot without the device."""
        try:
            if panicked:
                self.step("panic.reset", lambda: self.qmp.wait_event("RESET", self.args.boot_timeout))
            self.connect_agent("collect.agent", None if panicked else self.args.agent_timeout)
            return True
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")
        if not panicked:
            return False
        self.say("rescue: booting the overlay without virtio-gpu-rvk-pci for its dumps")
        self.stop_qemu()
        try:
            self.boot(rescue=True)
            return True
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")
            return False

    def collect(self, memory_dump):
        code, out, error = self.provision("collect", ["-MemoryDump"] if memory_dump else [])
        if code != 0:
            raise StepFailed("collect", f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        diagnostics = self.work / "diag"
        (diagnostics / "dumps").mkdir(parents=True, exist_ok=True)

        def pull():
            pulled = 0
            for kind, guest_path in MANIFEST.findall(out.decode(errors="replace")):
                name = guest_path.rsplit("\\", 1)[-1]
                self.agent.pull(guest_path, diagnostics / "dumps" / name if kind == "dump" else diagnostics / name)
                pulled += 1
            return pulled

        self.say(f"collect: {self.step('collect.pull', pull)} files in {diagnostics}")

    def collect_evidence(self, panicked):
        if not self.reach_guest(panicked):
            return
        try:
            self.collect(memory_dump=panicked)
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")

    def hold(self):
        for channel in (self.agent, self.qmp):
            if channel is not None:
                channel.close()
        self.agent = None
        self.qmp = None
        self.say("hold: the guest is left running")
        print(f"  shell  tools/guest/guest_shell.py --work {self.work} ps 'Get-PnpDevice -Class Display'")
        print(f"  qmp    socat - UNIX-CONNECT:{self.work / 'qmp.sock'}")
        if self.args.gdb is not None:
            print(f"  gdb    target remote 127.0.0.1:{self.args.gdb}")
        if self.args.vnc is not None:
            print(f"  vnc    127.0.0.1:{5900 + self.args.vnc}")
        print(f"  logs   {self.work / 'rvkgpu.log'}, {self.work / 'qemu.log'}")
        print("  press Enter to power the guest off", flush=True)
        try:
            sys.stdin.readline()
        except KeyboardInterrupt:
            pass
        if self.qemu.poll() is None:
            try:
                self.connect_agent("hold.agent", self.args.agent_timeout)
            except StepFailed as failed:
                self.say(f"{failed.step} failed: {failed.reason}")

    def power_off(self):
        if self.agent is not None and self.qemu is not None and self.qemu.poll() is None:
            try:
                self.agent.notify("guest-shutdown", {"mode": "powerdown"})
                self.qemu.wait(timeout=self.args.step_timeout)
                self.say("powerdown ok")
            except (AgentError, subprocess.TimeoutExpired):
                self.say("powerdown timed out: QEMU killed")

    def aftermath(self, failure):
        """Whatever the run did: the bugchecks, the guest's diagnostics, then off - or held for debugging."""
        if self.qemu is None or self.qemu_exited(0):
            return
        panicked = self.harvest_panics()
        if self.qmp_lost and self.qemu_exited(QEMU_EXIT_GRACE):
            return
        paused = panicked and self.args.on_panic == "pause"
        if paused:
            self.say("panic: the guest is paused at the bugcheck")
        elif not self.args.prepare_image:
            self.collect_evidence(panicked)
        if failure is not None and self.args.hold:
            self.hold()
        if not paused:
            self.power_off()

    def qemu_exited(self, timeout):
        """Whether QEMU is gone, reported with the last line it logged - an assertion, usually."""
        try:
            code = self.qemu.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return False
        how = f"on {signal.Signals(-code).name}" if code < 0 else str(code)
        lines = self.qemu_log.read_text(errors="replace").strip().splitlines()
        self.say(f"qemu exited {how}: {lines[-1] if lines else 'nothing in ' + self.qemu_log.name}")
        return True

    def stop_qemu(self):
        for channel in (self.agent, self.qmp):
            if channel is not None:
                channel.close()
        self.agent = None
        self.qmp = None
        if self.qemu is not None and self.qemu.poll() is None:
            self.qemu.kill()
            self.qemu.wait()
        for socket_name in ("agent.sock", "qmp.sock"):
            (self.work / socket_name).unlink(missing_ok=True)

    def finish(self):
        self.stop_qemu()
        for leftover in ("overlay.qcow2", "vars.fd"):
            (self.work / leftover).unlink(missing_ok=True)

    def run(self):
        self.work.mkdir(parents=True, exist_ok=True)
        watched = not self.args.prepare_image and os.path.isfile(self.args.image)
        before = fingerprint(self.args.image) if watched else None
        failure = None
        try:
            try:
                self.prepare()
                self.boot()
                self.push()
                if self.args.prepare_image:
                    self.prepare_guest()
                else:
                    self.install_and_test()
            except StepFailed as failed:
                failure = failed
                self.say(f"{failed.step} failed: {failed.reason}")
            self.aftermath(failure)
        finally:
            self.finish()
        verdict = 0 if failure is None else 1
        if before is not None and fingerprint(self.args.image) != before:
            self.say("image failed: the base image changed")
            verdict = 1
        (self.work / "verdict.txt").write_text("\n".join(self.log) + "\n")
        return verdict


def main():
    sys.exit(GuestRun(arguments()).run())


if __name__ == "__main__":
    main()
