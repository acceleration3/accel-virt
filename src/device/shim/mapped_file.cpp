#include "device/shim/mapped_file.hpp"

#include "core/trace/function_scope.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace av {

MappedFile::~MappedFile()
{
    AV_SCOPE("MappedFile::~MappedFile");
    if (bytes_ != nullptr) munmap(const_cast<uint8_t *>(bytes_), length_);
}

Status MappedFile::open(const char *path)
{
    AV_SCOPE("MappedFile::open");
    if (open_) return Status::invalid_state;
    const int file = ::open(path, O_RDONLY | O_CLOEXEC);
    if (file < 0) return Status::not_found;
    struct stat about = {};
    if (fstat(file, &about) != 0 || about.st_size < 0) {
        close(file);
        return Status::not_found;
    }
    length_ = static_cast<size_t>(about.st_size);
    open_ = true;
    if (length_ == 0u) {
        close(file);
        return Status::ok;
    }
    void *const mapped = mmap(nullptr, length_, PROT_READ, MAP_PRIVATE, file, 0);
    close(file);
    if (mapped == MAP_FAILED) {
        length_ = 0u;
        return Status::no_memory;
    }
    bytes_ = static_cast<const uint8_t *>(mapped);
    return Status::ok;
}

const uint8_t *MappedFile::bytes() const
{
    AV_SCOPE("MappedFile::bytes");
    return bytes_;
}

size_t MappedFile::length() const
{
    AV_SCOPE("MappedFile::length");
    return length_;
}

}
