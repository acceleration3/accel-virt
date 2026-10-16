# `cl_khr_d3d11_sharing`

An OpenCL context made from an application's Direct3D 11 device, and
`cl_mem` objects over that device's buffers and textures, which OpenCL and
Direct3D take turns over through acquire and release.

DaVinci Resolve is why this exists. Once it can pair the avgpu adapter with an
OpenCL device (docs/vendor-libraries.md), it makes its OpenCL context with
`CL_CONTEXT_D3D11_DEVICE_KHR`, which AMD's and NVIDIA's drivers always accept.
Refused, it waits forever at its splash screen.

## What the application has, and what the host can reach

The application's resources are ordinary Direct3D 11 resources of avumd's,
which the executor makes as ordinary DXVK resources. The host cannot export
their memory: DXVK exports only textures made to be shared, in six formats, and
never buffers or 3D textures (`ExportableImageInfo`). Nothing on either side
maps an `ID3D11Resource*` to the host object behind it. avumd sees driver
handles, not COM objects, and exports no in-process interface.

So OpenCL never touches the application's memory. Each `cl_mem` is an
ordinary host OpenCL buffer or image of the resource's size and format, and
acquire and release copy between the two.

## Stage 1: through the application's own device

Built. Unit tested through the loader's table (`tests/unit/icd/opencl/test_cl_d3d11_sharing.cpp`
and the tests of each class below), and end to end on the host's own OpenCL
with the tests' bridge standing in for Direct3D
(`fullstack_opencl_icd.a_d3d11_resource_s_bytes_reach_the_kernel_and_come_back_changed`).

Everything Direct3D is done by avcl, through COM, on the device the context
was made from: the device the application handed over, and its immediate
context, entered under `ID3D11Multithread` so a thread of the application's
cannot be inside it at the same time.

- **Acquire.** For each object, the resource's subresource is copied into a
  staging resource (`CopySubresourceRegion`) and mapped for reading. The `Map`
  waits for every Direct3D 11 command before the copy, which is the
  extension's rule that Direct3D's work on the object is done before OpenCL
  sees it. The mapped rows are written straight into the `cl_mem`, then the
  staging resource is unmapped and released. An object made
  `CL_MEM_WRITE_ONLY` is not copied: OpenCL will not read it.
- **Release.** The queue is drained, so every command that used the objects
  has run. Then each `cl_mem` is read straight into a staging resource mapped
  for writing, which is copied back into the resource's subresource. Direct3D
  commands the application makes afterwards are ordered after that copy on
  the same immediate context. An object made `CL_MEM_READ_ONLY` is not copied
  back.
- **Waits and events.** Both wait for the wait list first. The event they
  answer is a marker, complete once the call returns, since everything they do
  is done by then (`ClIcdRoot::marker_event`).
- **Failure.** An acquire or release that fails puts every object back where
  it was: Direct3D's after a failed acquire, OpenCL's after a failed release.
  A `cl_mem` released after the device is lost still lets go of its resource.

This is correct for every object the extension names: buffers, every
subresource of a 2D texture or texture array, every mip level of a 3D
texture, and every format the extension's table maps. The cost is two trips
through guest memory per acquire and release.

### Pieces

| Piece | Where | Does |
|---|---|---|
| `D3d11Bridge` | `src/icd/opencl/` (interface) | What only COM can do: describe a resource, check which device made it, hold and release a reference, and read or write a subresource through a staging resource mapped for a `D3d11Rows` visitor. |
| `WindowsD3d11Bridge` | `src/um/opencl/` | The COM implementation. |
| `D3d11Rows` | `src/icd/opencl/` (interface) | Handed a mapped subresource's rows: writes them into a `cl_mem`, or reads a `cl_mem` into them. |
| `ClD3d11Objects` | `src/icd/opencl/` | The `cl_mem` objects of one context made from Direct3D 11: the resource, the subresource, its shape, and whether it is acquired. |
| `DxgiFormat` | `src/proto/d3d/` | The one DXGI table: the extension's `cl_image_format` for each `DXGI_FORMAT`, and whether the host makes an image over a shared texture of it (`cl_image_on_host`). |
| `ClIcdRoot` | | The six entry points; context, memory and image queries; refusals. |

