// buffertransfer, imagetransfer, mapbuffer, kernellaunch, programstream: what it costs to move
// bytes and commands between the application and the device - the part of a
// paravirtual driver a kernel's own speed says nothing about.
#include "framework.h"

namespace samples {

namespace {

uint32_t pattern(uint32_t i, uint32_t seed) { return mix_bits(i * 0x9E3779B1u + seed); }

// ---------------------------------------------------------------- buffertransfer

// Each iteration writes 8 MiB in, copies it on the device, fills the source,
// and reads the copy back out.
class BufferTransfer final : public Sample {
public:
    const char *name() const override { return "buffertransfer"; }
    Work        work() const override { return {2.0 * bytes / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        host_in_ = static_cast<uint32_t *>(allocate(bytes));
        host_out_ = static_cast<uint32_t *>(allocate(bytes));
        for (uint32_t i = 0; i < words; i++) host_in_[i] = pattern(i, 1);
        source_ = gpu.buffer(bytes);
        copy_ = gpu.buffer(bytes);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        gpu.write(source_, host_in_, bytes);
        gpu.check(cl.EnqueueCopyBuffer(gpu.queue, source_, copy_, 0, 0, bytes, 0, nullptr, nullptr),
                  "clEnqueueCopyBuffer");
        const uint32_t fill = 0xDEADBEEFu;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, source_, &fill, 4, 0, bytes, 0, nullptr, nullptr),
                  "clEnqueueFillBuffer");
        gpu.read(copy_, host_out_, bytes);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        for (uint32_t i = 0; i < words; i++) host_in_[i] = pattern(i, 7);
        step(gpu);
        // An unaligned read and write of an odd length, within one buffer.
        const uint32_t marker[5] = {1, 2, 3, 4, 5};
        gpu.write(copy_, marker, sizeof marker, 4 * 1001);
        uint32_t window[16] = {};
        gpu.read(copy_, window, sizeof window, 4 * 999);
        uint32_t *filled = static_cast<uint32_t *>(allocate(bytes));
        gpu.read(source_, filled, bytes);
        bool right = !gpu.failed;
        for (uint32_t i = 0; i < words && right; i++) right = host_out_[i] == pattern(i, 7) && filled[i] == 0xDEADBEEFu;
        for (uint32_t i = 0; i < 16 && right; i++) {
            const uint32_t at = 999 + i;
            right = window[i] == (at >= 1001 && at < 1006 ? marker[at - 1001] : pattern(at, 7));
        }
        // A digest of the whole copy, then the window, then the copy's head.
        uint32_t digest[4 + 16 + 1024] = {};
        for (uint32_t i = 0; i < words; i++) {
            digest[0] += host_out_[i];
            digest[1] ^= host_out_[i] * (i | 1u);
            digest[2] += filled[i];
        }
        digest[3] = words;
        copy_bytes(digest + 4, window, sizeof window);
        copy_bytes(digest + 20, host_out_, 1024 * 4u);
        capture.buffer(Capture::u32, digest, sizeof digest / 4u);
        release_memory(filled);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(source_);
        samples::release(copy_);
        release_memory(host_in_);
        release_memory(host_out_);
        host_in_ = host_out_ = nullptr;
    }

private:
    static constexpr uint32_t bytes = 8u << 20;
    static constexpr uint32_t words = bytes / 4u;
    cl_mem                    source_ = nullptr, copy_ = nullptr;
    uint32_t                 *host_in_ = nullptr, *host_out_ = nullptr;
};

// ---------------------------------------------------------------- imagetransfer

// A 1024x1024 RGBA8 image written in, copied image to image, and read back; the
// capture is also a sub-rectangle written at an offset.
class ImageTransfer final : public Sample {
public:
    const char *name() const override { return "imagetransfer"; }
    Work        work() const override { return {2.0 * size * size * 4.0 / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        host_in_ = static_cast<uint32_t *>(allocate(size * size * 4u));
        host_out_ = static_cast<uint32_t *>(allocate(size * size * 4u));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float u = static_cast<float>(x) / size, v = static_cast<float>(y) / size;
                const float n = fbm(u * 6.0f, v * 6.0f, 9u, 5);
                host_in_[y * size + x] = pack_rgba(n, u, v, 1.0f);
            }
        source_ = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8);
        copy_ = gpu.image2d(size, size, CL_RGBA, CL_UNORM_INT8);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        gpu.write_image(source_, host_in_, size, size);
        const size_t origin[3] = {0, 0, 0}, region[3] = {size, size, 1};
        gpu.check(cl.EnqueueCopyImage(gpu.queue, source_, copy_, origin, origin, region, 0, nullptr, nullptr),
                  "clEnqueueCopyImage");
        gpu.read_image(copy_, host_out_, size, size);
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        bool right = !gpu.failed;
        for (uint32_t i = 0; i < size * size && right; i++) right = host_out_[i] == host_in_[i];
        // A 200x100 block of a flat colour written at (300, 400), with a pitch
        // wider than the block.
        const uint32_t pitch = 256, w = 200, h = 100;
        uint32_t      *block = static_cast<uint32_t *>(allocate(pitch * h * 4u));
        for (uint32_t i = 0; i < pitch * h; i++) block[i] = pack_rgba(1.0f, 0.2f, 0.6f, 1.0f);
        const size_t origin[3] = {300, 400, 0}, region[3] = {w, h, 1};
        gpu.check(cl.EnqueueWriteImage(gpu.queue, copy_, CL_TRUE, origin, region, pitch * 4u, 0, block, 0, nullptr,
                                       nullptr),
                  "clEnqueueWriteImage");
        gpu.read_image(copy_, host_out_, size, size);
        for (uint32_t y = 0; y < size && right; y++)
            for (uint32_t x = 0; x < size && right; x++) {
                const bool inside = x >= 300 && x < 300 + w && y >= 400 && y < 400 + h;
                right = host_out_[y * size + x] == (inside ? block[0] : host_in_[y * size + x]);
            }
        if (!gpu.failed) capture.image_rgba(reinterpret_cast<const uint8_t *>(host_out_), size, size);
        release_memory(block);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(source_);
        samples::release(copy_);
        release_memory(host_in_);
        release_memory(host_out_);
        host_in_ = host_out_ = nullptr;
    }

private:
    static constexpr uint32_t size = 1024;
    cl_mem                    source_ = nullptr, copy_ = nullptr;
    uint32_t                 *host_in_ = nullptr, *host_out_ = nullptr;
};

