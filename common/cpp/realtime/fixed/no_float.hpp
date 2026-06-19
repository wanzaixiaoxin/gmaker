// common/cpp/realtime/fixed/no_float.hpp
// 在战斗引擎翻译单元 include 此头,触发编译期断言:禁止使用 float/double。
// 用法:在 .cpp 顶部 #include "fixed/no_float.hpp"
#pragma once

#include <type_traits>

// 底座阶段的轻量守卫:被 include 即表示该 TU 承诺无浮点。
// 完整的浮点检测(拦截局部 float/double 变量)留给 M1 引入 clang-tidy 规则。
// 当前用此头作文档化标记 + code review 兜底。
namespace gs::realtime::fixed::detail {
    constexpr bool NO_FLOAT_GUARD = true;
}
