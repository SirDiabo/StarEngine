#pragma once
#include <cstddef>
#include <cstdint>
#include <istream>
#include <memory>
#include <streambuf>
#include <vector>

class MemoryStreamBuf : public std::streambuf {
public:
    MemoryStreamBuf(const char* data, size_t size) {
        char* start = const_cast<char*>(data);
        setg(start, start, start + size);
    }

protected:
    pos_type seekoff(off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        if (!(which & std::ios_base::in)) return pos_type(off_type(-1));
        off_type base = 0;
        if (dir == std::ios_base::cur) base = (off_type)(gptr() - eback());
        else if (dir == std::ios_base::end) base = (off_type)(egptr() - eback());
        const off_type target = base + offset;
        if (target < 0 || target > (off_type)(egptr() - eback())) return pos_type(off_type(-1));
        setg(eback(), eback() + target, egptr());
        return pos_type(target);
    }

    pos_type seekpos(pos_type position, std::ios_base::openmode which) override {
        return seekoff(off_type(position), std::ios_base::beg, which);
    }
};

class MemoryStream : public std::istream {
public:
    explicit MemoryStream(std::shared_ptr<const std::vector<uint8_t>> bytes)
        : std::istream(nullptr),
          owner(std::move(bytes)),
          buffer(owner ? reinterpret_cast<const char*>(owner->data()) : nullptr, owner ? owner->size() : 0) {
        rdbuf(&buffer);
    }

    MemoryStream(const MemoryStream&) = delete;
    MemoryStream& operator=(const MemoryStream&) = delete;

private:
    std::shared_ptr<const std::vector<uint8_t>> owner;
    MemoryStreamBuf buffer;
};