// ---------------------------------------------------------------- mapbuffer

// A host-accessible buffer written through a map, incremented by a kernel, and
// read through a map: the zero-copy path where a driver has one.
class MapBuffer final : public Sample {
public:
    const char *name() const override { return "mapbuffer"; }
    Work        work() const override { return {2.0 * bytes / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void bump(__global uint *values, uint by) { values[get_global_id(0)] += by; })");
        kernel_ = gpu.kernel(program_, "bump");
        buffer_ = gpu.buffer(bytes, nullptr, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
        gpu.arg(kernel_, 0, buffer_);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override { round(gpu, iteration_++); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        const uint32_t checksum = round(gpu, 12345u);
        cl_int         error = CL_SUCCESS;
        uint32_t      *mapped = static_cast<uint32_t *>(
            cl.EnqueueMapBuffer(gpu.queue, buffer_, CL_TRUE, CL_MAP_READ, 0, bytes, 0, nullptr, nullptr, &error));
        if (!gpu.check(error, "clEnqueueMapBuffer")) return "mismatch";
        bool right = true;
        for (uint32_t i = 0; i < words && right; i++) right = mapped[i] == (i ^ 12345u) + 12345u * 3u;
        uint32_t head[1025] = {};
        copy_bytes(head, mapped, 1024 * 4u);
        head[1024] = checksum;
        gpu.check(cl.EnqueueUnmapMemObject(gpu.queue, buffer_, mapped, 0, nullptr, nullptr), "clEnqueueUnmapMemObject");
        capture.buffer(Capture::u32, head, 1025);
        return right && !gpu.failed ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(buffer_);
        iteration_ = 0;
    }

private:
    static constexpr uint32_t bytes = 8u << 20;
    static constexpr uint32_t words = bytes / 4u;

    // Writes i ^ seed through a map, adds 3 * seed on the device, and answers
    // a checksum of a strided sample read back through a map.
    uint32_t round(Gpu &gpu, uint32_t seed)
    {
        cl_int    error = CL_SUCCESS;
        uint32_t *mapped = static_cast<uint32_t *>(cl.EnqueueMapBuffer(
            gpu.queue, buffer_, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, bytes, 0, nullptr, nullptr, &error));
        if (!gpu.check(error, "clEnqueueMapBuffer")) return 0;
        for (uint32_t i = 0; i < words; i++) mapped[i] = i ^ seed;
        gpu.check(cl.EnqueueUnmapMemObject(gpu.queue, buffer_, mapped, 0, nullptr, nullptr), "clEnqueueUnmapMemObject");
        gpu.arg(kernel_, 1, seed * 3u);
        gpu.run(kernel_, words, 256);
        mapped = static_cast<uint32_t *>(
            cl.EnqueueMapBuffer(gpu.queue, buffer_, CL_TRUE, CL_MAP_READ, 0, bytes, 0, nullptr, nullptr, &error));
        if (!gpu.check(error, "clEnqueueMapBuffer")) return 0;
        uint32_t checksum = 0;
        for (uint32_t i = 0; i < words; i += 64) checksum += mapped[i];
        gpu.check(cl.EnqueueUnmapMemObject(gpu.queue, buffer_, mapped, 0, nullptr, nullptr), "clEnqueueUnmapMemObject");
        return checksum;
    }

    cl_program program_ = nullptr;
    cl_kernel  kernel_ = nullptr;
    cl_mem     buffer_ = nullptr;
    uint32_t   iteration_ = 0;
};

// ---------------------------------------------------------------- kernellaunch

// A thousand launches of a one-work-item kernel, each with an argument set
// before it: nothing but the cost of a command.
class KernelLaunch final : public Sample {
public:
    const char *name() const override { return "kernellaunch"; }
    Work        work() const override { return {launches / 1e3, "klaunch/s"}; }
    bool        init(Gpu &gpu) override
    {
        program_ = gpu.program(R"(
__kernel void count(__global uint *counter, uint by) { counter[0] += by; })");
        kernel_ = gpu.kernel(program_, "count");
        counter_ = gpu.buffer(4);
        gpu.arg(kernel_, 0, counter_);
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        for (uint32_t i = 0; i < launches; i++) {
            gpu.arg(kernel_, 1, i);
            gpu.run(kernel_, 1, 1);
        }
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        const uint32_t zero = 0;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, counter_, &zero, 4, 0, 4, 0, nullptr, nullptr),
                  "clEnqueueFillBuffer");
        step(gpu);
        uint32_t counted = 0;
        gpu.read(counter_, &counted, 4);
        capture.buffer(Capture::u32, &counted, 1);
        return !gpu.failed && counted == launches * (launches - 1) / 2 ? "ok" : "mismatch";
    }
    void release() override
    {
        samples::release(kernel_);
        samples::release(program_);
        samples::release(counter_);
    }

private:
    static constexpr uint32_t launches = 1000;
    cl_program                program_ = nullptr;
    cl_kernel                 kernel_ = nullptr;
    cl_mem                    counter_ = nullptr;
};

