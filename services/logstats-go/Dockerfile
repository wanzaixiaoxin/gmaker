FROM golang:1.20-alpine AS builder
WORKDIR /build
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o logstats-go ./services/logstats-go

FROM alpine:latest
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /build/logstats-go .
EXPOSE 8085 8086
ENTRYPOINT ["./logstats-go"]
