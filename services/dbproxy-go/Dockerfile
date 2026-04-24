FROM golang:1.20-alpine AS builder
WORKDIR /build
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o dbproxy-go ./services/dbproxy-go

FROM alpine:latest
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /build/dbproxy-go .
COPY --from=builder /build/services/dbproxy-go/dbproxy.json .
EXPOSE 3307
ENTRYPOINT ["./dbproxy-go"]
