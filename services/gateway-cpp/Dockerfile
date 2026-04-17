FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake libprotobuf-dev protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target gateway-cpp

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libprotobuf23 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /build/build/gateway-cpp .
EXPOSE 8081 9081
ENTRYPOINT ["./gateway-cpp"]
