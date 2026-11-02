#include "framework.h"

namespace samples {

namespace {

struct Options {
    const char *out = ".";
    const char *only = nullptr;
    const char *gpu = nullptr;
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
    gpu.color = gpu.render_texture(GL_RGBA8, gpu.width, gpu.height);
    gpu.depth = gpu.render_texture(GL_DEPTH24_STENCIL8, gpu.width, gpu.height);
    glCreateFramebuffers(1, &gpu.target);
    glNamedFramebufferTexture(gpu.target, GL_COLOR_ATTACHMENT0, gpu.color, 0);
    glNamedFramebufferTexture(gpu.target, GL_DEPTH_STENCIL_ATTACHMENT, gpu.depth, 0);
    return glCheckNamedFramebufferStatus(gpu.target, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

bool capture(Gpu &gpu, const char *path)
{
    const size_t row = static_cast<size_t>(gpu.width) * 4u;
    const size_t bytes = 12u + row * gpu.height;
    uint8_t *file = static_cast<uint8_t *>(allocate(bytes));
    const uint32_t header[3] = {0x41524742u, gpu.width, gpu.height};
    for (size_t i = 0; i < 12u; i++) file[i] = reinterpret_cast<const uint8_t *>(header)[i];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gpu.target);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, static_cast<GLsizei>(gpu.width), static_cast<GLsizei>(gpu.height), GL_BGRA, GL_UNSIGNED_BYTE,
                 file + 12u);
    const bool written = glGetError() == GL_NO_ERROR && write_file(path, file, bytes);
    release_memory(file);
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

bool clean(const char *name, const char *where)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) return true;
    char line[256] = {};
    copy_text(line, sizeof line, name);
    append_text(line, sizeof line, ": GL error ");
    append_number(line, sizeof line, error, 0);
    append_text(line, sizeof line, " after ");
    append_text(line, sizeof line, where);
    append_text(line, sizeof line, "\n");
    log(line);
    while (glGetError() != GL_NO_ERROR) {
    }
    return false;
}

void run_sample(Sample &sample, Gpu &gpu, const Options &options, char *metrics, size_t capacity)
{
    gpu.failed = false;
    gpu.reset_state();
    const double started = seconds();
    const bool ready = sample.init(gpu) && !gpu.failed && clean(sample.name(), "init");
    glFinish();
    const double init_ms = (seconds() - started) * 1000.0;
    if (!ready || options.compile) {
        report(metrics, capacity, sample.name(), ready ? "compiled" : "init-failed", init_ms, 0, 0.0);
        sample.release();
        gpu.reset_state();
        return;
    }
    const float step = 1.0f / 60.0f;
    const double warm_from = seconds();
    for (uint32_t frame = 0; frame < options.warmup || seconds() - warm_from < options.warmup_seconds; frame++) {
        sample.render(gpu, static_cast<float>(frame % (options.warmup != 0 ? options.warmup : 1)) * step);
        glFlush();
    }
    glFinish();
    bool drawn = clean(sample.name(), "warm-up");
    const double timed = seconds();
    uint32_t     frames = 0;
    double       looped = 0.0;
    do {
        const double block = seconds();
        for (uint32_t frame = 0; frame < options.frames; frame++, frames++) {
            sample.render(gpu, static_cast<float>(frames) * step);
            glFlush();
        }
        looped += seconds() - block;
        glFinish();
    } while (seconds() - timed < options.timed_seconds);
    const double taken = seconds() - timed;
    log_number("loop ms", looped * 1000.0);
    log_number("finish ms", (taken - looped) * 1000.0);
    sample.render(gpu, options.capture_time);
    glFlush();
    glFinish();
    sample.render(gpu, options.capture_time);
    drawn = clean(sample.name(), "render") && drawn;
    char path[512] = {};
    copy_text(path, sizeof path, options.out);
    append_text(path, sizeof path, "/");
    append_text(path, sizeof path, sample.name());
    append_text(path, sizeof path, ".bgra");
    const bool captured = !gpu.failed && capture(gpu, path);
    report(metrics, capacity, sample.name(), !captured ? "capture-failed" : drawn ? "ok" : "gl-error", init_ms,
           frames, taken);
    sample.release();
    gpu.reset_state();
    glFlush();
}

}

int run(int argc, const char *const *argv)
{
    Options options;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        if (same_text(arg, "--out")) options.out = value, i++;
        else if (same_text(arg, "--only")) options.only = value, i++;
        else if (same_text(arg, "--gpu")) options.gpu = value, i++;
        else if (same_text(arg, "--width")) options.width = parse_number(value), i++;
        else if (same_text(arg, "--height")) options.height = parse_number(value), i++;
        else if (same_text(arg, "--warmup")) options.warmup = parse_number(value), i++;
        else if (same_text(arg, "--frames")) options.frames = parse_number(value), i++;
        else if (same_text(arg, "--compile")) options.compile = true;
    }
    if (!create_context(options.gpu)) {
        log("FAIL: no OpenGL 4.5 core context\n");
        return 2;
    }
    const char *missing = nullptr;
    if (!load_gl(missing)) {
        log("FAIL: the driver has no ");
        log(missing);
        log("\n");
        return 2;
    }
    log("renderer: ");
    log(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    log(" / ");
    log(reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    log("\n");
    Gpu gpu;
    gpu.width = options.width;
    gpu.height = options.height;
    if (!make_target(gpu)) {
        log("FAIL: no framebuffer\n");
        return 2;
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
    glDeleteFramebuffers(1, &gpu.target);
    glDeleteTextures(1, &gpu.color);
    glDeleteTextures(1, &gpu.depth);
    return 0;
}

}
