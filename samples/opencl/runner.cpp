// Runs every sample: warm-up iterations, timed iterations, then the capture both
// machines are compared by. The D3D11 suite's runner, with a kernel's iteration
// where it has a frame and clFinish where it has an event query.
#include "framework.h"

namespace samples {

namespace {

struct Options {
    const char *out = ".";
    const char *only = nullptr;
    uint32_t    device = 0;
    uint32_t    warmup = 10;
    double      warmup_seconds = 1.0;
    double      timed_seconds = 1.0;
    uint32_t    frames = 20;
    bool        list = false;
};

uint32_t parse_number(const char *text)
{
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10u + static_cast<uint32_t>(*text++ - '0');
    return value;
}

void report(char *metrics, size_t capacity, const char *name, const char *status, double init_ms, uint32_t iterations,
            double seconds_taken, Work work)
{
    char line[320] = {};
    copy_text(line, sizeof line, name);
    append_text(line, sizeof line, "\t");
    append_text(line, sizeof line, status);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, init_ms, 3);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, iterations, 0);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, seconds_taken * 1000.0, 3);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, seconds_taken > 0 ? iterations / seconds_taken : 0.0, 2);
    append_text(line, sizeof line, "\t");
    append_number(line, sizeof line, seconds_taken > 0 ? work.amount * iterations / seconds_taken : 0.0, 3);
    append_text(line, sizeof line, "\t");
    append_text(line, sizeof line, work.unit);
    append_text(line, sizeof line, "\n");
    log(line);
    append_text(metrics, capacity, line);
}

void run_sample(Sample &sample, Gpu &gpu, const Options &options, char *metrics, size_t capacity)
{
    gpu.failed = false;
    const double started = seconds();
    const bool   ready = sample.init(gpu) && !gpu.failed;
    gpu.finish();
    const double init_ms = (seconds() - started) * 1000.0;
    if (!ready) {
        report(metrics, capacity, sample.name(), "init-failed", init_ms, 0, 0.0, sample.work());
        sample.release();
        return;
    }
    /* The GPU's clocks climb over the first few hundred milliseconds of load,
     * so the warm-up runs for a time as well as a count. */
    const double warm_from = seconds();
    for (uint32_t i = 0; !gpu.failed && (i < options.warmup || seconds() - warm_from < options.warmup_seconds); i++) {
        sample.step(gpu);
        gpu.flush();
        if (i % 8u == 7u) gpu.finish();
    }
    gpu.finish();
    /* Blocks of the iterations asked for, each finished, until a second has
     * gone: nothing bounds how much a queue takes ahead of the device. */
    const double timed = seconds();
    uint32_t     iterations = 0;
    double       looped = 0.0;
    do {
        const double block = seconds();
        for (uint32_t i = 0; i < options.frames && !gpu.failed; i++, iterations++) {
            sample.step(gpu);
            gpu.flush();
        }
        looped += seconds() - block;
        gpu.finish();
    } while (!gpu.failed && seconds() - timed < options.timed_seconds);
    const double taken = seconds() - timed;
    log_number("loop ms ", looped * 1000.0);
    log_number("finish ms ", (taken - looped) * 1000.0);
    if (gpu.failed) {
        report(metrics, capacity, sample.name(), "run-failed", init_ms, iterations, taken, sample.work());
        sample.release();
        return;
    }
    Capture capture;
    capture.directory = options.out;
    capture.sample = sample.name();
    const char *verdict = sample.capture(gpu, capture);
    gpu.finish();
    const char *status = gpu.failed || capture.failed ? "capture-failed" : verdict;
    report(metrics, capacity, sample.name(), status, init_ms, iterations, taken, sample.work());
    sample.release();
}

}  // namespace

int run(int argc, const char *const *argv)
{
    Options options;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        if (same_text(arg, "--out")) options.out = value, i++;
        else if (same_text(arg, "--only")) options.only = value, i++;
        else if (same_text(arg, "--device")) options.device = parse_number(value), i++;
        else if (same_text(arg, "--warmup")) options.warmup = parse_number(value), i++;
        else if (same_text(arg, "--frames")) options.frames = parse_number(value), i++;
        else if (same_text(arg, "--list")) options.list = true;
    }
    uint32_t count = 0;
    Sample **all = registry(count);
    if (options.list) {
        for (uint32_t index = 0; index < count; index++) {
            log(all[index]->name());
            log("\n");
        }
        return 0;
    }
    if (!cl.load()) {
        log("FAIL: no OpenCL loader\n");
        return 2;
    }
    Gpu gpu;
    if (!gpu.open(options.device)) {
        log("FAIL: no OpenCL device\n");
        return 2;
    }
    char line[320] = {};
    copy_text(line, sizeof line, "device: ");
    append_text(line, sizeof line, gpu.device_name);
    append_text(line, sizeof line, "\n");
    log(line);
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/device.txt");
    size_t length = 0;
    while (gpu.device_name[length] != '\0') length++;
    write_file(path, gpu.device_name, length);

    const size_t capacity = 64u * 1024u;
    char        *metrics = static_cast<char *>(allocate(capacity));
    copy_text(metrics, capacity, "sample\tstatus\tinit_ms\titerations\tms\tips\trate\tunit\n");
    for (uint32_t index = 0; index < count; index++) {
        if (options.only != nullptr && !same_text(options.only, all[index]->name())) continue;
        run_sample(*all[index], gpu, options, metrics, capacity);
    }
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/metrics.tsv");
    length = 0;
    while (metrics[length] != '\0') length++;
    write_file(path, metrics, length);
    release_memory(metrics);
    gpu.close();
    return 0;
}

}  // namespace samples
