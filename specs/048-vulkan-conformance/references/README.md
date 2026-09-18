# Reference frames (feature 048, US2)

What an application run is compared against: the device's **scanout**, as QEMU's
`screendump` writes it, committed as a binary PPM named for the run
(`vulkan.ppm`, `d3d.ppm`).

**This directory is empty of frames on purpose.** Nothing the guest presents
reaches the scanout yet: feature 046 measured it (see
`specs/046-vulkan-present/` and `docs/kb/07-usermode.md`) - dxgkrnl issues two
Blt presents a boot, the primary stays zero, and no user-mode driver is
registered, so the desktop is black and a frame committed from it would be a
reference to a black screen. The runs the references come from are features 049
and 055-057.

Until then `run_guest_suite.py --frames` captures the scanout, says that no
reference is committed for that name, and keeps the capture in the run's work
directory - which is where the first reference will come from, committed in the
change that earns it.
