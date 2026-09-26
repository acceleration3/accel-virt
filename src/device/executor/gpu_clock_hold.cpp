#include "device/executor/gpu_clock_hold.hpp"

#include "core/trace/function_scope.hpp"

#include <drm/amdgpu_drm.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

namespace av {

namespace {

void *run_clock_hold(void *hold)
{
    AV_SCOPE("run_clock_hold");
    static_cast<GpuClockHold *>(hold)->watch();
    return nullptr;
}

}  // namespace

GpuClockHold::GpuClockHold()
    : fd_(-1), context_(0u), thread_{}, lock_(PTHREAD_MUTEX_INITIALIZER), wake_(PTHREAD_COND_INITIALIZER),
      last_busy_(0u), held_since_(0u), hold_for_(0u), peak_mhz_(0u), slow_(0u), held_(false), wanted_(false), stopping_(false), open_(false)
{
    AV_SCOPE("GpuClockHold::GpuClockHold");
}

GpuClockHold::~GpuClockHold()
{
    AV_SCOPE("GpuClockHold::~GpuClockHold");
    close();
}

bool GpuClockHold::open(int32_t render_minor)
{
    AV_SCOPE("GpuClockHold::open");
    if (open_) return true;
    if (render_minor < 0) return false;
    char path[32];
    snprintf(path, sizeof path, "/dev/dri/renderD%d", render_minor);
    fd_ = ::open(path, O_RDWR | O_CLOEXEC);
    if (fd_ < 0) return false;
    union drm_amdgpu_ctx made = {};
    made.in.op = AMDGPU_CTX_OP_ALLOC_CTX;
    if (ioctl(fd_, DRM_IOCTL_AMDGPU_CTX, &made) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    context_ = made.out.alloc.ctx_id;
    peak_mhz_ = sensor(AMDGPU_INFO_SENSOR_PEAK_PSTATE_GFX_SCLK);
    stopping_ = false;
    pthread_condattr_t clock;
    pthread_condattr_init(&clock);
    pthread_condattr_setclock(&clock, CLOCK_MONOTONIC);
    pthread_cond_destroy(&wake_);
    pthread_cond_init(&wake_, &clock);
    pthread_condattr_destroy(&clock);
    if (pthread_create(&thread_, nullptr, &run_clock_hold, this) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    open_ = true;
    return true;
}

void GpuClockHold::busy()
{
    AV_SCOPE("GpuClockHold::busy");
    if (!__atomic_load_n(&open_, __ATOMIC_ACQUIRE)) return;
    __atomic_store_n(&last_busy_, now(), __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&wanted_, __ATOMIC_RELAXED) || __atomic_exchange_n(&wanted_, true, __ATOMIC_SEQ_CST)) return;
    pthread_mutex_lock(&lock_);
    pthread_cond_signal(&wake_);
    pthread_mutex_unlock(&lock_);
}

void GpuClockHold::watch()
{
    AV_SCOPE("GpuClockHold::watch");
    pthread_mutex_lock(&lock_);
    while (!stopping_) {
        if (now() - __atomic_load_n(&last_busy_, __ATOMIC_RELAXED) >= release_after_ns) {
            if (held_) let_go();
            slow_ = 0u;
            __atomic_store_n(&wanted_, false, __ATOMIC_SEQ_CST);
            /* A batch after the store above either sees wanted_ clear and
             * signals, or was counted before the idle test: no wake is lost. */
            if (now() - __atomic_load_n(&last_busy_, __ATOMIC_SEQ_CST) >= release_after_ns)
                pthread_cond_wait(&wake_, &lock_);
            continue;
        }
        timespec until = {};
        clock_gettime(CLOCK_MONOTONIC, &until);
        until.tv_nsec += static_cast<long>(look_every_ns);
        until.tv_sec += until.tv_nsec / 1000000000L;
        until.tv_nsec %= 1000000000L;
        pthread_cond_timedwait(&wake_, &lock_, &until);
        if (stopping_) break;
        if (held_) {
            if (now() - held_since_ >= hold_for_) let_go();
            continue;
        }
        slow_ = slow() ? slow_ + 1u : 0u;
        if (slow_ >= slow_looks) take();
    }
    if (held_) let_go();
    pthread_mutex_unlock(&lock_);
}

bool GpuClockHold::slow() const
{
    AV_SCOPE("GpuClockHold::slow");
    if (peak_mhz_ == 0u) return false;
    return sensor(AMDGPU_INFO_SENSOR_GPU_LOAD) >= busy_percent &&
           sensor(AMDGPU_INFO_SENSOR_GFX_SCLK) * 10u < peak_mhz_ * 9u;
}

/* A refusal - another process holds a stable state - is left be: the next
 * slow stretch asks again. */
void GpuClockHold::take()
{
    AV_SCOPE("GpuClockHold::take");
    slow_ = 0u;
    if (!request(AMDGPU_CTX_STABLE_PSTATE_PEAK)) return;
    __atomic_store_n(&held_, true, __ATOMIC_RELEASE);
    /* A hold that follows a look soon after the last one let go is one the
     * driver's own clock did not beat: it may stay longer. */
    const uint64_t taken = now();
    hold_for_ = taken - held_since_ < probe_every_ns + first_probe_ns ? probe_every_ns : first_probe_ns;
    held_since_ = taken;
}

void GpuClockHold::let_go()
{
    AV_SCOPE("GpuClockHold::let_go");
    static_cast<void>(request(AMDGPU_CTX_STABLE_PSTATE_NONE));
    __atomic_store_n(&held_, false, __ATOMIC_RELEASE);
    slow_ = 0u;
}

uint32_t GpuClockHold::sensor(uint32_t type) const
{
    AV_SCOPE("GpuClockHold::sensor");
    uint32_t              value = 0u;
    struct drm_amdgpu_info asked = {};
    asked.return_pointer = reinterpret_cast<uint64_t>(&value);
    asked.return_size = sizeof value;
    asked.query = AMDGPU_INFO_SENSOR;
    asked.sensor_info.type = type;
    return ioctl(fd_, DRM_IOCTL_AMDGPU_INFO, &asked) == 0 ? value : 0u;
}

void GpuClockHold::close()
{
    AV_SCOPE("GpuClockHold::close");
    if (!open_) return;
    pthread_mutex_lock(&lock_);
    stopping_ = true;
    pthread_cond_signal(&wake_);
    pthread_mutex_unlock(&lock_);
    pthread_join(thread_, nullptr);
    union drm_amdgpu_ctx freed = {};
    freed.in.op = AMDGPU_CTX_OP_FREE_CTX;
    freed.in.ctx_id = context_;
    static_cast<void>(ioctl(fd_, DRM_IOCTL_AMDGPU_CTX, &freed));
    ::close(fd_);
    fd_ = -1;
    open_ = false;
}

bool GpuClockHold::request(uint32_t pstate)
{
    AV_SCOPE("GpuClockHold::request");
    union drm_amdgpu_ctx asked = {};
    asked.in.op = AMDGPU_CTX_OP_SET_STABLE_PSTATE;
    asked.in.ctx_id = context_;
    asked.in.flags = pstate;
    return ioctl(fd_, DRM_IOCTL_AMDGPU_CTX, &asked) == 0;
}

uint64_t GpuClockHold::now() const
{
    AV_SCOPE("GpuClockHold::now");
    timespec clock = {};
    clock_gettime(CLOCK_MONOTONIC, &clock);
    return static_cast<uint64_t>(clock.tv_sec) * 1000000000u + static_cast<uint64_t>(clock.tv_nsec);
}

}  // namespace av
