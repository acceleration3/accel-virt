#include "harness/guest/directory_image_sink.hpp"

#include <cstdio>
#include <sys/stat.h>

namespace av {

DirectoryImageSink::DirectoryImageSink(const char *directory) : directory_(directory), bytes_{}, written_(0u) {}

void DirectoryImageSink::save(const char *name, const BmpImage &image)
{
    if (image.encode(bytes_, sizeof bytes_) != Status::ok) return;
    static_cast<void>(mkdir(directory_, 0755));
    char path[512];
    std::snprintf(path, sizeof path, "%s/%s.bmp", directory_, name);
    std::FILE *const file = std::fopen(path, "wb");
    if (file == nullptr) return;
    if (std::fwrite(bytes_, 1u, image.encoded_size(), file) == image.encoded_size()) written_++;
    std::fclose(file);
}

std::size_t DirectoryImageSink::written() const { return written_; }

}  // namespace av
