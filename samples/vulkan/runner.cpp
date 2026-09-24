// Runs the examples off-screen with samples/d3d11/runner.cpp's semantics:
// warm-up frames for a count and a second, then blocks of timed frames, each
// block finished, until a second has passed; then one moment rendered twice
// and read back as the picture both machines are compared by. The examples'
// own render() draws each frame, and this drives the bookkeeping upstream's
// renderLoop() does around it (frame time, the animation timer, the overlay's
// once-a-second refresh), on a fixed 1/60 s step.
#include "prelude.hpp"
#include "registry.hpp"

#include <atomic>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct Options
{
	const char *out = ".";
	const char *only = nullptr;
	const char *data = "data/";
	uint32_t width = 1280;
	uint32_t height = 720;
	uint32_t warmup = 60;
	double warmupSeconds = 1.0;
	double timedSeconds = 1.0;
	uint32_t frames = 600;
	float captureTime = 1.5f;
	// A sample that has not reported by then is stuck: its line says so and the program ends.
	uint32_t watchdogSeconds = 300;
	bool compile = false;
	bool list = false;
};

const float frameStep = 1.0f / 60.0f;

uint32_t parseNumber(const char *text)
{
	uint32_t value = 0;
	while (*text >= '0' && *text <= '9')
	{
		value = value * 10u + static_cast<uint32_t>(*text++ - '0');
	}
	return value;
}

double seconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

class HeadlessRunner
{
public:
	explicit HeadlessRunner(const Options &options);
	int run();
	// The line of whichever sample was running when the process ended early: a
	// failed VK_CHECK_RESULT exits, and the report must still see the sample.
	void reportUnfinished(const char *how);
	void watch();

private:
	const Options &options;
	std::string metrics = "sample\tstatus\tinit_ms\tframes\tms\tfps\n";
	std::atomic<const char *> current{nullptr};
	// "init", "render" or "capture": where a sample that never reported stopped.
	std::atomic<const char *> phase{"init"};
	std::atomic<double> deadline{0.0};
	double fpsFrom = 0.0;

	VulkanExampleBase *start(const SampleEntry &entry);
	void frame(VulkanExampleBase *example, float step);
	void tick(VulkanExampleBase *example);
	bool capture(VulkanExampleBase *example, const std::string &path);
	void report(const char *name, const char *status, double initMs, uint32_t frames, double taken);
	void runSample(const SampleEntry &entry);
	void writeMetrics();
};

HeadlessRunner::HeadlessRunner(const Options &options) : options(options)
{
}

VulkanExampleBase *HeadlessRunner::start(const SampleEntry &entry)
{
	VulkanExampleBase *example = entry.make();
	example->width = options.width;
	example->height = options.height;
	example->initSwapchain();
	example->prepare();
	vkQueueWaitIdle(example->queue);
	// renderLoop() leaves the first frame 1 s of frame time; a fixed step instead.
	example->frameTimer = frameStep;
	return example;
}

void HeadlessRunner::frame(VulkanExampleBase *example, float step)
{
	example->frameTimer = step;
	example->render();
	example->frameCounter++;
	if (!example->paused)
	{
		example->timer += example->timerSpeed * step;
		if (example->timer > 1.0)
		{
			example->timer -= 1.0f;
		}
	}
}

// renderLoop()'s once-a-second fps count and overlay refresh, which the
// samples showing text pay for every second.
void HeadlessRunner::tick(VulkanExampleBase *example)
{
	const double now = seconds();
	if (now - fpsFrom < 1.0)
	{
		return;
	}
	example->lastFPS = example->frameCounter;
	example->updateTextOverlay();
	example->frameCounter = 0;
	fpsFrom = now;
}

