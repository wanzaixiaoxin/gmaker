package main

import (
	"encoding/json"
	"fmt"
	"os"
	"time"
)

// Reporter 负责实时输出和最终报告
type Reporter struct {
	mode       string // "text" | "json"
	reportPath string
	startTime  time.Time
}

// NewReporter 创建报告器
func NewReporter(mode, reportPath string) *Reporter {
	return &Reporter{
		mode:       mode,
		reportPath: reportPath,
		startTime:  time.Now(),
	}
}

// PrintHeader 打印测试开始信息
func (r *Reporter) PrintHeader(addr string, bots int, scenario string, duration time.Duration) {
	if r.mode == "json" {
		return
	}
	fmt.Printf("\n=== TestClient ===\n")
	fmt.Printf("Target:    %s\n", addr)
	fmt.Printf("Bots:      %d\n", bots)
	fmt.Printf("Scenario:  %s\n", scenario)
	fmt.Printf("Duration:  %s\n", duration)
	fmt.Printf("\n")
}

// PrintSnapshot 打印实时统计
func (r *Reporter) PrintSnapshot(snap StatsSnapshot) {
	if r.mode == "json" {
		return
	}
	elapsed := time.Since(r.startTime)
	fmt.Printf("[%8s] %s\n", elapsed.Truncate(time.Second), snap.PrintLine())
}

// FinalReport 最终报告结构
type FinalReport struct {
	StartTime   time.Time     `json:"start_time"`
	EndTime     time.Time     `json:"end_time"`
	Duration    time.Duration `json:"duration_ms"`
	TotalReq    uint64        `json:"total_req"`
	SuccessReq  uint64        `json:"success_req"`
	FailReq     uint64        `json:"fail_req"`
	SuccessRate float64       `json:"success_rate"`
	QPS         float64       `json:"qps"`
	P50Ms       float64       `json:"p50_ms"`
	P99Ms       float64       `json:"p99_ms"`
	AvgMs       float64       `json:"avg_ms"`
}

// PrintFinal 打印/输出最终报告
func (r *Reporter) PrintFinal(stats *GlobalStats) {
	snap := stats.Snapshot()
	elapsed := time.Since(r.startTime)

	report := FinalReport{
		StartTime:   r.startTime,
		EndTime:     time.Now(),
		Duration:    elapsed,
		TotalReq:    snap.Total,
		SuccessReq:  snap.Success,
		FailReq:     snap.Fail,
		SuccessRate: snap.SuccessRate(),
		QPS:         snap.ReqPerSec(elapsed),
		P50Ms:       float64(snap.P50.Microseconds()) / 1000.0,
		P99Ms:       float64(snap.P99.Microseconds()) / 1000.0,
		AvgMs:       float64(snap.Avg.Microseconds()) / 1000.0,
	}

	if r.mode != "json" {
		fmt.Printf("\n=== Final Report ===\n")
		fmt.Printf("Duration:  %s\n", elapsed.Truncate(time.Millisecond))
		fmt.Printf("Total:     %d\n", report.TotalReq)
		fmt.Printf("Success:   %d (%.2f%%)\n", report.SuccessReq, report.SuccessRate)
		fmt.Printf("Fail:      %d\n", report.FailReq)
		fmt.Printf("QPS:       %.2f\n", report.QPS)
		fmt.Printf("P50:       %.2f ms\n", report.P50Ms)
		fmt.Printf("P99:       %.2f ms\n", report.P99Ms)
		fmt.Printf("Avg:       %.2f ms\n", report.AvgMs)
		fmt.Printf("\n")
	}

	if r.reportPath != "" {
		data, _ := json.MarshalIndent(report, "", "  ")
		_ = os.WriteFile(r.reportPath, data, 0644)
		if r.mode != "json" {
			fmt.Printf("Report saved to: %s\n", r.reportPath)
		}
	}

	if r.mode == "json" {
		data, _ := json.Marshal(report)
		fmt.Println(string(data))
	}
}
