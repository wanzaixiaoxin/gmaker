#pragma once

#include "iconnection.hpp"

namespace gs {
namespace net {

struct Packet;

// Middleware 中间件接口，返回 false 表示拦截该包
class Middleware {
public:
    virtual ~Middleware() = default;
    virtual bool OnPacket(IConnection* conn, Packet& pkt) = 0;
};

} // namespace net
} // namespace gs
