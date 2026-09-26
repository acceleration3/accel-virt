// Runs every sample off-screen: warm-up frames, timed frames, then one frame
// at a fixed time read back as the picture both machines are compared by.
// The same protocol as samples/d3d11/runner.cpp, where a D3D11 Flush is a
// D3D12 submit of the frame's command list and a finish is a fence wait.
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
    uint32_t    roundtrips = 0;
    bool        spin_waits = false;
    bool        shared_fence = false;
};

uint32_t parse_number(const char *text)
{
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10u + static_cast<uint32_t>(*text++ - '0');
    return value;
}

// The frame as B8G8R8A8 rows: "BGRA", width, height, then the texels. The
// capture frame is submitted first, with any lists the sample queued after
// its own, and the copy follows in a list of its own.
bool capture(Gpu &gpu, const char *path)
{
    gpu.submit();
    gpu.open();
    const uint32_t pitch = (gpu.width * 4u + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
                           ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = static_cast<UINT64>(pitch) * gpu.height;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *readback = nullptr;
    if (FAILED(gpu.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr, __uuidof(ID3D12Resource),
                                                   reinterpret_cast<void **>(&readback)))) {
        gpu.submit();
        return false;
    }
    gpu.barrier(gpu.color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION to = {};
    to.pResource = readback;
    to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint.Footprint = {Gpu::color_format, gpu.width, gpu.height, 1, pitch};
    D3D12_TEXTURE_COPY_LOCATION from = {};
    from.pResource = gpu.color;
    from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    gpu.list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    gpu.barrier(gpu.color, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    gpu.submit();
    gpu.finish();
    bool written = false;
    const uint8_t *mapped = nullptr;
    if (!gpu.failed && SUCCEEDED(readback->Map(0, nullptr, reinterpret_cast<void **>(const_cast<uint8_t **>(&mapped))))) {
        const size_t row = static_cast<size_t>(gpu.width) * 4u;
        const size_t bytes = 12u + row * gpu.height;
        uint8_t *file = static_cast<uint8_t *>(allocate(bytes));
        const uint32_t header[3] = {0x41524742u, gpu.width, gpu.height};   /* "BGRA" */
        copy_bytes(file, header, 12u);
        for (uint32_t y = 0; y < gpu.height; y++)
            copy_bytes(file + 12u + y * row, mapped + static_cast<size_t>(y) * pitch, row);
        const D3D12_RANGE none = {0, 0};
        readback->Unmap(0, &none);
        written = write_file(path, file, bytes);
        release_memory(file);
    }
    readback->Release();
    return written;
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

void frame(Sample &sample, Gpu &gpu, float time)
{
    gpu.open();
    sample.render(gpu, time);
    gpu.submit();
}

void retire(Sample &sample, Gpu &gpu)
{
    gpu.finish();
    sample.release();
    gpu.reset_sample_state();
}

void run_sample(Sample &sample, Gpu &gpu, const Options &options, char *metrics, size_t capacity)
{
    gpu.failed = false;
    const double started = seconds();
    gpu.open();
    const bool ready = sample.init(gpu) && !gpu.failed;
    gpu.submit();
    gpu.finish();
    const double init_ms = (seconds() - started) * 1000.0;
    if (!ready || options.compile) {
        report(metrics, capacity, sample.name(), ready ? "compiled" : "init-failed", init_ms, 0, 0.0);
        retire(sample, gpu);
        return;
    }
    const float step = 1.0f / 60.0f;
    /* The GPU's clocks climb over the first few hundred milliseconds of load,
     * so the warm-up runs for a time as well as a count: otherwise whichever
     * run followed another on a warm GPU looks faster than the one that did
     * not. */
    const double warm_from = seconds();
    for (uint32_t n = 0; n < options.warmup || seconds() - warm_from < options.warmup_seconds; n++)
        frame(sample, gpu, static_cast<float>(n % (options.warmup != 0 ? options.warmup : 1)) * step);
    gpu.finish();
    /* Blocks of the frames asked for, each finished, until a second has gone:
     * a run of a few milliseconds measures whatever else the machine did
     * then. Within a block the CPU runs at most Gpu::slots frames ahead, as a
     * D3D12 application with that many frames in flight does. */
    const double timed = seconds();
    uint32_t     frames = 0;
    double       looped = 0.0;
    /* Each second's rate as well as the whole run's, so a long run shows
     * whether the rate holds or sags while the same process renders. */
    double   second_from = timed;
    uint32_t second_frames = 0;
    double   waited_from = gpu.waited;
    uint32_t waits_from = gpu.waits;
    double   submitting_from = gpu.submitting;
    double   gpu_time_from = gpu.gpu_time;
    double   recording_from = gpu.recording;
    double   signalling_from = gpu.signalling;
    log_number("timestamp frequency ", static_cast<double>(gpu.stamp_frequency));
    uint32_t gpu_frames_from = gpu.gpu_frames;
    do {
        const double block = seconds();
        for (uint32_t n = 0; n < options.frames; n++, frames++) frame(sample, gpu, static_cast<float>(frames) * step);
        looped += seconds() - block;
        gpu.finish();
        const double now = seconds();
        if (now - second_from >= 1.0) {
            log_number("second fps ", (frames - second_frames) / (now - second_from));
            log_number("second wait us/frame ", (gpu.waited - waited_from) * 1e6 / (frames - second_frames));
            log_number("second waits/frame ", double(gpu.waits - waits_from) / (frames - second_frames));
            log_number("second submit us/frame ", (gpu.submitting - submitting_from) * 1e6 / (frames - second_frames));
            log_number("second record us/frame ", (gpu.recording - recording_from) * 1e6 / (frames - second_frames));
            recording_from = gpu.recording;
            log_number("second signal us/frame ", (gpu.signalling - signalling_from) * 1e6 / (frames - second_frames));
            signalling_from = gpu.signalling;
            second_from = now;
            second_frames = frames;
            waited_from = gpu.waited;
            waits_from = gpu.waits;
            submitting_from = gpu.submitting;
            if (gpu.gpu_frames != gpu_frames_from)
                log_number("second gpu us/frame ", (gpu.gpu_time - gpu_time_from) * 1e6 / (gpu.gpu_frames - gpu_frames_from));
            gpu_time_from = gpu.gpu_time;
            gpu_frames_from = gpu.gpu_frames;
        }
    } while (seconds() - timed < options.timed_seconds);
    const double taken = seconds() - timed;
    log_number("loop ms", looped * 1000.0);
    log_number("finish ms", (taken - looped) * 1000.0);
    /* Twice: a sample that shows what its previous frame measured - the
     * occlusion counts - then shows the capture time's own, however many
     * frames each machine managed before it. */
    frame(sample, gpu, options.capture_time);
    gpu.finish();
    gpu.open();
    sample.render(gpu, options.capture_time);
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/");
    append_text(path, sizeof path, sample.name());
    append_text(path, sizeof path, ".bgra");
    const bool captured = capture(gpu, path) && !gpu.failed;
    report(metrics, capacity, sample.name(), captured ? "ok" : "capture-failed", init_ms, frames, taken);
    retire(sample, gpu);
}

}  // namespace

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
        else if (same_text(arg, "--seconds")) options.timed_seconds = parse_number(value), i++;
        else if (same_text(arg, "--roundtrips")) options.roundtrips = parse_number(value), i++;
        else if (same_text(arg, "--spin-waits")) options.spin_waits = true;
        else if (same_text(arg, "--shared-fence")) options.shared_fence = true;
        else if (same_text(arg, "--compile")) options.compile = true;
    }
    Gpu gpu;
    gpu.spin_waits = options.spin_waits;
    gpu.shared_fence = options.shared_fence;
    gpu.width = options.width;
    gpu.height = options.height;
    if (!create_device(&gpu.device)) {
        log("FAIL: no Direct3D 12 device\n");
        return 2;
    }
    if (!gpu.initialize()) {
        log("FAIL: the Direct3D 12 device could not be set up\n");
        return 2;
    }
    /* An empty list, its signal and the wait for it, over and over: how long
     * one submission takes to come back when there is no work in it, the
     * floor under any frame that waits for the last one. */
    if (options.roundtrips != 0) {
        double shortest = 1e9;
        const double began = seconds();
        for (uint32_t n = 0; n < options.roundtrips; n++) {
            const double from = seconds();
            gpu.open();
            gpu.submit();
            gpu.finish();
            const double took = seconds() - from;
            if (took < shortest) shortest = took;
        }
        log_number("roundtrip mean us ", (seconds() - began) * 1e6 / options.roundtrips);
        log_number("roundtrip min us ", shortest * 1e6);
    }
    const size_t capacity = 64u * 1024u;
    char *metrics = static_cast<char *>(allocate(capacity));
    copy_text(metrics, capacity, "sample\tstatus\tinit_ms\tframes\tms\tfps\n");
    uint32_t count = 0;
    Sample **all = registry(count);
    for (uint32_t index = 0; index < count; index++) {
        if (options.only != nullptr && !same_text(options.only, all[index]->name())) continue;
        run_sample(*all[index], gpu, options, metrics, capacity);
    }
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/metrics.tsv");
    size_t length = 0;
    while (metrics[length] != '\0') length++;
    write_file(path, metrics, length);
    release_memory(metrics);
    gpu.shutdown();
    safe_release(gpu.device);
    return 0;
}

}  // namespace samples
