FROM golang:1.20-alpine AS builder
WORKDIR /build
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o biz-go ./services/biz-go

FROM alpine:latest
RUN apk --no-cache add ca-certificates
WORKDIR /app
COPY --from=builder /build/biz-go .
COPY --from=builder /build/services/biz-go/biz.json ./conf/biz.json
EXPOSE 8082 9082
ENTRYPOINT ["./biz-go"]
