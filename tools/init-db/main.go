package main

import (
	"database/sql"
	"fmt"
	"os"
	"strings"

	_ "github.com/go-sql-driver/mysql"
)

func main() {
	dsn := "root:123456@tcp(192.168.0.85:3306)/gmaker?charset=utf8mb4&multiStatements=true"
	if len(os.Args) > 1 {
		dsn = os.Args[1]
	}

	db, err := sql.Open("mysql", dsn)
	if err != nil {
		fmt.Fprintf(os.Stderr, "open db failed: %v\n", err)
		os.Exit(1)
	}
	defer db.Close()

	if err := db.Ping(); err != nil {
		fmt.Fprintf(os.Stderr, "ping db failed: %v\n", err)
		os.Exit(1)
	}
	fmt.Println("[init-db] Connected to MySQL")

	data, err := os.ReadFile("scripts/init-db.sql")
	if err != nil {
		fmt.Fprintf(os.Stderr, "read init-db.sql failed: %v\n", err)
		os.Exit(1)
	}

	// 分割并执行每条 SQL（去除注释和空行）
	statements := splitStatements(string(data))
	for _, stmt := range statements {
		stmt = strings.TrimSpace(stmt)
		if stmt == "" {
			continue
		}
		if _, err := db.Exec(stmt); err != nil {
			// 忽略表已存在的错误
			if strings.Contains(err.Error(), "already exists") {
				fmt.Printf("[init-db] SKIP (already exists): %s...\n", truncate(stmt, 50))
				continue
			}
			fmt.Fprintf(os.Stderr, "[init-db] EXEC FAILED: %v\n  SQL: %s\n", err, truncate(stmt, 200))
			continue
		}
		fmt.Printf("[init-db] OK: %s...\n", truncate(stmt, 50))
	}
	fmt.Println("[init-db] Done")
}

func splitStatements(sql string) []string {
	var result []string
	var current strings.Builder
	lines := strings.Split(sql, "\n")
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "--") || strings.HasPrefix(trimmed, "#") || trimmed == "" {
			continue
		}
		current.WriteString(line)
		current.WriteString("\n")
		if strings.HasSuffix(trimmed, ";") {
			result = append(result, current.String())
			current.Reset()
		}
	}
	if current.Len() > 0 {
		result = append(result, current.String())
	}
	return result
}

func truncate(s string, max int) string {
	if len(s) <= max {
		return s
	}
	return s[:max]
}
