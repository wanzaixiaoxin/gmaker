package metrics

import (
	"fmt"
	"net/http"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

// ==================== Counter ====================

// Counter 单调递增计数器
type Counter struct {
	val atomic.Int64
}

func NewCounter() *Counter { return &Counter{} }
func (c *Counter) Inc()    { c.val.Add(1) }
func (c *Counter) Add(n int64) { c.val.Add(n) }
func (c *Counter) Value() int64 { return c.val.Load() }

// ==================== Gauge ====================

// Gauge 可增可减的瞬时指标
type Gauge struct {
	val atomic.Int64
}

func NewGauge() *Gauge        { return &Gauge{} }
func (g *Gauge) Set(n int64)  { g.val.Store(n) }
func (g *Gauge) Inc()         { g.val.Add(1) }
func (g *Gauge) Dec()         { g.val.Add(-1) }
func (g *Gauge) Value() int64 { return g.val.Load() }

// ==================== Histogram ====================

// Histogram 延迟/大小分布直方图（简化版：只记录 sum/count/buckets）
type Histogram struct {
	mu       sync.RWMutex
	buckets  []int64 // 上界（毫秒），如 [1,5,10,25,50,100,250,500,1000]
	counts   []int64
	sum      atomic.Int64
	count    atomic.Int64
}

// NewHistogram 创建直方图，buckets 为毫秒上界，必须有序递增
func NewHistogram(buckets []int64) *Histogram {
	return &Histogram{
		buckets: append([]int64{}, buckets...),
		counts:  make([]int64, len(buckets)),
	}
}

// Observe 记录一次观测值（duration 为毫秒）
func (h *Histogram) Observe(ms float64) {
	v := int64(ms)
	h.sum.Add(v)
	h.count.Add(1)

	h.mu.Lock()
	for i, b := range h.buckets {
		if v <= b {
			h.counts[i]++
		}
	}
	h.mu.Unlock()
}

// Snapshot 返回当前统计快照
func (h *Histogram) Snapshot() (sum, count int64, counts []int64, buckets []int64) {
	sum = h.sum.Load()
	count = h.count.Load()
	h.mu.RLock()
	counts = append([]int64{}, h.counts...)
	buckets = append([]int64{}, h.buckets...)
	h.mu.RUnlock()
	return
}

// ==================== Registry ====================

// Registry 指标注册中心（线程安全）
type Registry struct {
	mu       sync.RWMutex
	counters map[string]*Counter
	gauges   map[string]*Gauge
	hists    map[string]*Histogram
}

func NewRegistry() *Registry {
	return &Registry{
		counters: make(map[string]*Counter),
		gauges:   make(map[string]*Gauge),
		hists:    make(map[string]*Histogram),
	}
}

func (r *Registry) Counter(name string) *Counter {
	r.mu.Lock()
	defer r.mu.Unlock()
	if c, ok := r.counters[name]; ok {
		return c
	}
	c := NewCounter()
	r.counters[name] = c
	return c
}

func (r *Registry) Gauge(name string) *Gauge {
	r.mu.Lock()
	defer r.mu.Unlock()
	if g, ok := r.gauges[name]; ok {
		return g
	}
	g := NewGauge()
	r.gauges[name] = g
	return g
}

func (r *Registry) Histogram(name string, buckets []int64) *Histogram {
	r.mu.Lock()
	defer r.mu.Unlock()
	if h, ok := r.hists[name]; ok {
		return h
	}
	h := NewHistogram(buckets)
	r.hists[name] = h
	return h
}

// PrometheusText 输出 Prometheus 文本格式
func (r *Registry) PrometheusText() string {
	var b strings.Builder

	r.mu.RLock()
	counters := make(map[string]*Counter, len(r.counters))
	for k, v := range r.counters { counters[k] = v }
	gauges := make(map[string]*Gauge, len(r.gauges))
	for k, v := range r.gauges { gauges[k] = v }
	hists := make(map[string]*Histogram, len(r.hists))
	for k, v := range r.hists { hists[k] = v }
	r.mu.RUnlock()

	// counters
	var names []string
	for name := range counters { names = append(names, name) }
	sort.Strings(names)
	for _, name := range names {
		fmt.Fprintf(&b, "# TYPE %s counter\n", name)
		fmt.Fprintf(&b, "%s %d\n", name, counters[name].Value())
	}

	// gauges
	names = nil
	for name := range gauges { names = append(names, name) }
	sort.Strings(names)
	for _, name := range names {
		fmt.Fprintf(&b, "# TYPE %s gauge\n", name)
		fmt.Fprintf(&b, "%s %d\n", name, gauges[name].Value())
	}

	// histograms
	names = nil
	for name := range hists { names = append(names, name) }
	sort.Strings(names)
	for _, name := range names {
		sum, count, counts, buckets := hists[name].Snapshot()
		fmt.Fprintf(&b, "# TYPE %s histogram\n", name)
		for i, bc := range buckets {
			fmt.Fprintf(&b, "%s_bucket{le=\"%d\"} %d\n", name, bc, counts[i])
		}
		fmt.Fprintf(&b, "%s_bucket{le=\"+Inf\"} %d\n", name, count)
		fmt.Fprintf(&b, "%s_sum %d\n", name, sum)
		fmt.Fprintf(&b, "%s_count %d\n", name, count)
	}

	return b.String()
}

// ServeHTTP 启动 /metrics HTTP 服务（端口隔离）
func ServeHTTP(addr string, reg *Registry) {
	mux := http.NewServeMux()
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.Write([]byte(reg.PrometheusText()))
	})
	go func() {
		if err := http.ListenAndServe(addr, mux); err != nil {
			fmt.Printf("[metrics] http server error: %v\n", err)
		}
	}()
}

// DefaultRegistry 全局默认注册中心
var DefaultRegistry = NewRegistry()

func DefaultCounter(name string) *Counter    { return DefaultRegistry.Counter(name) }
func DefaultGauge(name string) *Gauge       { return DefaultRegistry.Gauge(name) }
func DefaultHistogram(name string, buckets []int64) *Histogram {
	return DefaultRegistry.Histogram(name, buckets)
}
func ServeDefaultHTTP(addr string) { ServeHTTP(addr, DefaultRegistry) }
