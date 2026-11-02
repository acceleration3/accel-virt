#pragma once

#include <stdint.h>

class VulkanExampleBase;

struct SampleEntry
{
	const char *name;
	VulkanExampleBase *(*make)();
	void (*destroy)(VulkanExampleBase *);
};

const SampleEntry *sampleRegistry(uint32_t &count);
