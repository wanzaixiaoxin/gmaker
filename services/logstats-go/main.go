package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"net"
	"net/http"
	"strings"
	"sync"

	"github.com/gmaker/game-server/common/go/logger"
)

// LogEntry 单条结构化日志
type LogEntry struct {
	Time      string                 `json:"time"`
	Level     string                 `json:"level"`
	Service   string                 `json:"service"`
	NodeID    string                 `json:"node_id"`
	TraceID   string                 `json:"trace_id,omitempty"`
	Msg       string                 `json:"msg"`
	Extra     map[string]interface{} `json:"-"`
}

// LogStats 日志收集与检索服务
type LogStats struct {
	mu       sync.RWMutex
	entries  []LogEntry
	byTrace  map[string][]int // trace_id -> indices in entries
	maxSize  int
}

func NewLogStats(maxSize int) *LogStats {
	return &LogStats{
		entries: make([]LogEntry, 0, maxSize),
		byTrace: make(map[string][]int),
		maxSize: maxSize,
	}
}

func (ls *LogStats) Append(raw []byte) {
	var entry LogEntry
	if err := json.Unmarshal(raw, &entry); err != nil {
		return
	}
	// 将未知字段也放到 Extra
	var full map[string]interface{}
	json.Unmarshal(raw, &full)
	entry.Extra = full

	ls.mu.Lock()
	defer ls.mu.Unlock()

	idx := len(ls.entries)
	ls.entries = append(ls.entries, entry)
	if entry.TraceID != "" {
		ls.byTrace[entry.TraceID] = append(ls.byTrace[entry.TraceID], idx)
	}

	// 环形缓冲：超限时淘汰旧数据（简化：直接清空，生产环境用更精细策略）
	if len(ls.entries) > ls.maxSize {
		ls.entries = ls.entries[ls.maxSize/2:]
		ls.rebuildIndex()
	}
}

func (ls *LogStats) rebuildIndex() {
	ls.byTrace = make(map[string][]int)
	for i, e := range ls.entries {
		if e.TraceID != "" {
			ls.byTrace[e.TraceID] = append(ls.byTrace[e.TraceID], i)
		}
	}
}

func (ls *LogStats) QueryByTrace(traceID string) []LogEntry {
	ls.mu.RLock()
	defer ls.mu.RUnlock()
	indices, ok := ls.byTrace[traceID]
	if !ok {
		return nil
	}
	out := make([]LogEntry, 0, len(indices))
	for _, idx := range indices {
		if idx < len(ls.entries) {
			out = append(out, ls.entries[idx])
		}
	}
	return out
}

func (ls *LogStats) Recent(n int) []LogEntry {
	ls.mu.RLock()
	defer ls.mu.RUnlock()
	if n > len(ls.entries) {
		n = len(ls.entries)
	}
	out := make([]LogEntry, n)
	copy(out, ls.entries[len(ls.entries)-n:])
	return out
}

func (ls *LogStats) Stats() map[string]interface{} {
	ls.mu.RLock()
	defer ls.mu.RUnlock()
	levels := make(map[string]int)
	services := make(map[string]int)
	for _, e := range ls.entries {
		levels[e.Level]++
		services[e.Service]++
	}
	return map[string]interface{}{
		"total_entries": len(ls.entries),
		"by_level":      levels,
		"by_service":    services,
	}
}

func main() {
	var (
		listenAddr = flag.String("listen", ":8085", "TCP log ingest address")
		httpAddr   = flag.String("http", ":8086", "HTTP query address")
		maxSize    = flag.Int("max-size", 100000, "Max in-memory log entries")
		logFile    = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel   = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	log := logger.InitServiceLogger("logstats", "logstats-1", *logLevel, *logFile)

	ls := NewLogStats(*maxSize)

	// TCP 日志接收
	tcpListener, err := net.Listen("tcp", *listenAddr)
	if err != nil {
		log.Fatalf("listen tcp failed: %v", err)
	}
	defer tcpListener.Close()
	log.Infof("LogStats TCP ingest on %s", *listenAddr)

	go func() {
		for {
			conn, err := tcpListener.Accept()
			if err != nil {
				continue
			}
			go handleTCPConn(conn, ls)
		}
	}()

	// HTTP 查询接口
	mux := http.NewServeMux()
	mux.HandleFunc("/query/trace", func(w http.ResponseWriter, r *http.Request) {
		traceID := r.URL.Query().Get("trace_id")
		if traceID == "" {
			http.Error(w, "missing trace_id", http.StatusBadRequest)
			return
		}
		entries := ls.QueryByTrace(traceID)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(entries)
	})
	mux.HandleFunc("/query/recent", func(w http.ResponseWriter, r *http.Request) {
		n := 100
		fmt.Sscanf(r.URL.Query().Get("n"), "%d", &n)
		entries := ls.Recent(n)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(entries)
	})
	mux.HandleFunc("/stats", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(ls.Stats())
	})
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"ok":true}`))
	})

	log.Infof("LogStats HTTP query on %s", *httpAddr)
	if err := http.ListenAndServe(*httpAddr, mux); err != nil {
		log.Fatalf("http server failed: %v", err)
	}
}

func handleTCPConn(conn net.Conn, ls *LogStats) {
	defer conn.Close()
	reader := bufio.NewReader(conn)
	for {
		line, err := reader.ReadBytes('\n')
		if err != nil {
			return
		}
		line = []byte(strings.TrimSpace(string(line)))
		if len(line) > 0 {
			ls.Append(line)
		}
	}
}
