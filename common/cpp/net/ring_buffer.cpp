#include "ring_buffer.hpp"
#include <cstring>
#include <algorithm>

namespace gs {
namespace net {

RingBuffer::RingBuffer(size_t cap) : buf_(cap) {}

void RingBuffer::Append(const uint8_t* data, size_t len) {
    EnsureSpace(len);
    size_t pos = wpos_;
    size_t remaining = len;
    while (remaining > 0) {
        size_t chunk = std::min(remaining, buf_.size() - pos);
        std::memcpy(buf_.data() + pos, data + (len - remaining), chunk);
        pos = (pos + chunk) % buf_.size();
        remaining -= chunk;
    }
    wpos_ = pos;
    size_ += len;
}

size_t RingBuffer::Readable() const {
    return size_;
}

bool RingBuffer::IsContiguous(size_t offset, size_t len) const {
    if (offset + len > size_) return false;
    size_t start = (rpos_ + offset) % buf_.size();
    return start + len <= buf_.size();
}

const uint8_t* RingBuffer::DataAt(size_t offset) const {
    return buf_.data() + (rpos_ + offset) % buf_.size();
}

void RingBuffer::ReadAt(size_t offset, uint8_t* out, size_t len) const {
    size_t start = (rpos_ + offset) % buf_.size();
    size_t copied = 0;
    while (copied < len) {
        size_t chunk = std::min(len - copied, buf_.size() - start);
        std::memcpy(out + copied, buf_.data() + start, chunk);
        copied += chunk;
        start = 0;
    }
}

void RingBuffer::Consume(size_t len) {
    len = std::min(len, size_);
    rpos_ = (rpos_ + len) % buf_.size();
    size_ -= len;
}

void RingBuffer::EnsureSpace(size_t len) {
    if (size_ + len <= buf_.size()) return;

    size_t new_cap = buf_.size();
    while (new_cap < size_ + len) new_cap *= 2;

    std::vector<uint8_t> new_buf(new_cap);
    if (size_ > 0) {
        size_t pos = rpos_;
        size_t copied = 0;
        while (copied < size_) {
            size_t chunk = std::min(size_ - copied, buf_.size() - pos);
            std::memcpy(new_buf.data() + copied, buf_.data() + pos, chunk);
            copied += chunk;
            pos = 0;
        }
    }

    buf_ = std::move(new_buf);
    rpos_ = 0;
    wpos_ = size_;
}

} // namespace net
} // namespace gs
