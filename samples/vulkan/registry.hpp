// The examples in the program, in the order the runner takes them.
#pragma once

#include <stdint.h>

class VulkanExampleBase;

struct SampleEntry
{
	const char *name;
	VulkanExampleBase *(*make)();
	// Through the example's own type: VulkanExampleBase's destructor is not virtual.
	void (*destroy)(VulkanExampleBase *);
};

const SampleEntry *sampleRegistry(uint32_t &count);
