FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake libprotobuf-dev protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target realtime-cpp

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libprotobuf23 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /build/build/realtime-cpp .
EXPOSE 8084 9090
ENTRYPOINT ["./realtime-cpp"]
