#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

namespace procebo::pipe {
    struct descriptor {
        int32_t rfd;
        int32_t wfd;
    };
    
    inline descriptor create() {
        descriptor desc;

        if (::pipe(reinterpret_cast<int32_t*>(&desc)) < 0) throw std::runtime_error("Pipe open error!");

        return desc;
    }

    inline void close(descriptor desc) {
        if (::close(desc.rfd) < 0) throw std::runtime_error("Pipe close error!");
        if (::close(desc.wfd) < 0) throw std::runtime_error("Pipe close error!");
    }

    inline ssize_t read(descriptor desc, std::byte* buffer, size_t size) {
        ssize_t rsize = ::read(desc.rfd, buffer, size);

        if (rsize < 0) throw std::runtime_error("Pipe read error!");

        return rsize;
    }

    inline std::vector<std::byte> read(descriptor desc, size_t size) {
        std::vector<std::byte> buffer(size);
        buffer.resize(read(desc, buffer.data(), size));

        return buffer;
    }

    inline std::string readstring(descriptor desc, size_t size) {
        std::string buffer(size, 0);
        buffer.resize(read(desc, reinterpret_cast<std::byte*>(buffer.data()), size));

        return buffer;
    }

    inline ssize_t write(descriptor desc, const std::byte* buffer, size_t size) {
        ssize_t wsize = ::write(desc.wfd, buffer, size);

        if (wsize < 0) throw std::runtime_error("Pipe write error!");

        return wsize;
    }

    inline ssize_t write(descriptor desc, const std::vector<std::byte>& buffer) {
        return write(desc, buffer.data(), buffer.size());
    }

    inline ssize_t writestring(descriptor desc, const std::string& buffer) {
        return write(desc, reinterpret_cast<const std::byte*>(buffer.data()), buffer.size());
    }
}