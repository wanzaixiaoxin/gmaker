#include "buffer.hpp"

namespace gs {
namespace net {

Buffer Buffer::Allocate(size_t size) {
    Buffer buf;
    buf.own_ = std::make_shared<std::vector<uint8_t>>(size);
    buf.offset_ = 0;
    buf.length_ = size;
    return buf;
}

Buffer Buffer::FromVector(std::vector<uint8_t> data) {
    Buffer buf;
    buf.own_ = std::make_shared<std::vector<uint8_t>>(std::move(data));
    buf.offset_ = 0;
    buf.length_ = buf.own_->size();
    return buf;
}

Buffer Buffer::Wrap(std::shared_ptr<const std::vector<uint8_t>> data,
                    size_t offset, size_t len) {
    Buffer buf;
    if (!data) return buf;
    if (offset > data->size()) return buf;
    if (offset + len > data->size()) len = data->size() - offset;
    buf.shared_ = std::move(data);
    buf.offset_ = offset;
    buf.length_ = len;
    return buf;
}

Buffer Buffer::Slice(size_t offset, size_t len) const {
    if (offset > length_) return Buffer();
    if (offset + len > length_) len = length_ - offset;

    Buffer sliced;
    sliced.offset_ = offset_ + offset;
    sliced.length_ = len;
    if (own_) {
        sliced.own_ = own_;  // 共享同一个 own_
    } else {
        sliced.shared_ = shared_;  // 共享同一个 shared_
    }
    return sliced;
}

uint8_t* Buffer::Data() {
    if (own_) return own_->data() + offset_;
    return nullptr;  // 只读共享 Buffer 不提供写指针
}

const uint8_t* Buffer::Data() const {
    if (own_) return own_->data() + offset_;
    if (shared_) return shared_->data() + offset_;
    return nullptr;
}

std::vector<uint8_t> Buffer::ToVector() const {
    if (length_ == 0) return {};
    const uint8_t* p = Data();
    if (!p) return {};
    return std::vector<uint8_t>(p, p + length_);
}

} // namespace net
} // namespace gs
