/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include <sys/prctl.h>
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qemu/iov.h"
#include "qemu/mmap-alloc.h"
#include "hw/core/qdev-properties.h"
#include "qapi/visitor.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-av.h"
#include "ui/console.h"

static void (*virtio_gpu_av_parent_unrealize)(DeviceState *qdev);

static void virtio_gpu_av_fence_kick(VirtIODevice *vdev, VirtQueue *vq)
{
}

static void virtio_gpu_av_schedule(AvHostRoutine routine, void *context)
{
    aio_bh_schedule_oneshot(qemu_get_aio_context(), routine, context);
}


static int virtio_gpu_av_console_flags(void *opaque)
{
    VirtIOGPUBase *g = opaque;
    return virtio_gpu_dmabuf_enabled(g->conf) ? GRAPHIC_FLAGS_DMABUF : GRAPHIC_FLAGS_NONE;
}

static void virtio_gpu_av_console_invalidate(void *opaque)
{
}

static bool virtio_gpu_av_console_update(void *opaque)
{
    return true;
}

static void virtio_gpu_av_console_text(void *opaque, uint32_t *chardata)
{
}

static const GraphicHwOps virtio_gpu_av_console_ops = {
    .get_flags = virtio_gpu_av_console_flags,
    .invalidate = virtio_gpu_av_console_invalidate,
    .gfx_update = virtio_gpu_av_console_update,
    .text_update = virtio_gpu_av_console_text,
};

static bool virtio_gpu_av_apply_modes(VirtIOGPUAv *r, const char *modes, Error **errp)
{
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(r);
    uint32_t width = 0;
    uint32_t height = 0;
    const int set = av_device_set_modes(r->core, modes, &width, &height);

    if (set < 0) {
        error_setg(errp, "virtio-gpu-av: modes '%s' is not a list of WxH sizes between commas "
                   "(the first preferred), such as 1920x1080,80x80", modes);
        return false;
    }
    for (uint32_t i = 0; set > 0 && i < b->conf.max_outputs; i++) {
        b->req_state[i].width = width;
        b->req_state[i].height = height;
    }
    return true;
}

static char *virtio_gpu_av_get_modes(Object *obj, Error **errp)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);

    return g_strdup(r->modes ? r->modes : "");
}

static void virtio_gpu_av_set_modes(Object *obj, const char *value, Error **errp)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(obj);

    if (r->core) {
        if (!virtio_gpu_av_apply_modes(r, value, errp)) {
            return;
        }
        b->virtio_config.events_read |= VIRTIO_GPU_EVENT_DISPLAY;
        virtio_notify_config(VIRTIO_DEVICE(obj));
    }
    g_free(r->modes);
    r->modes = g_strdup(value);
}

static void virtio_gpu_av_get_refresh(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);
    uint32_t value = r->refresh;

    visit_type_uint32(v, name, &value, errp);
}

static void virtio_gpu_av_set_refresh(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
    if (r->core) {
        if (value == r->refresh) {
            return;
        }
        if (av_device_set_refresh(r->core, value) != 1) {
            error_setg(errp, "virtio-gpu-av: refresh %u is not between 1000 and 1000000 millihertz", value);
            return;
        }
        b->virtio_config.events_read |= VIRTIO_GPU_EVENT_DISPLAY;
        virtio_notify_config(VIRTIO_DEVICE(obj));
    }
    r->refresh = value;
}

