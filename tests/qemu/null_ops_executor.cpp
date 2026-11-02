#include "av_executor.h"

extern "C" __attribute__((visibility("default"))) const AvExecutorOps *av_executor_ops(void)
{
    return nullptr;
}
