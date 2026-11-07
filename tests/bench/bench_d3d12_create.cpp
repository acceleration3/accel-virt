#include "bench_timer.hpp"

#include "harness/transport/roomed_transport.hpp"
#include "icd/d3d/d3d12_commitment.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"

#include <pthread.h>

#include <cstdint>
#include <cstdio>

namespace {

constexpr std::size_t command_bytes = 4096u;
constexpr std::size_t reply_bytes = 64u;

class HandoffTransport final : public av::RoomedTransport {
public:
    HandoffTransport()
        : RoomedTransport(command_bytes, reply_bytes), command_{}, reply_{}, decode_(nullptr, 0u), served_(0u),
          ready_(false), done_(false), stop_(false), pending_(0u)
    {
        pthread_create(&server_, nullptr, &HandoffTransport::serve, this);
    }

    ~HandoffTransport()
    {
        pthread_mutex_lock(&lock_);
        stop_ = true;
        pthread_cond_broadcast(&wake_);
        pthread_mutex_unlock(&lock_);
        pthread_join(server_, nullptr);
    }

    av::WireWriter request_writer() override { return av::WireWriter(command_, command_bytes); }

    av::TransportResult send_async(std::uint32_t, av::WireWriter &) override
    {
        pending_++;
        return av::TransportResult::ok;
    }

    av::TransportResult send_sync(std::uint32_t, av::WireWriter &, av::WireReader &reply) override
    {
        pthread_mutex_lock(&lock_);
        ready_ = true;
        done_ = false;
        pthread_cond_signal(&wake_);
        while (!done_) pthread_cond_wait(&answered_, &lock_);
        pthread_mutex_unlock(&lock_);
        reply = av::WireReader(reply_, sizeof(std::uint32_t));
        return av::TransportResult::ok;
    }

    void end_call() override {}
    av::WireArena &decode_arena() override { return decode_; }
    av::ClientBlobArena &blobs() override { return blobs_; }
    std::uint32_t served() const { return served_; }

private:
    static void *serve(void *self)
    {
        HandoffTransport &t = *static_cast<HandoffTransport *>(self);
        pthread_mutex_lock(&t.lock_);
        for (;;) {
            while (!t.ready_ && !t.stop_) pthread_cond_wait(&t.wake_, &t.lock_);
            if (t.stop_) break;
            t.ready_ = false;
            t.served_ += 1u + t.pending_;
            t.pending_ = 0u;
            t.done_ = true;
            pthread_cond_signal(&t.answered_);
        }
        pthread_mutex_unlock(&t.lock_);
        return nullptr;
    }

    std::uint8_t     command_[command_bytes];
    std::uint8_t     reply_[reply_bytes];
    av::WireArena    decode_;
    av::ClientBlobArena blobs_;
    std::uint32_t    served_;
    bool             ready_;
    bool             done_;
    bool             stop_;
    std::uint32_t    pending_;
    pthread_t        server_;
    pthread_mutex_t  lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t   wake_ = PTHREAD_COND_INITIALIZER;
    pthread_cond_t   answered_ = PTHREAD_COND_INITIALIZER;
};

}

int main()
{
    constexpr std::uint32_t creates = 2000u;
    av::BenchTimer timer(5u);
    HandoffTransport wire;
    av::d3d12proto::HeapProperties properties = {1u, 0u, 0u, 0u};
    av::d3d12proto::ResourceDesc desc = {};
    desc.dimension = 1u;
    desc.width = 65536u;
    desc.height = 1u;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.sample_count = 1u;
    av::d3d12proto::ClearValue clear = {};
    av::d3d::D3d12Commitment commitment;
    commitment.learn(std::uint64_t{64} << 30u);

    const double before = timer.nanoseconds_per_operation(creates, [&] {
        for (std::uint32_t id = 1u; id <= creates; id++) {
            std::uint32_t hresult = 1u;
            static_cast<void>(av::d3d12proto::av_d3d12_CreateCommittedResource12(wire, id, properties, 0u, desc, 0u, clear,
                                                                                 id * 65536u, &hresult));
        }
    });
    timer.report("create committed resource, synchronous (before)", creates, before);

    const double after = timer.nanoseconds_per_operation(creates, [&] {
        for (std::uint32_t id = 1u; id <= creates; id++) {
            if (commitment.charge(65536u) == av::d3d::D3d12Commitment::Admission::ask) {
                std::uint32_t hresult = 1u;
                static_cast<void>(av::d3d12proto::av_d3d12_CreateCommittedResource12(
                    wire, id, properties, 0u, desc, 0u, clear, id * 65536u, &hresult));
            } else {
                static_cast<void>(av::d3d12proto::av_d3d12_CreateCommittedResourceAsync12(
                    wire, id, properties, 0u, desc, 0u, clear, id * 65536u));
            }
        }
        std::uint32_t flushed = 1u;
        static_cast<void>(av::d3d12proto::av_d3d12_Finish12(wire, &flushed));
        commitment.release(std::uint64_t{creates} * 65536u);
    });
    timer.report("create committed resource, deferred (after)", creates, after);
    std::printf("served %u\n", wire.served());
    return 0;
}
