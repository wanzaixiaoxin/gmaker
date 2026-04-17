.PHONY: proto build clean test

# ==================== 变量定义 ====================
PROTO_DIR      := spec/proto
GEN_GO_DIR     := gen/go
GEN_CPP_DIR    := gen/cpp
GO_SERVICES    := services/registry-go
CPP_SERVICES   := services/gateway-cpp services/realtime-cpp
COMMON_GO      := common/go
COMMON_CPP     := common/cpp/gs

# ==================== Protobuf 生成 ====================
proto:
	@echo "Generating protobuf code..."
	@mkdir -p $(GEN_GO_DIR) $(GEN_CPP_DIR)
	protoc \
		--proto_path=$(PROTO_DIR) \
		--go_out=. --go_opt=module=github.com/gmaker/game-server \
		--go-grpc_out=. --go-grpc_opt=module=github.com/gmaker/game-server \
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
	@echo "Go build done."

build-cpp:
	@echo "Building C++ services..."
	@mkdir -p build
	cd build && cmake --build . --config Release
	@echo "C++ build done."

# ==================== 测试 ====================
test:
	@echo "Running Go tests..."
	cd $(COMMON_GO)/net && go test -v ./...
	@echo "Go tests done."
	@echo "C++ tests require compiled binaries and GTest. Skipped in skeleton phase."

# ==================== 清理 ====================
clean:
	rm -rf $(GEN_GO_DIR) $(GEN_CPP_DIR) build bin
	@echo "Clean done."
