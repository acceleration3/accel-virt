# Data model (046)

- **Surface** - a window handle and the instance that made it; handle is the
  record's address. Destroyed by `vkDestroySurfaceKHR`, or with its instance.
- **Swapchain** - device, surface, extent, format, present mode, 2..8 images,
  the read-back buffer (width x height x 4 bytes, host-visible), a command
  pool, command buffer and fence for the copy, the index handed out next.
  States: `made` -> (`acquired` <-> `presented`) -> `out_of_date` ->
  `destroyed`. An old swapchain passed to a new one is retired: its images
  stay valid until destroyed, and no image of it is handed out again.
- **Presented frame** - an image index, copied into the read-back buffer,
  converted to BGRA bottom-up, drawn at the window's origin.