// The presented image as B8G8R8A8 rows: "BGRA", width, height, then the texels.
bool HeadlessRunner::capture(VulkanExampleBase *example, const std::string &path)
{
	VkDevice device = example->device;
	const uint32_t width = example->width;
	const uint32_t height = example->height;
	const VkDeviceSize bytes = VkDeviceSize(width) * height * 4u;
	VkImage image = example->swapChain.buffers[example->currentBuffer].image;

	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkBufferCreateInfo bufferInfo = vkTools::initializers::bufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_DST_BIT, bytes);
	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		return false;
	}
	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, buffer, &requirements);
	VkMemoryAllocateInfo allocateInfo = vkTools::initializers::memoryAllocateInfo();
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = example->getMemoryType(requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS)
	{
		vkDestroyBuffer(device, buffer, nullptr);
		return false;
	}
	vkBindBufferMemory(device, buffer, memory, 0);

	VkCommandBuffer copy = example->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkImageMemoryBarrier toSource = vkTools::initializers::imageMemoryBarrier();
	toSource.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
	toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	toSource.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	toSource.image = image;
	toSource.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdPipelineBarrier(copy, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		0, nullptr, 0, nullptr, 1, &toSource);
	VkBufferImageCopy region = {};
	region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.imageExtent = { width, height, 1 };
	vkCmdCopyImageToBuffer(copy, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
	VkImageMemoryBarrier back = toSource;
	back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	back.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	back.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkBufferMemoryBarrier toHost = vkTools::initializers::bufferMemoryBarrier();
	toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toHost.buffer = buffer;
	toHost.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(copy, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		0, nullptr, 1, &toHost, 1, &back);
	example->flushCommandBuffer(copy, example->queue, true);

	bool written = false;
	void *mapped = nullptr;
	if (vkMapMemory(device, memory, 0, bytes, 0, &mapped) == VK_SUCCESS)
	{
		FILE *file = fopen(path.c_str(), "wb");
		if (file != nullptr)
		{
			const uint32_t header[3] = { 0x41524742u, width, height };
			written = fwrite(header, sizeof header, 1, file) == 1 && fwrite(mapped, size_t(bytes), 1, file) == 1;
			written = fclose(file) == 0 && written;
		}
		vkUnmapMemory(device, memory);
	}
	vkDestroyBuffer(device, buffer, nullptr);
	vkFreeMemory(device, memory, nullptr);
	return written;
}

void HeadlessRunner::report(const char *name, const char *status, double initMs, uint32_t frames, double taken)
{
	char line[256];
	snprintf(line, sizeof line, "%s\t%s\t%.3f\t%u\t%.3f\t%.2f\n", name, status, initMs, frames, taken * 1000.0,
		taken > 0 ? frames / taken : 0.0);
	fputs(line, stdout);
	fflush(stdout);
	metrics += line;
}

void HeadlessRunner::runSample(const SampleEntry &entry)
{
	current = entry.name;
	phase = "init";
	deadline = seconds() + options.watchdogSeconds;
	const double started = seconds();
	VulkanExampleBase *example = start(entry);
	const double initMs = (seconds() - started) * 1000.0;
	if (!example->prepared || options.compile)
	{
		report(entry.name, example->prepared ? "compiled" : "init-failed", initMs, 0, 0.0);
		entry.destroy(example);
		current = nullptr;
		return;
	}
	phase = "render";
	fpsFrom = seconds();
	/* The GPU's clocks climb over the first few hundred milliseconds of load,
	 * so the warm-up runs for a time as well as a count. */
	const double warmFrom = seconds();
	for (uint32_t count = 0; count < options.warmup || seconds() - warmFrom < options.warmupSeconds; count++)
	{
		frame(example, frameStep);
		tick(example);
	}
	vkQueueWaitIdle(example->queue);
	/* Blocks of the frames asked for, each finished, until a second has gone. */
	const double timed = seconds();
	uint32_t frames = 0;
	double looped = 0.0;
	do
	{
		const double block = seconds();
		for (uint32_t count = 0; count < options.frames; count++, frames++)
		{
			frame(example, frameStep);
			tick(example);
		}
		looped += seconds() - block;
		vkQueueWaitIdle(example->queue);
	} while (seconds() - timed < options.timedSeconds);
	const double taken = seconds() - timed;
	printf("loop ms%.3f\nfinish ms%.3f\n", looped * 1000.0, (taken - looped) * 1000.0);
	entry.destroy(example);

	/* The picture: a fresh example stepped to the capture time, so a sample
	 * that accumulates its own state - particles, a skeleton's clock - shows
	 * the same moment however many frames each machine managed above. Then
	 * that moment twice, so one showing what its previous frame measured -
	 * the occlusion counts - shows the capture time's own. */
	phase = "capture";
	example = start(entry);
	const uint32_t steps = uint32_t(options.captureTime / frameStep + 0.5f);
	for (uint32_t step = 0; step < steps; step++)
	{
		frame(example, frameStep);
	}
	frame(example, 0.0f);
	vkQueueWaitIdle(example->queue);
	frame(example, 0.0f);
	vkQueueWaitIdle(example->queue);
	const std::string path = std::string(options.out) + "/" + entry.name + ".bgra";
	const bool captured = capture(example, path);
	report(entry.name, captured ? "ok" : "capture-failed", initMs, frames, taken);
	entry.destroy(example);
	current = nullptr;
}

