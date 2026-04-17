package errors

import (
	"fmt"
)

// Error 带错误码的结构化错误，实现 error 接口
type Error struct {
	Code    int32
	Message string
	Cause   error
}

func (e *Error) Error() string {
	if e.Cause != nil {
		return fmt.Sprintf("[%d] %s: %v", e.Code, e.Message, e.Cause)
	}
	return fmt.Sprintf("[%d] %s", e.Code, e.Message)
}

func (e *Error) Unwrap() error {
	return e.Cause
}

// New 创建一个新的结构化错误
func New(code int32, message string) *Error {
	return &Error{Code: code, Message: message}
}

// Wrap 包装一个已有错误，附加错误码
func Wrap(cause error, code int32) *Error {
	return &Error{Code: code, Message: cause.Error(), Cause: cause}
}

// WrapWithMsg 包装一个已有错误，附加错误码和自定义消息
func WrapWithMsg(cause error, code int32, message string) *Error {
	return &Error{Code: code, Message: message, Cause: cause}
}

// Is 判断错误码是否匹配
func Is(err error, code int32) bool {
	if e, ok := err.(*Error); ok {
		return e.Code == code
	}
	return false
}

// FromCode 根据错误码创建错误（使用预定义的消息）
func FromCode(code int32) *Error {
	return &Error{Code: code, Message: CodeName(code)}
}
