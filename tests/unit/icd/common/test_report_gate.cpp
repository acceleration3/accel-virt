/* A state dump's hold on its root's lock (ReportGate): the section opened,
 * busy=false and the lock held until the gate goes when the lock is free;
 * busy=true and the section closed at once, never waiting, when it is not. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/common/report_gate.hpp"

#include <cstring>

using av::icd::ReportGate;

TEST(report_gate, a_free_lock_is_held_until_the_gate_goes)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'R');
  char                    bytes[128] = {};
  av::StateWriter         writer(bytes, sizeof bytes);
  { const ReportGate gate(lock, writer, "root");
    CHECK(gate.held());
    CHECK(lock.held());
    writer.number("things", 3u);
    writer.close(); }
  CHECK(!lock.held());
  CHECK(std::strstr(writer.dump(), "root/0 busy=false things=3") != nullptr); }

TEST(report_gate, a_lock_held_elsewhere_is_busy_and_closes_the_section)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'R');
  lock.held_elsewhere(true);
  char            bytes[128] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  { const ReportGate gate(lock, writer, "umd");
    CHECK(!gate.held()); }
  writer.open("next", 0u);
  writer.close();
  CHECK(std::strstr(writer.dump(), "umd/0 busy=true") != nullptr);
  /* Closed: what follows is not inside it. */
  CHECK(std::strstr(writer.dump(), "next/0") != nullptr);
  CHECK(std::strstr(writer.dump(), "umd/0/next") == nullptr);
  lock.held_elsewhere(false); }
