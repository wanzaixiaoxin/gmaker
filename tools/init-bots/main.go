package main

import (
	"database/sql"
	"flag"
	"fmt"
	"os"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	_ "github.com/go-sql-driver/mysql"
)

func main() {
	var (
		mysqlAddr = flag.String("mysql", "root:123456@tcp(192.168.0.85:3306)/gmaker?charset=utf8mb4", "MySQL DSN")
		count     = flag.Int("count", 10, "Number of bots to create")
		nodeID    = flag.Int("node", 999, "Snowflake node ID for bot player_id generation")
		prefix    = flag.String("prefix", "Bot", "Nickname prefix")
		botType   = flag.String("type", "chatbot", "Bot type")
	)
	flag.Parse()

	fmt.Printf("[InitBots] Connecting to MySQL\n")
	db, err := sql.Open("mysql", *mysqlAddr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "open db failed: %v\n", err)
		os.Exit(1)
	}
	defer db.Close()
	if err := db.Ping(); err != nil {
		fmt.Fprintf(os.Stderr, "ping db failed: %v\n", err)
		os.Exit(1)
	}

	idGen, err := idgen.NewSnowflake(int64(*nodeID))
	if err != nil {
		fmt.Fprintf(os.Stderr, "init snowflake failed: %v\n", err)
		os.Exit(1)
	}

	now := uint64(time.Now().Unix())
	success := 0

	for i := 0; i < *count; i++ {
		rawID, err := idGen.NextID()
		if err != nil {
			fmt.Fprintf(os.Stderr, "generate player_id failed: %v\n", err)
			continue
		}
		playerID := uint64(rawID)
		account := fmt.Sprintf("bot_%d", playerID)
		nickname := fmt.Sprintf("%s%d", *prefix, i+1)

		if err := insertAccount(db, playerID, account, now); err != nil {
			fmt.Fprintf(os.Stderr, "insert account %d failed: %v\n", playerID, err)
			continue
		}
		if err := insertProfile(db, playerID, nickname, now); err != nil {
			fmt.Fprintf(os.Stderr, "insert profile %d failed: %v\n", playerID, err)
			continue
		}
		if err := insertBotAccount(db, playerID, *botType, now); err != nil {
			fmt.Fprintf(os.Stderr, "insert bot_account %d failed: %v\n", playerID, err)
			continue
		}

		fmt.Printf("[InitBots] Created bot-%d: player_id=%d account=%s nickname=%s\n", i+1, playerID, account, nickname)
		success++
	}

	fmt.Printf("[InitBots] Done. Success=%d Failed=%d\n", success, *count-success)
}

func insertAccount(db *sql.DB, playerID uint64, account string, now uint64) error {
	_, err := db.Exec(
		"INSERT INTO accounts (player_id, account, password, status, create_at) VALUES (?, ?, '', 0, ?)",
		playerID, account, now,
	)
	return err
}

func insertProfile(db *sql.DB, playerID uint64, nickname string, now uint64) error {
	_, err := db.Exec(
		"INSERT INTO player_profiles (player_id, nickname, level, exp, coin, diamond, is_bot, create_at, login_at) VALUES (?, ?, 1, 0, 0, 0, 1, ?, ?)",
		playerID, nickname, now, now,
	)
	return err
}

func insertBotAccount(db *sql.DB, playerID uint64, botType string, now uint64) error {
	_, err := db.Exec(
		"INSERT INTO bot_accounts (player_id, bot_type, status, create_at) VALUES (?, ?, 0, ?)",
		playerID, botType, now,
	)
	return err
}