// ---------------------------------------------------------------- heldmaps

// Hundreds of small host-accessible buffers, every one of them mapped at once,
// written, and only then unmapped: what Affinity does to open a document. A
// driver that holds only a few mappings at a time refuses it.
class HeldMaps final : public Sample {
public:
    const char *name() const override { return "heldmaps"; }
    Work        work() const override { return {1.0 * buffers * bytes / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        for (uint32_t i = 0; i < buffers; i++)
            buffers_[i] = gpu.buffer(bytes, nullptr, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
        return !gpu.failed;
    }
    void        step(Gpu &gpu) override { round(gpu, iteration_++); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        round(gpu, 777u);
        uint32_t sums[buffers] = {};
        for (uint32_t i = 0; i < buffers; i++) {
            uint32_t head[4] = {};
            gpu.read(buffers_[i], head, sizeof head);
            sums[i] = head[0] + head[3];
        }
        bool right = !gpu.failed;
        for (uint32_t i = 0; i < buffers && right; i++) right = sums[i] == (777u ^ i) + (777u ^ (i + 3u));
        capture.buffer(Capture::u32, sums, buffers);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        for (cl_mem &buffer : buffers_) samples::release(buffer);
        iteration_ = 0;
    }

private:
    static constexpr uint32_t buffers = 600;
    static constexpr uint32_t bytes = 4096;

    // Maps every buffer, writes seed ^ (buffer + word) into each, and unmaps
    // them all once every one is mapped.
    void round(Gpu &gpu, uint32_t seed)
    {
        uint32_t *mapped[buffers] = {};
        for (uint32_t i = 0; i < buffers; i++) {
            cl_int error = CL_SUCCESS;
            mapped[i] = static_cast<uint32_t *>(cl.EnqueueMapBuffer(
                gpu.queue, buffers_[i], CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, bytes, 0, nullptr, nullptr, &error));
            if (!gpu.check(error, "clEnqueueMapBuffer")) return;
            for (uint32_t word = 0; word < bytes / 4u; word++) mapped[i][word] = seed ^ (i + word);
        }
        for (uint32_t i = 0; i < buffers; i++)
            gpu.check(cl.EnqueueUnmapMemObject(gpu.queue, buffers_[i], mapped[i], 0, nullptr, nullptr),
                      "clEnqueueUnmapMemObject");
    }

    cl_mem   buffers_[buffers] = {};
    uint32_t iteration_ = 0;
};

// ---------------------------------------------------------------- bufferrects

// Rectangles of a buffer written, filled and read back with every origin and
// pitch away from zero, judged byte by byte against the same copies done on
// the host: the shape of what Affinity does to start a document.
class BufferRects final : public Sample {
public:
    const char *name() const override { return "bufferrects"; }
    Work        work() const override { return {1.0 * bytes / 1e9, "GB/s"}; }
    bool        init(Gpu &gpu) override
    {
        buffer_ = gpu.buffer(bytes);
        return !gpu.failed;
    }
    void        step(Gpu &gpu) override { round(gpu); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        const bool right = round(gpu);
        uint32_t   verdict = right ? 1u : 0u;
        capture.buffer(Capture::u32, &verdict, 1u);
        return right && !gpu.failed ? "ok" : "mismatch";
    }
    void release() override { samples::release(buffer_); }

private:
    static constexpr size_t bytes = 64u * 1024u;
    static constexpr size_t buffer_row = 256u, buffer_slice = 256u * 40u;
    static constexpr size_t host_row = 300u, host_slice = 300u * 45u;
    static constexpr size_t host_bytes = host_slice * 4u;

    bool round(Gpu &gpu)
    {
        static unsigned char source[host_bytes], back[host_bytes], model[bytes], read[bytes];
        for (size_t i = 0; i < host_bytes; i++) source[i] = static_cast<unsigned char>(i * 7u + 3u);
        const unsigned char zero = 0u;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, buffer_, &zero, 1, 0, bytes, 0, nullptr, nullptr), "clEnqueueFillBuffer");
        for (size_t i = 0; i < bytes; i++) model[i] = 0u;
        const size_t buffer_origin[3] = {16u, 3u, 1u}, host_origin[3] = {8u, 2u, 1u}, region[3] = {100u, 5u, 2u};
        gpu.check(cl.EnqueueWriteBufferRect(gpu.queue, buffer_, CL_TRUE, buffer_origin, host_origin, region, buffer_row,
                                            buffer_slice, host_row, host_slice, source, 0, nullptr, nullptr),
                  "clEnqueueWriteBufferRect");
        for (size_t z = 0; z < region[2]; z++)
            for (size_t y = 0; y < region[1]; y++)
                for (size_t x = 0; x < region[0]; x++)
                    model[(buffer_origin[2] + z) * buffer_slice + (buffer_origin[1] + y) * buffer_row + buffer_origin[0] + x] =
                        source[(host_origin[2] + z) * host_slice + (host_origin[1] + y) * host_row + host_origin[0] + x];
        const uint32_t pattern = 0xA5C3E1F7u;
        gpu.check(cl.EnqueueFillBuffer(gpu.queue, buffer_, &pattern, 4, 4096, 512, 0, nullptr, nullptr),
                  "clEnqueueFillBuffer");
        for (size_t i = 0; i < 512; i++) model[4096 + i] = static_cast<unsigned char>(pattern >> (8u * (i % 4u)));
        for (size_t i = 0; i < host_bytes; i++) back[i] = 0xEEu;
        gpu.check(cl.EnqueueReadBufferRect(gpu.queue, buffer_, CL_TRUE, buffer_origin, host_origin, region, buffer_row,
                                           buffer_slice, host_row, host_slice, back, 0, nullptr, nullptr),
                  "clEnqueueReadBufferRect");
        bool right = !gpu.failed;
        for (size_t z = 0; z < region[2] && right; z++)
            for (size_t y = 0; y < region[1] && right; y++)
                for (size_t x = 0; x < region[0] && right; x++) {
                    const size_t at = (host_origin[2] + z) * host_slice + (host_origin[1] + y) * host_row + host_origin[0] + x;
                    right = back[at] == source[at];
                }
        right = right && back[0] == 0xEEu && back[host_bytes - 1u] == 0xEEu;
        gpu.read(buffer_, read, bytes);
        for (size_t i = 0; i < bytes && right; i++) right = read[i] == model[i];
        return right;
    }

