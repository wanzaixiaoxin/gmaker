package main

import (
	"database/sql"
	"fmt"

	_ "github.com/go-sql-driver/mysql"
)

func main() {
	dsn := "root:123456@tcp(192.168.0.85:3306)/gmaker?charset=utf8mb4"
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		fmt.Println("MySQL FAIL (open):", err)
		return
	}
	defer db.Close()

	if err := db.Ping(); err != nil {
		fmt.Println("MySQL FAIL (ping):", err)
		return
	}

	var v int
	if err := db.QueryRow("SELECT 1").Scan(&v); err != nil {
		fmt.Println("MySQL FAIL (query):", err)
		return
	}

	fmt.Println("MySQL OK: SELECT 1 =", v)
}