static void virtio_gpu_av_realize(DeviceState *qdev, Error **errp)
{
    const char *profiled = getenv("AV_PERFETTO");
    if (profiled != NULL && profiled[0] == '1') {
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    }
    ERRP_GUARD();
    VirtIOGPUAv *r = VIRTIO_GPU_AV(qdev);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(qdev);
    VirtIOGPU *g = VIRTIO_GPU(qdev);
    const AvExecutorOps *executor = NULL;
    const char *renderer = r->renderer && r->renderer[0] ? r->renderer : NULL;
    const AvRecordingOptions recording = {
        .path = r->record,
        .limit_bytes = r->record_limit,
        .kinds = r->record_kinds,
        .wrap = r->record_wrap,
    };
    char error[256];

    for (uint32_t head = 0; head < VIRTIO_GPU_MAX_SCANOUTS; head++) {
        r->showing[head] = -1;
    }
    if (virtio_gpu_hostmem_enabled(b->conf)) {
        void *map = qemu_ram_mmap(-1, b->conf.hostmem, qemu_real_host_page_size(), 0, 0);

        if (map == MAP_FAILED) {
            error_setg_errno(errp, errno, "virtio-gpu-av: the hostmem region could not be reserved");
            return;
        }
        r->hostmem_mmap = map;
        memory_region_init_ram_ptr(&r->hostmem_background, NULL, "virtio-gpu-av-hostmem-background",
                                   b->conf.hostmem, map);
        memory_region_add_subregion(&b->hostmem, 0, &r->hostmem_background);
    }
    if (r->executor && r->executor[0] && !renderer) {
        AvExecutorOpsFunction ops = NULL;

        r->executor_library = g_module_open(r->executor, G_MODULE_BIND_LOCAL);
        if (!r->executor_library) {
            error_setg(errp, "virtio-gpu-av: executor '%s' could not be loaded: %s", r->executor,
                       g_module_error());
            return;
        }
        if (!g_module_symbol(r->executor_library, AV_EXECUTOR_OPS_SYMBOL, (gpointer *)&ops) || !ops) {
            error_setg(errp, "virtio-gpu-av: executor '%s' exports no %s", r->executor, AV_EXECUTOR_OPS_SYMBOL);
            return;
        }
        executor = ops();
        if (!executor) {
            error_setg(errp, "virtio-gpu-av: executor '%s' answered no table from %s",
                       r->executor, AV_EXECUTOR_OPS_SYMBOL);
            return;
        }
    }
    av_device_trace_start(virtio_gpu_av_schedule);
    virtio_gpu_av_host_init(r);
    r->core = av_device_create(&r->host, executor, r->host_gpu, virtio_gpu_blob_enabled(g->parent_obj.conf),
                                &recording, renderer, r->executor, r->vram_reserve, error, sizeof(error));
    if (!r->core) {
        error_setg(errp, "%s", error);
        return;
    }
    if (r->export_socket && r->export_socket[0] && !virtio_gpu_av_export_listen(r, errp)) {
        return;
    }
    b->virtio_config.num_capsets = cpu_to_le32(av_device_capsets(r->core));
    if (av_device_capsets(r->core)) {
        b->conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
    }
    virtio_gpu_device_realize(qdev, errp);
    if (*errp) {
        return;
    }
    for (uint32_t i = 0; i < b->conf.max_outputs; i++) {
        qemu_graphic_console_set_hwops(b->scanout[i].con, &virtio_gpu_av_console_ops, b);
    }
    if (!virtio_gpu_av_apply_modes(r, r->modes, errp)) {
        return;
    }
    if (av_device_set_refresh(r->core, r->refresh) != 1) {
        error_setg(errp, "virtio-gpu-av: refresh %u is not between 1000 and 1000000 millihertz", r->refresh);
        return;
    }
    if (!r->display) {
        b->enabled_output_bitmask = 0;
    }
    for (uint32_t i = 0; i < AV_DEVICE_INTERRUPT_LINES; i++) {
        r->fence_queues[i] = virtio_add_queue(VIRTIO_DEVICE(qdev), 2, virtio_gpu_av_fence_kick);
    }
}

static void virtio_gpu_av_unrealize(DeviceState *qdev)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(qdev);

    av_device_reset(r->core);
    for (uint32_t i = 0; i < AV_DEVICE_INTERRUPT_LINES; i++) {
        virtio_del_queue(VIRTIO_DEVICE(qdev), AV_DEVICE_FENCE_QUEUE_FIRST + i);
    }
    virtio_gpu_av_parent_unrealize(qdev);
}

int virtio_gpu_av_pci_identity(VirtIOGPUAv *r, AvPciIdentity *identity)
{
    return r->core && av_device_pci_identity(r->core, identity);
}

static int virtio_gpu_av_set_status(VirtIODevice *vdev, uint8_t status)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(vdev);

    av_device_status(r->core, status);
    return 0;
}

