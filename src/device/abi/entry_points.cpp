#include "device/abi/entry_points.hpp"

#include "device/threading/worker.hpp"

extern "C" {

void RvkWorkerRun(void *worker)
{ static_cast<av::Worker *>(worker)->run(); }

}