    cl_mem buffer_ = nullptr;
};

// ---------------------------------------------------------------- eventcallback

// A write's completion told through clSetEventCallback after a flush, with no
// wait of the application's own: what Affinity waits on to make a document. A
// round whose callback never came in two seconds is a failure, and every
// round is one - the first callback of a process is not the only one.
class EventCallback final : public Sample {
public:
    const char *name() const override { return "eventcallback"; }
    Work        work() const override { return {1.0, "callbacks/s"}; }
    bool        init(Gpu &gpu) override
    {
        buffer_ = gpu.buffer(bytes);
        return !gpu.failed;
    }
    void        step(Gpu &gpu) override { round(gpu); }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        const bool came = round(gpu);
        uint32_t   verdict = came ? 1u : 0u;
        capture.buffer(Capture::u32, &verdict, 1u);
        return came && !gpu.failed ? "ok" : "no callback";
    }
    void release() override { samples::release(buffer_); }

private:
    static constexpr size_t bytes = 4096u;

    static void CL_CALLBACK done(cl_event, cl_int status, void *flag)
    {
        __atomic_store_n(static_cast<int *>(flag), status == CL_COMPLETE ? 1 : 2, __ATOMIC_RELEASE);
    }

    bool round(Gpu &gpu)
    {
        static unsigned char source[bytes];
        static int           flag;
        __atomic_store_n(&flag, 0, __ATOMIC_RELEASE);
        cl_event written = nullptr;
        gpu.check(cl.EnqueueWriteBuffer(gpu.queue, buffer_, CL_FALSE, 0, bytes, source, 0, nullptr, &written),
                  "clEnqueueWriteBuffer");
        if (written == nullptr) return false;
        gpu.check(cl.SetEventCallback(written, CL_COMPLETE, &done, &flag), "clSetEventCallback");
        gpu.check(cl.Flush(gpu.queue), "clFlush");
        const double start = seconds();
        while (__atomic_load_n(&flag, __ATOMIC_ACQUIRE) == 0 && seconds() - start < 2.0) {}
        const bool came = __atomic_load_n(&flag, __ATOMIC_ACQUIRE) == 1;
        cl.ReleaseEvent(written);
        gpu.check(cl.Finish(gpu.queue), "clFinish");
        return came;
    }

    cl_mem buffer_ = nullptr;
};

}  // namespace

