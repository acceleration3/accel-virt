/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-device: the base virtio-gpu device handles the 2D protocol;
 * realize, reset, finalize and every control request reach the device core
 * through rvk_device.h first. Beyond stock virtio-gpu, only what the core says
 * it implements is offered: context init, when it has a capset, the doorbell
 * pages the PCI wrapper maps, and a fence queue per doorbell whose guest
 * notifier the core's workers set to interrupt the guest.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-rvk.c.
 */
#include "qemu/osdep.h"
#include <sys/prctl.h>
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qemu/iov.h"
#include "qemu/mmap-alloc.h"
#include "hw/core/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-rvk.h"
#include "ui/console.h"

static void (*virtio_gpu_rvk_parent_unrealize)(DeviceState *qdev);

/* Fence queues carry no requests: a kick does nothing. */
static void virtio_gpu_rvk_fence_kick(VirtIODevice *vdev, VirtQueue *vq)
{
}

/* The device core's trace socket asks the main loop for its state dump
 * through this, with no device state behind it (docs/trace.md). */
static void virtio_gpu_rvk_schedule(RvkHostRoutine routine, void *context)
{
    aio_bh_schedule_oneshot(qemu_get_aio_context(), routine, context);
}


/* The base device's console ops without ui_info. rvkgpu reads the display's
 * size once, at StartDevice, and offers no mode larger than it; a host window
 * reporting its own size (gtk's is 642x480 before the guest has drawn) would
 * pin the guest's desktop there. So the guest keeps the size xres and yres
 * name, and the window scales it. */
static int virtio_gpu_rvk_console_flags(void *opaque)
{
    VirtIOGPUBase *g = opaque;
    return virtio_gpu_dmabuf_enabled(g->conf) ? GRAPHIC_FLAGS_DMABUF : GRAPHIC_FLAGS_NONE;
}

static void virtio_gpu_rvk_console_invalidate(void *opaque)
{
}

static bool virtio_gpu_rvk_console_update(void *opaque)
{
    return true;
}

static void virtio_gpu_rvk_console_text(void *opaque, uint32_t *chardata)
{
}

static const GraphicHwOps virtio_gpu_rvk_console_ops = {
    .get_flags = virtio_gpu_rvk_console_flags,
    .invalidate = virtio_gpu_rvk_console_invalidate,
    .gfx_update = virtio_gpu_rvk_console_update,
    .text_update = virtio_gpu_rvk_console_text,
};

