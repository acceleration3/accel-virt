# Quickstart (046)

- Linux: `ctest --test-dir build -LE tools -j6` (WSI units over IcdRig, the
  present blit, the probe over ScriptedVulkan);
  `test_fullstack_vulkan_icd` presents on the host GPU and writes
  `build/images/vulkan-present.bmp`.
- Guest: `run_guest_suite.py --vulkan ...` - the probe's `surface`,
  `present`, `pacing` and `resized` steps pass and
  `<work>/images/vulkan-present.bmp` is the frame read back from the window;
  the suite boot's `screen0.ppm` shows the desktop.