// ---------------------------------------------------------------- programstream

// Six programs of 300 KB of source each - past the room one command is lent,
// so each crosses in pieces over several batches - built one after another
// with a buffer made from the application's data between each two, as an
// application loading kernels while it streams data in. Each program's kernel
// writes its own mark into its own buffer. The guest driver holds what it
// sends, and the transport once took a command's arrays to be read by the
// batch carrying its first piece; a command of its own sent between pieces,
// or an array lent again too early, would build one program from another's
// source, and its marks would come back in the wrong buffer.
class ProgramStream final : public Sample {
public:
    const char *name() const override { return "programstream"; }
    Work        work() const override { return {static_cast<double>(programs), "programs"}; }
    bool        init(Gpu &gpu) override
    {
        source_ = static_cast<char *>(allocate(padding_bytes + 512));
        for (uint32_t program = 0; program < programs; program++) {
            program_source(program);
            programs_[program] = gpu.program(source_);
            kernels_[program] = gpu.kernel(programs_[program], "mark");
            uint32_t *data = static_cast<uint32_t *>(allocate(elements * sizeof(uint32_t)));
            for (uint32_t i = 0; i < elements; i++) data[i] = pattern(i, program);
            data_[program] = gpu.buffer(elements * sizeof(uint32_t), data);
            marks_[program] = gpu.buffer(elements * sizeof(uint32_t));
            release_memory(data);
        }
        return !gpu.failed;
    }
    void step(Gpu &gpu) override
    {
        for (uint32_t program = 0; program < programs; program++) {
            gpu.arg(kernels_[program], 0, marks_[program]);
            gpu.arg(kernels_[program], 1, data_[program]);
            gpu.run(kernels_[program], elements);
        }
    }
    const char *capture(Gpu &gpu, Capture &capture) override
    {
        step(gpu);
        uint32_t *digest = static_cast<uint32_t *>(allocate(programs * elements * sizeof(uint32_t)));
        bool      right = !gpu.failed;
        for (uint32_t program = 0; program < programs; program++) {
            uint32_t *read = digest + program * elements;
            gpu.read(marks_[program], read, elements * sizeof(uint32_t));
            for (uint32_t i = 0; i < elements && right; i++)
                right = read[i] == (mark_of(program) ^ pattern(i, program));
        }
        right = right && !gpu.failed;
        capture.buffer(Capture::u32, digest, programs * elements);
        release_memory(digest);
        return right ? "ok" : "mismatch";
    }
    void release() override
    {
        for (uint32_t program = 0; program < programs; program++) {
            samples::release(kernels_[program]);
            samples::release(programs_[program]);
            samples::release(data_[program]);
            samples::release(marks_[program]);
        }
        release_memory(source_);
        source_ = nullptr;
    }

private:
    static constexpr uint32_t programs = 6;
    static constexpr uint32_t elements = 4096;
    static constexpr uint32_t padding_bytes = 300u * 1024u;

