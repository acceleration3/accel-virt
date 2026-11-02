#include "framework.h"

namespace samples {

namespace {

struct Options {
    const char *out = ".";
    const char *only = nullptr;
    uint32_t    width = 1280;
    uint32_t    height = 720;
    uint32_t    warmup = 60;
    double      warmup_seconds = 1.0;
    double      timed_seconds = 1.0;
    uint32_t    frames = 600;
    float       capture_time = 1.5f;
    bool        compile = false;
};

uint32_t parse_number(const char *text)
{
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10u + static_cast<uint32_t>(*text++ - '0');
    return value;
}

bool make_target(Gpu &gpu)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = gpu.width;
    desc.Height = gpu.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(gpu.device->CreateTexture2D(&desc, nullptr, &gpu.color))) return false;
    if (FAILED(gpu.device->CreateRenderTargetView(gpu.color, nullptr, &gpu.target))) return false;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D *depth = nullptr;
    if (FAILED(gpu.device->CreateTexture2D(&desc, nullptr, &depth))) return false;
    const bool made = SUCCEEDED(gpu.device->CreateDepthStencilView(depth, nullptr, &gpu.depth));
    depth->Release();
    return made;
}

void finish(Gpu &gpu)
{
    D3D11_QUERY_DESC desc = {D3D11_QUERY_EVENT, 0};
    ID3D11Query *done = nullptr;
    if (FAILED(gpu.device->CreateQuery(&desc, &done))) return;
    gpu.context->End(done);
    BOOL finished = FALSE;
    UINT flags = 0;
    while (gpu.context->GetData(done, &finished, sizeof finished, flags) == S_FALSE) {
        flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
    }
    done->Release();
}

bool capture(Gpu &gpu, const char *path)
{
    D3D11_TEXTURE2D_DESC desc = {};
    gpu.color->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *staging = nullptr;
    if (FAILED(gpu.device->CreateTexture2D(&desc, nullptr, &staging))) return false;
    gpu.context->CopyResource(staging, gpu.color);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    bool written = false;
    if (SUCCEEDED(gpu.context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        const size_t row = static_cast<size_t>(gpu.width) * 4u;
        const size_t bytes = 12u + row * gpu.height;
        uint8_t *file = static_cast<uint8_t *>(allocate(bytes));
        const uint32_t header[3] = {0x41524742u, gpu.width, gpu.height};
        for (size_t i = 0; i < 12u; i++) file[i] = reinterpret_cast<const uint8_t *>(header)[i];
        for (uint32_t y = 0; y < gpu.height; y++) {
            const uint8_t *from = static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
            uint8_t *to = file + 12u + y * row;
            for (size_t x = 0; x < row; x++) to[x] = from[x];
        }
        gpu.context->Unmap(staging, 0);
        written = write_file(path, file, bytes);
        release_memory(file);
    }
    staging->Release();
    return written;
}

struct Frames {
    uint32_t count = 0;
    double   recording = 0.0;
};

Frames render_blocks(Sample &sample, Gpu &gpu, uint32_t block, uint32_t least, double minimum, uint32_t cycle)
{
    const float  step = 1.0f / 60.0f;
    const double from = seconds();
    const uint32_t size = block != 0 ? block : 1;
    Frames frames;
    do {
        const double started = seconds();
        for (uint32_t frame = 0; frame < size; frame++, frames.count++) {
            const uint32_t at = cycle != 0 ? frames.count % cycle : frames.count;
            sample.render(gpu, static_cast<float>(at) * step);
            gpu.context->Flush();
        }
        frames.recording += seconds() - started;
        finish(gpu);
    } while (frames.count < least || seconds() - from < minimum);
    return frames;
}

void report(char *metrics, size_t capacity, const char *name, const char *status, double init_ms, uint32_t frames,
            double seconds_taken)
{
    char line[256] = {};
    copy_text(line, sizeof line, name);
    append_text(line, sizeof line, "\t");
    append_text(line, sizeof line, status);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, init_ms, 3);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, frames, 0);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, seconds_taken * 1000.0, 3);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, seconds_taken > 0 ? frames / seconds_taken : 0.0, 2);
    append_text(line, sizeof line, "\n");
    log(line);
    append_text(metrics, capacity, line);
}

void run_sample(Sample &sample, Gpu &gpu, const Options &options, char *metrics, size_t capacity)
{
    gpu.failed = false;
    const double started = seconds();
    const bool ready = sample.init(gpu) && !gpu.failed;
    finish(gpu);
    const double init_ms = (seconds() - started) * 1000.0;
    if (!ready || options.compile) {
        report(metrics, capacity, sample.name(), ready ? "compiled" : "init-failed", init_ms, 0, 0.0);
        sample.release();
        return;
    }
    render_blocks(sample, gpu, options.warmup, options.warmup, options.warmup_seconds, options.warmup);
    const double timed = seconds();
    const Frames frames = render_blocks(sample, gpu, options.frames, options.frames, options.timed_seconds, 0);
    const double taken = seconds() - timed;
    log_number("loop ms", frames.recording * 1000.0);
    log_number("finish ms", (taken - frames.recording) * 1000.0);
    sample.render(gpu, options.capture_time);
    gpu.context->Flush();
    finish(gpu);
    sample.render(gpu, options.capture_time);
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/");
    append_text(path, sizeof path, sample.name());
    append_text(path, sizeof path, ".bgra");
    const bool captured = !gpu.failed && capture(gpu, path);
    report(metrics, capacity, sample.name(), captured ? "ok" : "capture-failed", init_ms, frames.count, taken);
    sample.release();
    gpu.context->ClearState();
    gpu.context->Flush();
}

void save_metrics(const Options &options, const char *metrics)
{
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/metrics.tsv");
    size_t length = 0;
    while (metrics[length] != '\0') length++;
    write_file(path, metrics, length);
}

}

int run(int argc, const char *const *argv)
{
    Options options;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        if (same_text(arg, "--out")) options.out = value, i++;
        else if (same_text(arg, "--shaders")) set_shader_directory(value), i++;
        else if (same_text(arg, "--only")) options.only = value, i++;
        else if (same_text(arg, "--width")) options.width = parse_number(value), i++;
        else if (same_text(arg, "--height")) options.height = parse_number(value), i++;
        else if (same_text(arg, "--warmup")) options.warmup = parse_number(value), i++;
        else if (same_text(arg, "--frames")) options.frames = parse_number(value), i++;
        else if (same_text(arg, "--compile")) options.compile = true;
    }
    Gpu gpu;
    gpu.width = options.width;
    gpu.height = options.height;
    if (!create_device(&gpu.device, &gpu.context) || !make_target(gpu)) {
        log("FAIL: no Direct3D 11 device\n");
        return 2;
    }
    const size_t capacity = 64u * 1024u;
    char *metrics = static_cast<char *>(allocate(capacity));
    copy_text(metrics, capacity, "sample\tstatus\tinit_ms\tframes\tms\tfps\n");
    save_metrics(options, metrics);
    uint32_t count = 0;
    Sample **all = registry(count);
    for (uint32_t index = 0; index < count; index++) {
        if (options.only != nullptr && !same_text(options.only, all[index]->name())) continue;
        run_sample(*all[index], gpu, options, metrics, capacity);
        save_metrics(options, metrics);
    }
    release_memory(metrics);
    safe_release(gpu.target);
    safe_release(gpu.depth);
    safe_release(gpu.color);
    safe_release(gpu.context);
    safe_release(gpu.device);
    return 0;
}

}
