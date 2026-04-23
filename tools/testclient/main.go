package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"
)

func main() {
	var (
		addr       = flag.String("addr", "127.0.0.1:8081", "Gateway address")
		bots       = flag.Int("bots", 1, "Number of concurrent bots")
		scenario   = flag.String("scenario", "login", "Test scenario: login | heartbeat | flood")
		duration   = flag.Duration("duration", 30*time.Second, "Test duration")
		rate       = flag.Int("rate", 0, "Requests per second per bot (0=unlimited)")
		masterKey  = flag.String("master-key", "", "Handshake master key (default: 32 zero bytes)")
		output     = flag.String("output", "text", "Output format: text | json")
		reportPath = flag.String("report", "", "Final report output path")
		interval   = flag.Duration("interval", 1*time.Second, "Stats report interval")
	)
	flag.Parse()

	// 解析 master key
	mk := make([]byte, 32)
	if *masterKey != "" {
		copy(mk, []byte(*masterKey))
	}

	// 选择场景
	var sc Scenario
	switch *scenario {
	case "login":
		sc = &LoginScenario{}
	case "heartbeat":
		sc = &HeartbeatScenario{Interval: 5 * time.Second}
	case "flood":
		sc = &FloodScenario{Rate: *rate}
	case "chat":
		sc = &ChatScenario{}
	default:
		fmt.Fprintf(os.Stderr, "Unknown scenario: %s\n", *scenario)
		os.Exit(1)
	}

	reporter := NewReporter(*output, *reportPath)
	reporter.PrintHeader(*addr, *bots, sc.Name(), *duration)

	stats := NewGlobalStats()
	stopCh := make(chan struct{})
	var wg sync.WaitGroup

	// 连接所有 Bot
	botList := make([]*Bot, 0, *bots)
	for i := 0; i < *bots; i++ {
		bot := NewBot(i, *addr, mk)
		if err := bot.Connect(); err != nil {
			fmt.Fprintf(os.Stderr, "Bot %d connect failed: %v\n", i, err)
			continue
		}
		stats.ActiveConns.Add(1)
		botList = append(botList, bot)
	}

	if len(botList) == 0 {
		fmt.Fprintf(os.Stderr, "No bots connected, exiting\n")
		os.Exit(1)
	}

	// 启动场景 goroutine
	for _, bot := range botList {
		wg.Add(1)
		go func(b *Bot) {
			defer wg.Done()
			defer stats.ActiveConns.Add(-1)
			if err := sc.Run(b, stats, stopCh); err != nil {
				if *output == "text" {
					fmt.Fprintf(os.Stderr, "Bot %d error: %v\n", b.id, err)
				}
			}
			// 场景完成后保持连接活跃，直到 stopCh 关闭
			<-stopCh
		}(bot)
	}

	// 定时报告
	doneCh := make(chan struct{})
	go func() {
		ticker := time.NewTicker(*interval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				reporter.PrintSnapshot(stats.Snapshot())
			case <-doneCh:
				return
			}
		}
	}()

	// 信号处理和超时控制
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	if *duration > 0 {
		select {
		case <-time.After(*duration):
			// 正常结束
		case <-sigCh:
			if *output == "text" {
				fmt.Println("\nReceived signal, stopping...")
			}
		}
	} else {
		// duration == 0 表示永久运行（守护模式）
		<-sigCh
		if *output == "text" {
			fmt.Println("\nReceived signal, stopping...")
		}
	}

	close(stopCh)
	close(doneCh)
	wg.Wait()

	// 关闭所有连接
	for _, bot := range botList {
		bot.Close()
	}

	reporter.PrintFinal(stats)
}