#define VIRTIO_GPU_AV_CMD_SCANOUT_IMAGE 0x02F0

struct virtio_gpu_av_scanout_image {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t frame;
    uint64_t key;
};

#define VIRTIO_GPU_AV_NO_HEAD UINT32_MAX

static uint32_t virtio_gpu_av_head_of(VirtIOGPU *g, uint32_t resource_id)
{
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(g);

    for (uint32_t head = 0; head < b->conf.max_outputs && head < VIRTIO_GPU_MAX_SCANOUTS; head++) {
        if (b->scanout[head].resource_id == resource_id) {
            return head;
        }
    }
    return VIRTIO_GPU_AV_NO_HEAD;
}

static void virtio_gpu_av_scanout_image(VirtIOGPU *g, VirtIOGPUAv *r, struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_av_scanout_image request;
    struct virtio_gpu_simple_resource *resource;
    enum virtio_gpu_ctrl_type answer = VIRTIO_GPU_RESP_OK_NODATA;
    AvExportedImage exported;
    uint32_t head;

    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &request, sizeof(request)) != sizeof(request)) {
        answer = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
    } else {
        resource = virtio_gpu_find_resource(g, le32_to_cpu(request.resource_id));
        if (resource == NULL || resource->image == NULL) {
            answer = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID;
        } else {
            head = virtio_gpu_av_head_of(g, le32_to_cpu(request.resource_id));
            if (r->host.display_takes_dmabuf(r->host.opaque, head) &&
                !av_device_post_frame(r->core, head, le32_to_cpu(request.frame), le64_to_cpu(request.key)) &&
                !(av_device_export_head(r->core, head, le64_to_cpu(request.key), &exported) &&
                  virtio_gpu_av_show_image(r, head, &exported))) {
                answer = VIRTIO_GPU_RESP_ERR_UNSPEC;
            }
        }
    }
    virtio_gpu_ctrl_response_nodata(g, cmd, answer);
}

static void virtio_gpu_av_process_cmd(VirtIOGPU *g, struct virtio_gpu_ctrl_command *cmd)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(g);
    struct virtio_gpu_ctrl_hdr header;

    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &header, sizeof(header)) == sizeof(header)) {
        virtio_gpu_ctrl_hdr_bswap(&header);
        cmd->cmd_hdr = header;
        if (header.type == VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D) {
            struct virtio_gpu_transfer_to_host_2d transfer;
            if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &transfer, sizeof(transfer)) == sizeof(transfer)) {
                const uint32_t head = virtio_gpu_av_head_of(g, le32_to_cpu(transfer.resource_id));
                if (head != VIRTIO_GPU_AV_NO_HEAD) {
                    virtio_gpu_av_show_surface(r, head);
                }
            }
        }
        if (header.type == VIRTIO_GPU_CMD_SET_SCANOUT) {
            struct virtio_gpu_set_scanout scanout;
            if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &scanout, sizeof(scanout)) == sizeof(scanout)) {
                const uint32_t id = le32_to_cpu(scanout.resource_id);
                struct virtio_gpu_simple_resource *res = id != 0 ? virtio_gpu_find_resource(g, id) : NULL;
                if (res != NULL && res->image == NULL && res->blob == NULL) {
                    virtio_gpu_ctrl_response_nodata(g, cmd, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
                    return;
                }
            }
        }
        if (header.type == VIRTIO_GPU_AV_CMD_SCANOUT_IMAGE) {
            virtio_gpu_av_scanout_image(g, r, cmd);
            return;
        }
        if (header.type == VIRTIO_GPU_CMD_RESOURCE_FLUSH) {
            struct virtio_gpu_resource_flush flush;
            if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &flush, sizeof(flush)) == sizeof(flush)) {
                const uint32_t head = virtio_gpu_av_head_of(g, le32_to_cpu(flush.resource_id));
                if (head != VIRTIO_GPU_AV_NO_HEAD && r->showing[head] >= 0) {
                    virtio_gpu_ctrl_response_nodata(g, cmd, VIRTIO_GPU_RESP_OK_NODATA);
                    return;
                }
            }
        }
        if (av_device_control(r->core, cmd, header.type)) {
            return;
        }
    }
    virtio_gpu_simple_process_cmd(g, cmd);
}