static void virtio_gpu_rvk_realize(DeviceState *qdev, Error **errp)
{
    /* A profiling run (RVK_PERFETTO) lets any process of the user read this
     * one's stacks: tools/perfetto_trace.py's sampler, under Yama's scope 1. */
    const char *profiled = getenv("RVK_PERFETTO");
    if (profiled != NULL && profiled[0] == '1') {
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    }
    ERRP_GUARD();
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(qdev);
    VirtIOGPU *g = VIRTIO_GPU(qdev);
    const RvkExecutorOps *executor = NULL;
    const RvkRecordingOptions recording = {
        .path = r->record,
        .limit_bytes = r->record_limit,
        .kinds = r->record_kinds,
        .wrap = r->record_wrap,
    };
    char error[256];

    for (uint32_t head = 0; head < VIRTIO_GPU_MAX_SCANOUTS; head++) {
        r->showing[head] = -1;
    }
    /* Before the core: its capset reports host-visible memory from the region. */
    if (virtio_gpu_hostmem_enabled(b->conf)) {
        void *map = qemu_ram_mmap(-1, b->conf.hostmem, qemu_real_host_page_size(), 0, 0);

        if (map == MAP_FAILED) {
            error_setg_errno(errp, errno, "virtio-gpu-rvk: the hostmem region could not be reserved");
            return;
        }
        r->hostmem_mmap = map;
        memory_region_init_ram_ptr(&r->hostmem_background, NULL, "virtio-gpu-rvk-hostmem-background",
                                   b->conf.hostmem, map);
        memory_region_add_subregion(&b->hostmem, 0, &r->hostmem_background);
    }
    if (r->executor && r->executor[0]) {
        RvkExecutorOpsFunction ops = NULL;

        r->executor_library = g_module_open(r->executor, G_MODULE_BIND_LOCAL);
        if (!r->executor_library) {
            error_setg(errp, "virtio-gpu-rvk: executor '%s' could not be loaded: %s", r->executor,
                       g_module_error());
            return;
        }
        if (!g_module_symbol(r->executor_library, RVK_EXECUTOR_OPS_SYMBOL, (gpointer *)&ops) || !ops) {
            error_setg(errp, "virtio-gpu-rvk: executor '%s' exports no %s", r->executor, RVK_EXECUTOR_OPS_SYMBOL);
            return;
        }
        executor = ops();
        /* The symbol answering NULL is a library that loaded and has nothing
         * to say. Left unchecked it is worse than a failure: the device comes
         * up as a plain virtio-gpu with no capset, and the guest's driver finds
         * no rvk protocol with nothing anywhere saying why (feature 042). */
        if (!executor) {
            error_setg(errp, "virtio-gpu-rvk: executor '%s' answered no table from %s",
                       r->executor, RVK_EXECUTOR_OPS_SYMBOL);
            return;
        }
    }
    rvk_device_trace_start(virtio_gpu_rvk_schedule);
    virtio_gpu_rvk_host_init(r);
    r->core = rvk_device_create(&r->host, executor, r->host_gpu, virtio_gpu_blob_enabled(g->parent_obj.conf),
                                &recording, error, sizeof(error));
    if (!r->core) {
        error_setg(errp, "%s", error);
        return;
    }
    b->virtio_config.num_capsets = cpu_to_le32(rvk_device_capsets(r->core));
    if (rvk_device_capsets(r->core)) {
        b->conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
    }
    virtio_gpu_device_realize(qdev, errp);
    if (*errp) {
        return;
    }
    for (uint32_t i = 0; i < b->conf.max_outputs; i++) {
        qemu_graphic_console_set_hwops(b->scanout[i].con, &virtio_gpu_rvk_console_ops, b);
    }
    /* A GPU that renders but drives no monitor, as a second host GPU is. */
    if (!r->display) {
        b->enabled_output_bitmask = 0;
    }
    for (uint32_t i = 0; i < RVK_DEVICE_DOORBELLS; i++) {
        r->fence_queues[i] = virtio_add_queue(VIRTIO_DEVICE(qdev), 2, virtio_gpu_rvk_fence_kick);
    }
}

/* The workers are joined and the fence interrupts released before the queues go. */
static void virtio_gpu_rvk_unrealize(DeviceState *qdev)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);

    rvk_device_reset(r->core);
    for (uint32_t i = 0; i < RVK_DEVICE_DOORBELLS; i++) {
        virtio_del_queue(VIRTIO_DEVICE(qdev), RVK_DEVICE_FENCE_QUEUE_FIRST + i);
    }
    virtio_gpu_rvk_parent_unrealize(qdev);
}

int virtio_gpu_rvk_pci_identity(VirtIOGPURvk *r, RvkPciIdentity *identity)
{
    return r->core && rvk_device_pci_identity(r->core, identity);
}

static int virtio_gpu_rvk_set_status(VirtIODevice *vdev, uint8_t status)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(vdev);

    rvk_device_status(r->core, status);
    return 0;
}

/* A 2D scanout resource filled from the host image a guest primary shows,
 * named by its key (docs/d3d-desktop.md §3.3): the guest's pages are not read,
 * and RESOURCE_FLUSH then shows it as it would a TRANSFER_TO_HOST_2D. */
#define VIRTIO_GPU_RVK_CMD_SCANOUT_IMAGE 0x02F0

struct virtio_gpu_rvk_scanout_image {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
    uint64_t key;
};

#define VIRTIO_GPU_RVK_NO_HEAD UINT32_MAX