### Errors

| Case | Answer |
|---|---|
| A context's Direct3D 11 device not on the avgpu adapter | `CL_INVALID_D3D11_DEVICE_KHR` |
| A resource not a buffer, 2D or 3D texture, as the call names it, or made on another device | `CL_INVALID_D3D11_RESOURCE_KHR` |
| A subresource past the resource's | `CL_INVALID_VALUE` |
| A typeless format, or one the table does not map | `CL_INVALID_IMAGE_FORMAT_DESCRIPTOR` |
| Acquiring an object already acquired | `CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR` |
| Releasing an object not acquired | `CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR` |
| An object of another context, or not from Direct3D 11 | `CL_INVALID_MEM_OBJECT` |
| A queue whose context was not made from a Direct3D 11 device | `CL_INVALID_CONTEXT` |

A context may share with OpenGL and with Direct3D 11 at once. The
extensions call that `CL_INVALID_OPERATION`, but AMD's and NVIDIA's drivers
make the context, and DaVinci Resolve asks for exactly that: one context with
its GL context and its Direct3D 11 device. The two never meet here. GL objects
are the host's share group's, and Direct3D 11 objects are copied through the
device.

The resource gains a reference when its `cl_mem` is made, and loses it when
the application's last reference to the `cl_mem` goes, as the extension
requires. The context holds a reference to its device for its life.

## Stage 2: on the host GPU, where the host can export

Built. For a 2D texture, the copies never leave the GPU:

1. **The twin.** avcl makes a twin on the application's device
   (`D3d11Bridge::twin`): the subresource's size and format, one mip, one
   slice, `D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE`.
   avumd makes it over memory the host exports (`ExportableImages`), laid out
   as rows, which avcl asks for around the CreateTexture2D
   (`SharedRowsScope`; every other shared texture is tiled), and publishes it
   under the key the miniport minted, as the size of its tight rows in whole
   pages (`D3d11Device::publish_shared`).
2. **The image.** Publishing, the executor describes what the memory is as an
   image: width, height, DXGI format and the row pitch of the linear image
   (`SharedImageShape`, `D3dBackend::describe_shared_image`). The kind on the
   wire stays memory and no ABI changed; the shape is the publisher's, never
   the guest's. avcl opens the twin's NT handle in the context
   (`ClExternalMemory::open_own`, which closes the handle once adopted), and
   the OpenCL backend makes a 2D image over the buffer it imports, at that
   pitch and in the order the bytes are (`OpenClBackend::image_over`,
   `DxgiFormat::cl_image_on_host`: BGRA as `CL_BGRA`). The application's `cl_mem` is that
   image.
3. **Acquire and release.** Acquire copies the subresource into the twin
   (`CopySubresourceRegion`) and waits for it with a map of one texel of
   the twin through a staging texture. The device blocks in that map, so
   nothing polls. Release drains the queue, then copies the twin back on the
   immediate context. An object made read-only or write-only skips the
   direction its side never reads, as in stage 1.

A twin the device will not make, or whose handle the host cannot open as an
image, leaves the object to stage 1. That covers a format with no exportable
or OpenCL equivalent, sRGB, and a host without `cl_khr_image2d_from_buffer`.
avcl asks for no twin of a format the host would open as bytes - the same
`DxgiFormat::cl_image_on_host` the backend reads - so an sRGB texture, which
OpenCL would decode where Direct3D did not encode, crosses through memory.
The application sees the same object either way. The twin is let go after
the image over it, when the `cl_mem`'s last reference goes.

Tested on the host by
`d3d_cl_interop_device.a_shared_texture_opened_by_opencl_is_the_image_direct3d_wrote`,
and in avcl by the `cl_d3d11_sharing` twin tests.

Stage 1 stays the path for everything stage 2 does not cover.