static void virtio_gpu_av_reset(VirtIODevice *vdev)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(vdev);

    av_device_reset(r->core);
    virtio_gpu_reset(vdev);
}

static void virtio_gpu_av_finalize(Object *obj)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);

    virtio_gpu_av_export_finish(r);
    (void)av_device_destroy(r->core);
    r->core = NULL;
    virtio_gpu_av_renderer_finish(r);
    virtio_gpu_av_host_finish(r);
    if (r->executor_library) {
        g_module_close(r->executor_library);
    }
    g_free(r->modes);
    r->modes = NULL;
}

static uint64_t virtio_gpu_av_doorbell_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void virtio_gpu_av_doorbell_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
}

static const MemoryRegionOps virtio_gpu_av_doorbell_ops = {
    .read = virtio_gpu_av_doorbell_read,
    .write = virtio_gpu_av_doorbell_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void virtio_gpu_av_instance_init(Object *obj)
{
    VirtIOGPUAv *r = VIRTIO_GPU_AV(obj);

    virtio_gpu_av_renderer_init(r);
    virtio_gpu_av_export_init(r);
    r->refresh = 60000;
    memory_region_init_io(&r->doorbells, obj, &virtio_gpu_av_doorbell_ops, r, "virtio-gpu-av-doorbells",
                          (uint64_t)AV_DEVICE_DOORBELLS * AV_DEVICE_DOORBELL_PAGE);
}

static const Property virtio_gpu_av_properties[] = {
    DEFINE_PROP_STRING("host-gpu", VirtIOGPUAv, host_gpu),
    DEFINE_PROP_STRING("executor", VirtIOGPUAv, executor),
    DEFINE_PROP_STRING("renderer", VirtIOGPUAv, renderer),
    DEFINE_PROP_STRING("record", VirtIOGPUAv, record),
    DEFINE_PROP_UINT64("record-limit", VirtIOGPUAv, record_limit, 0),
    DEFINE_PROP_UINT32("vram-reserve", VirtIOGPUAv, vram_reserve, 0),
    DEFINE_PROP_STRING("record-kinds", VirtIOGPUAv, record_kinds),
    DEFINE_PROP_BOOL("record-wrap", VirtIOGPUAv, record_wrap, false),
    DEFINE_PROP_BOOL("display", VirtIOGPUAv, display, true),
    DEFINE_PROP_STRING("export-socket", VirtIOGPUAv, export_socket),
};

static void virtio_gpu_av_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vdc->realize = virtio_gpu_av_realize;
    virtio_gpu_av_parent_unrealize = vdc->unrealize;
    vdc->unrealize = virtio_gpu_av_unrealize;
    vdc->set_status = virtio_gpu_av_set_status;
    vgc->process_cmd = virtio_gpu_av_process_cmd;
    vdc->reset = virtio_gpu_av_reset;
    device_class_set_props(dc, virtio_gpu_av_properties);
    object_class_property_add_str(klass, "modes", virtio_gpu_av_get_modes, virtio_gpu_av_set_modes);
    object_class_property_set_description(klass, "modes",
                                          "the display's modes, WxH between commas, the first preferred; "
                                          "empty: the host monitors' modes");
    object_class_property_add(klass, "refresh", "uint32", virtio_gpu_av_get_refresh, virtio_gpu_av_set_refresh,
                              NULL, NULL);
    object_class_property_set_description(klass, "refresh",
                                          "the display's refresh rate in millihertz, 1000 to 1000000, for every "
                                          "mode; the guest switches to a new one without a restart");
}

static const TypeInfo virtio_gpu_av_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_AV,
        .parent = TYPE_VIRTIO_GPU,
        .instance_size = sizeof(VirtIOGPUAv),
        .instance_init = virtio_gpu_av_instance_init,
        .instance_finalize = virtio_gpu_av_finalize,
        .class_init = virtio_gpu_av_class_init,
    },
};

DEFINE_TYPES(virtio_gpu_av_info)

module_obj(TYPE_VIRTIO_GPU_AV);
module_kconfig(VIRTIO_GPU);
module_dep("hw-display-virtio-gpu");