/* The head scanning `resource_id` out, or VIRTIO_GPU_RVK_NO_HEAD: the cursor's
 * image, for one, is on no head, and a transfer to it must leave every head
 * as it is. */
static uint32_t virtio_gpu_rvk_head_of(VirtIOGPU *g, uint32_t resource_id)
{
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(g);

    for (uint32_t head = 0; head < b->conf.max_outputs && head < VIRTIO_GPU_MAX_SCANOUTS; head++) {
        if (b->scanout[head].resource_id == resource_id) {
            return head;
        }
    }
    return VIRTIO_GPU_RVK_NO_HEAD;
}

static void virtio_gpu_rvk_scanout_image(VirtIOGPU *g, VirtIOGPURvk *r, struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_rvk_scanout_image request;
    struct virtio_gpu_simple_resource *resource;
    enum virtio_gpu_ctrl_type answer = VIRTIO_GPU_RESP_OK_NODATA;
    RvkExportedImage exported;

    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &request, sizeof(request)) != sizeof(request)) {
        answer = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
    } else if (rvk_device_export_image(r->core, le64_to_cpu(request.key), &exported) &&
               virtio_gpu_rvk_show_image(r, virtio_gpu_rvk_head_of(g, le32_to_cpu(request.resource_id)), &exported)) {
        /* A GL display composes the host's image as it is: nothing is read back. */
        answer = VIRTIO_GPU_RESP_OK_NODATA;
    } else {
        resource = virtio_gpu_find_resource(g, le32_to_cpu(request.resource_id));
        if (resource == NULL || resource->image == NULL) {
            answer = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID;
        } else if (!rvk_device_read_image(r->core, le64_to_cpu(request.key), pixman_image_get_data(resource->image),
                                          (uint32_t)pixman_image_get_stride(resource->image), resource->width,
                                          resource->height)) {
            answer = VIRTIO_GPU_RESP_ERR_UNSPEC;
        }
    }
    virtio_gpu_ctrl_response_nodata(g, cmd, answer);
}

/* The core answers the requests it owns; the base class answers the rest. */
static void virtio_gpu_rvk_process_cmd(VirtIOGPU *g, struct virtio_gpu_ctrl_command *cmd)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(g);
    struct virtio_gpu_ctrl_hdr header;

    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &header, sizeof(header)) == sizeof(header)) {
        virtio_gpu_ctrl_hdr_bswap(&header);
        /* virtio_gpu_ctrl_response copies fence, flags and context from here. */
        cmd->cmd_hdr = header;
        if (header.type == VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D) {
            struct virtio_gpu_transfer_to_host_2d transfer;
            /* Guest pixels for the whole of a resource: its head shows the 2D
             * surface again. A part of one is not the guest going back to
             * drawing on the CPU, and showing a stale surface with that part
             * on it would be worse than the image already shown. */
            if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &transfer, sizeof(transfer)) == sizeof(transfer)) {
                const uint32_t head = virtio_gpu_rvk_head_of(g, le32_to_cpu(transfer.resource_id));
                if (head != VIRTIO_GPU_RVK_NO_HEAD && le32_to_cpu(transfer.r.x) == 0 &&
                    le32_to_cpu(transfer.r.y) == 0 &&
                    le32_to_cpu(transfer.r.width) >= VIRTIO_GPU_BASE(g)->scanout[head].width &&
                    le32_to_cpu(transfer.r.height) >= VIRTIO_GPU_BASE(g)->scanout[head].height) {
                    virtio_gpu_rvk_show_surface(r, head);
                }
            }
        }
        if (header.type == VIRTIO_GPU_CMD_SET_SCANOUT) {
            struct virtio_gpu_set_scanout scanout;
            /* A resource with neither a 2D image nor a mapped blob - a blob
             * the host keeps in hostmem - has nothing virtio-gpu can scan out,
             * and its SET_SCANOUT reads a null image (a SIGSEGV in
             * pixman_image_get_data): refused here instead. */
            if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &scanout, sizeof(scanout)) == sizeof(scanout)) {
                const uint32_t id = le32_to_cpu(scanout.resource_id);
                struct virtio_gpu_simple_resource *res = id != 0 ? virtio_gpu_find_resource(g, id) : NULL;
                if (res != NULL && res->image == NULL && res->blob == NULL) {
                    virtio_gpu_ctrl_response_nodata(g, cmd, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
                    return;
                }
            }
        }
        if (header.type == VIRTIO_GPU_RVK_CMD_SCANOUT_IMAGE) {
            virtio_gpu_rvk_scanout_image(g, r, cmd);
            return;
        }
        if (rvk_device_control(r->core, cmd, header.type)) {
            return;
        }
    }
    virtio_gpu_simple_process_cmd(g, cmd);
}

