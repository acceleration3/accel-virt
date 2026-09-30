# Reference frames

What an application run is compared against: the device's **scanout**, as QEMU's
`screendump` writes it, committed as a binary PPM named for the run
(`vulkan.ppm`, `d3d.ppm`).

No frame is committed yet. `run_guest_suite.py --frames` captures the scanout,
says that no reference is committed for that name, and keeps the capture in the
run's work directory - which is where the first reference comes from, committed
in the change that earns it.