    static uint32_t mark_of(uint32_t program) { return 0x51000000u + program * 0x10101u; }

    // The program's mark, its kernel, and 300 KB of comment after them.
    void program_source(uint32_t program)
    {
        copy_text(source_, 512, "#define MARK ");
        append_number(source_, 512, mark_of(program), 0);
        append_text(source_, 512, "u\n__kernel void mark(__global uint *out, __global const uint *in) {\n"
                                   "    out[get_global_id(0)] = MARK ^ in[get_global_id(0)];\n}\n");
        uint32_t at = 0;
        while (source_[at] != 0) at++;
        for (uint32_t i = 0; i < padding_bytes; i++)
            source_[at + i] = i % 64 == 63 ? '\n' : (i % 64 < 2 ? '/' : static_cast<char>('a' + (i + program) % 26));
        source_[at + padding_bytes] = 0;
    }

    char     *source_ = nullptr;
    cl_program programs_[programs] = {};
    cl_kernel  kernels_[programs] = {};
    cl_mem     data_[programs] = {};
    cl_mem     marks_[programs] = {};
};

void register_transfer(Sample **all, uint32_t &made)
{
    all[made++] = new BufferTransfer();
    all[made++] = new ImageTransfer();
    all[made++] = new MapBuffer();
    all[made++] = new KernelLaunch();
    all[made++] = new HeldMaps();
    all[made++] = new BufferRects();
    all[made++] = new EventCallback();
    all[made++] = new ProgramStream();
}

}  // namespace samples