static void virtio_gpu_rvk_reset(VirtIODevice *vdev)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(vdev);

    rvk_device_reset(r->core);
    virtio_gpu_reset(vdev);
}

/* After unrealize, or after a realize that failed. The hostmem reservation
 * stays mapped, as virtio-gpu-gl's does: the region may outlive the device.
 * The executor library is closed after the core, which calls into it. */
static void virtio_gpu_rvk_finalize(Object *obj)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(obj);

    (void)rvk_device_destroy(r->core);
    virtio_gpu_rvk_host_finish(r);
    if (r->executor_library) {
        g_module_close(r->executor_library);
    }
}

/* Stores that are not a connected doorbell's ioeventfd land here and do nothing. */
static uint64_t virtio_gpu_rvk_doorbell_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void virtio_gpu_rvk_doorbell_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
}

static const MemoryRegionOps virtio_gpu_rvk_doorbell_ops = {
    .read = virtio_gpu_rvk_doorbell_read,
    .write = virtio_gpu_rvk_doorbell_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void virtio_gpu_rvk_instance_init(Object *obj)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(obj);

    memory_region_init_io(&r->doorbells, obj, &virtio_gpu_rvk_doorbell_ops, r, "virtio-gpu-rvk-doorbells",
                          (uint64_t)RVK_DEVICE_DOORBELLS * RVK_DEVICE_DOORBELL_PAGE);
}

static const Property virtio_gpu_rvk_properties[] = {
    DEFINE_PROP_STRING("host-gpu", VirtIOGPURvk, host_gpu),
    DEFINE_PROP_STRING("executor", VirtIOGPURvk, executor),
    DEFINE_PROP_STRING("record", VirtIOGPURvk, record),
    DEFINE_PROP_UINT64("record-limit", VirtIOGPURvk, record_limit, 0),
    DEFINE_PROP_STRING("record-kinds", VirtIOGPURvk, record_kinds),
    DEFINE_PROP_BOOL("record-wrap", VirtIOGPURvk, record_wrap, false),
    DEFINE_PROP_BOOL("display", VirtIOGPURvk, display, true),
};

static void virtio_gpu_rvk_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vdc->realize = virtio_gpu_rvk_realize;
    virtio_gpu_rvk_parent_unrealize = vdc->unrealize;
    vdc->unrealize = virtio_gpu_rvk_unrealize;
    vdc->set_status = virtio_gpu_rvk_set_status;
    vgc->process_cmd = virtio_gpu_rvk_process_cmd;
    vdc->reset = virtio_gpu_rvk_reset;
    device_class_set_props(dc, virtio_gpu_rvk_properties);
}

static const TypeInfo virtio_gpu_rvk_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_RVK,
        .parent = TYPE_VIRTIO_GPU,
        .instance_size = sizeof(VirtIOGPURvk),
        .instance_init = virtio_gpu_rvk_instance_init,
        .instance_finalize = virtio_gpu_rvk_finalize,
        .class_init = virtio_gpu_rvk_class_init,
    },
};

DEFINE_TYPES(virtio_gpu_rvk_info)

module_obj(TYPE_VIRTIO_GPU_RVK);
module_kconfig(VIRTIO_GPU);
module_dep("hw-display-virtio-gpu");
