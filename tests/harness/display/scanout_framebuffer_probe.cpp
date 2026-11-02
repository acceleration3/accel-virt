#include "harness/display/scanout_framebuffer_probe.hpp"

namespace av {

ScanoutFramebufferProbe::ScanoutFramebufferProbe(ScanoutFramebuffer &target) : framebuffer(target) {}

void ScanoutFramebufferProbe::set_adopted(bool adopted) { framebuffer.adopted_ = adopted; }

void ScanoutFramebufferProbe::set_format(uint32_t format) { framebuffer.format_ = ScanoutFormat(format); }

void ScanoutFramebufferProbe::set_width(uint32_t width) { framebuffer.width_ = width; }

void ScanoutFramebufferProbe::set_height(uint32_t height) { framebuffer.height_ = height; }

void ScanoutFramebufferProbe::set_stride(uint32_t stride) { framebuffer.stride_ = stride; }

void ScanoutFramebufferProbe::set_offset(uint32_t offset) { framebuffer.offset_ = offset; }

void ScanoutFramebufferProbe::set_blob_size(uint64_t size) { framebuffer.blob_size_ = size; }

}
