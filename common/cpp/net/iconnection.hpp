#pragma once

#include <cstdint>
#include <vector>

namespace gs {
namespace net {

struct Packet;
class Buffer;

// IConnection 抽象连接接口
// 用于解耦 Middleware 与具体连接实现（阻塞模型 / libuv 异步模型）
class IConnection {
public:
    virtual ~IConnection() = default;

    // 获取连接唯一标识
    virtual uint64_t ID() const = 0;

    // 关闭连接
    virtual void Close() = 0;

    // 发送一个 Packet（若启用加密则自动处理）
    virtual bool SendPacket(const Packet& pkt) = 0;

    // 发送原始字节流（用于写聚合器等底层优化场景）
    virtual bool Send(std::vector<uint8_t> data) = 0;

    // 发送 Buffer（零拷贝共享场景）
    virtual bool Send(const Buffer& data) = 0;

    // 批量发送 Buffer（减少跨线程 Post 次数）
    virtual bool SendBatch(const std::vector<Buffer>& buffers) = 0;
};

} // namespace net
} // namespace gs
