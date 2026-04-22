package main

import (
	"flag"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/mysql"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/server"
)

func main() {
	var (
		listenAddr = flag.String("listen", ":3307", "DBProxy listen address")
		mysqlDSNs  = flag.String("mysql", "", "MySQL DSNs, comma separated")
		logFile    = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel   = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	log := logger.InitServiceLogger("dbproxy", "dbproxy-1", *logLevel, *logFile)

	// MySQL
	var mproxy *mysql.Proxy
	if *mysqlDSNs != "" {
		var cfgs []mysql.Config
		for _, dsn := range strings.Split(*mysqlDSNs, ",") {
			cfgs = append(cfgs, mysql.Config{
				DSN:         dsn,
				MaxOpenConn: 20,
				MaxIdleConn: 5,
			})
		}
		var err error
		mproxy, err = mysql.NewProxy(cfgs)
		if err != nil {
			log.Fatalf("mysql proxy init failed: %v", err)
		}
		defer mproxy.Close()
	}

	srv := server.New(*listenAddr, mproxy)
	if err := srv.Start(); err != nil {
		log.Fatalf("start dbproxy failed: %v", err)
	}
	log.Infof("DBProxy started on %s", *listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down dbproxy...")
	srv.Stop()
}
