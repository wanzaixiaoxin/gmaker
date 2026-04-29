.PHONY: proto build clean test

# ==================== 变量定义 ====================
PROTO_DIR      := spec/proto
GEN_GO_DIR     := gen/go
GEN_CPP_DIR    := gen/cpp
GO_SERVICES    := services/registry-go
CPP_SERVICES   := services/gateway-cpp services/realtime-cpp
COMMON_GO      := common/go
COMMON_CPP     := common/cpp
# 优先使用系统 protoc，否则回退到本地预编译路径
PROTOC         ?= $(shell command -v protoc 2>/dev/null || echo 3rd/protobuf/protobuf-34.1/build/Release/protoc.exe)

# ==================== Protobuf 生成 ====================
proto:
	@echo "Generating protobuf code..."
	@mkdir -p $(GEN_GO_DIR) $(GEN_CPP_DIR)
	$(PROTOC) \
		--proto_path=$(PROTO_DIR) \
		--go_out=. --go_opt=module=github.com/gmaker/luffa \
		--go-grpc_out=. --go-grpc_opt=module=github.com/gmaker/luffa \
		--cpp_out=$(GEN_CPP_DIR) \
		$(PROTO_DIR)/*.proto
	@echo "Protobuf generation done."

# ==================== 构建 ====================
build: build-go build-cpp

build-go:
	@echo "Building Go services..."
	cd services/registry-go && go mod tidy && go build -o ../../bin/registry-go.exe ./main.go
	cd services/dbproxy-go && go mod tidy && go build -o ../../bin/dbproxy-go.exe ./main.go
	cd services/biz-go && go mod tidy && go build -o ../../bin/biz-go.exe ./main.go
	cd services/logstats-go && go mod tidy && go build -o ../../bin/logstats-go.exe ./main.go
	cd services/chat-go && go mod tidy && go build -o ../../bin/chat-go.exe ./main.go
	@echo "Go build done."

build-cpp:
	@echo "Building C++ services..."
	@mkdir -p build
ifeq ($(OS),Windows_NT)
	cmd /C "cd /d build && cmake --build . --config Release"
else
	cd build && cmake --build . --config Release
endif
	@echo "C++ build done."

# ==================== 测试 ====================
test:
	@echo "Running Go tests..."
	cd $(COMMON_GO)/net && go test -v ./...
	cd $(COMMON_GO)/config && go test -v ./...
	@echo "Go tests done."
	@echo "C++ tests require compiled binaries and GTest. Skipped in skeleton phase."

# ==================== 清理 ====================
ifeq ($(OS),Windows_NT)
clean:
	@if exist $(GEN_GO_DIR) rmdir /s /q $(GEN_GO_DIR)
	@if exist $(GEN_CPP_DIR) rmdir /s /q $(GEN_CPP_DIR)
	@if exist build rmdir /s /q build
	@if exist bin rmdir /s /q bin
	@echo "Clean done."
else
clean:
	rm -rf $(GEN_GO_DIR) $(GEN_CPP_DIR) build bin
	@echo "Clean done."
endif
