/* The C entry points the host calls back into the device core through: each
 * one line, forwarding to the object behind its context pointer. */
#ifndef AV_DEVICE_ENTRY_POINTS_HPP
#define AV_DEVICE_ENTRY_POINTS_HPP

extern "C" {

void RvkWorkerRun(void *worker);

}

#endif  // AV_DEVICE_ENTRY_POINTS_HPP