void HeadlessRunner::writeMetrics()
{
	const std::string path = std::string(options.out) + "/metrics.tsv";
	FILE *file = fopen(path.c_str(), "wb");
	if (file == nullptr)
	{
		return;
	}
	fwrite(metrics.data(), 1, metrics.size(), file);
	fclose(file);
}

// how is "failed" for a process ending early, "hung" for the watchdog.
void HeadlessRunner::reportUnfinished(const char *how)
{
	const char *name = current.exchange(nullptr);
	if (name == nullptr)
	{
		return;
	}
	const std::string status = std::string(how) + "-in-" + phase.load();
	report(name, status.c_str(), 0.0, 0, 0.0);
	writeMetrics();
}

void HeadlessRunner::watch()
{
	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		if (current.load() != nullptr && seconds() > deadline.load())
		{
			reportUnfinished("hung");
			_Exit(4);
		}
	}
}

int HeadlessRunner::run()
{
	uint32_t count = 0;
	const SampleEntry *entries = sampleRegistry(count);
	if (options.list)
	{
		for (uint32_t index = 0; index < count; index++)
		{
			printf("%s\n", entries[index].name);
		}
		return 0;
	}
	std::string root = options.data;
	if (!root.empty() && root.back() != '/' && root.back() != '\\')
	{
		root += "/";
	}
	VulkanExampleBase::assetRoot = root;
	std::thread watchdog(&HeadlessRunner::watch, this);
	watchdog.detach();
	for (uint32_t index = 0; index < count; index++)
	{
		if (options.only != nullptr && strcmp(options.only, entries[index].name) != 0)
		{
			continue;
		}
		runSample(entries[index]);
	}
	writeMetrics();
	return 0;
}

namespace {

HeadlessRunner *running = nullptr;

void reportAtExit()
{
	if (running != nullptr)
	{
		running->reportUnfinished("failed");
	}
}

}  // namespace

int main(int argc, char **argv)
{
#if defined(_WIN32)
	// A crash ends the process instead of waiting on a dialog nobody sees.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
	Options options;
	for (int i = 1; i < argc; i++)
	{
		const char *arg = argv[i];
		const char *value = i + 1 < argc ? argv[i + 1] : "";
		if (!strcmp(arg, "--out")) options.out = value, i++;
		else if (!strcmp(arg, "--data")) options.data = value, i++;
		else if (!strcmp(arg, "--only")) options.only = value, i++;
		else if (!strcmp(arg, "--width")) options.width = parseNumber(value), i++;
		else if (!strcmp(arg, "--height")) options.height = parseNumber(value), i++;
		else if (!strcmp(arg, "--warmup")) options.warmup = parseNumber(value), i++;
		else if (!strcmp(arg, "--frames")) options.frames = parseNumber(value), i++;
		else if (!strcmp(arg, "--watchdog")) options.watchdogSeconds = parseNumber(value), i++;
		else if (!strcmp(arg, "--compile")) options.compile = true;
		else if (!strcmp(arg, "--list")) options.list = true;
	}
	HeadlessRunner runner(options);
	running = &runner;
	atexit(reportAtExit);
	const int status = runner.run();
	running = nullptr;
	return status;
}
