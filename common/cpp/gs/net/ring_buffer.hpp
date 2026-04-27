#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace gs {
namespace net {

// 环形读缓冲区：替代 vector + memmove，避免数据移动
class RingBuffer {
public:
    explicit RingBuffer(size_t cap = 1024 * 1024);

    void Append(const uint8_t* data, size_t len);
    size_t Readable() const;

    // 检查 [offset, offset+len) 是否在连续区域（不跨越 buf_ 末尾）
    bool IsContiguous(size_t offset, size_t len) const;

    // 获取偏移处指针（仅在 IsContiguous 返回 true 时安全）
    const uint8_t* DataAt(size_t offset) const;

    // 读取到 out（支持跨环）
    void ReadAt(size_t offset, uint8_t* out, size_t len) const;

    // 消费 len 字节
    void Consume(size_t len);

private:
    void EnsureSpace(size_t len);

    std::vector<uint8_t> buf_;
    size_t rpos_ = 0;
    size_t wpos_ = 0;
    size_t size_ = 0;
};

} // namespace net
} // namespace gs
