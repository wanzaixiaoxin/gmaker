#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>

namespace gs {
namespace net {

// Buffer 引用计数缓冲区，支持零拷贝切片共享
//
// 设计目标：
//   1. 广播场景下编码一次、共享 N 次，避免内存拷贝
//   2. 向后兼容：提供 ToVector() 按需拷贝为传统 vector
//   3. 空 Buffer 安全（Data() 返回 nullptr，Size() 返回 0）
//
// 内部使用 std::shared_ptr 管理底层数据生命周期。
// 对于可变场景（如编码器输出），使用 Allocate/FromVector；
// 对于只读共享场景（如广播），使用 Wrap + Slice。
class Buffer {
public:
    Buffer() = default;

    // 从 vector 移动构造（向后兼容）
    Buffer(std::vector<uint8_t> data) : Buffer(FromVector(std::move(data))) {}

    // 分配指定大小的可写缓冲区
    static Buffer Allocate(size_t size);

    // 从 vector 移动构造（获得所有权）
    static Buffer FromVector(std::vector<uint8_t> data);

    // 包装外部共享的只读数据（零拷贝引用）
    static Buffer Wrap(std::shared_ptr<const std::vector<uint8_t>> data,
                       size_t offset, size_t len);

    // 零拷贝切片：共享同一份底层数据，仅调整 offset/length
    // 若 offset+len 越界，返回空 Buffer
    Buffer Slice(size_t offset, size_t len) const;

    // 获取可写指针。仅当 Buffer 通过 Allocate/FromVector 创建时有效，
    // 否则返回 nullptr（表示只读共享 Buffer）
    uint8_t* Data();

    // 获取只读指针。空 Buffer 返回 nullptr
    const uint8_t* Data() const;

    // 当前可见长度
    size_t Size() const { return length_; }

    bool Empty() const { return length_ == 0; }

    // 索引访问（只读）
    uint8_t operator[](size_t idx) const {
        const uint8_t* p = Data();
        return p ? p[idx] : 0;
    }

    // 向后兼容：深拷贝为 std::vector<uint8_t>
    std::vector<uint8_t> ToVector() const;

private:
    // own_ : 可变所有权（Allocate / FromVector）
    // shared_ : 只读共享引用（Wrap）
    // 两者互斥，至多一个非空
    std::shared_ptr<std::vector<uint8_t>> own_;
    std::shared_ptr<const std::vector<uint8_t>> shared_;
    size_t offset_ = 0;
    size_t length_ = 0;
};

} // namespace net
} // namespace gs
