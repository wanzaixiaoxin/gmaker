package main

import (
	"database/sql"
	"fmt"
	"os"
	"strings"

	_ "github.com/go-sql-driver/mysql"
)

func main() {
	adminDSN := "root:123456@tcp(192.168.0.85:3306)/"
	db, err := sql.Open("mysql", adminDSN)
	if err != nil {
		fmt.Println("MySQL open fail:", err)
		os.Exit(1)
	}
	defer db.Close()

	if err := db.Ping(); err != nil {
		fmt.Println("MySQL ping fail:", err)
		os.Exit(1)
	}

	// Read SQL file
	sqlBytes, err := os.ReadFile("scripts/init-db.sql")
	if err != nil {
		fmt.Println("Read SQL file fail:", err)
		os.Exit(1)
	}

	// Execute statements
	statements := strings.Split(string(sqlBytes), ";")
	for _, stmt := range statements {
		stmt = strings.TrimSpace(stmt)
		if stmt == "" {
			continue
		}
		if _, err := db.Exec(stmt); err != nil {
			fmt.Println("Exec fail:", err)
			fmt.Println("Statement:", stmt)
			os.Exit(1)
		}
		fmt.Println("OK:", stmt[:min(60, len(stmt))])
	}

	fmt.Println("Database initialized successfully.")
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
