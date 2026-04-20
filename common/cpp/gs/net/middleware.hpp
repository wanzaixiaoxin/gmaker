#pragma once

namespace gs {
namespace net {

class TCPConn;
struct Packet;

// Middleware 中间件接口，返回 false 表示拦截该包
class Middleware {
public:
    virtual ~Middleware() = default;
    virtual bool OnPacket(TCPConn* conn, Packet& pkt) = 0;
};

} // namespace net
} // namespace gs
