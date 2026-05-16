// Package main implements xlsx2all: a code generator that reads Excel table
// definitions and produces SQL DDL, Protobuf .proto files.
//
// Usage:
//
//	xlsx2all                          # Process all tables/*.xlsx
//	xlsx2all --init player_profiles   # Create a template Excel
//	xlsx2all --table player_profiles  # Process single table only
//	xlsx2all --demo                   # Create demo Excel files for existing tables
//	xlsx2all --clean                  # Remove all generated SQL/Proto/Go files
//
// Excel format (one .xlsx per table, filename = table name):
//
//	Row 1: Headers → 字段名 | 数据类型 | 长度 | 无符号 | 默认值 | 非空 | 主键 | 自增 | Proto类型 | 注释
//	Row 2+: Data rows
//	Metadata rows (first column starts with #):
//	  #表注释: 玩家基础信息表
//	  #引擎: InnoDB
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"unicode"

	"github.com/xuri/excelize/v2"
)

// ──────────────────────────────────────────────
// Data Structures
// ──────────────────────────────────────────────

const (
	colField    = 0 // A: 字段名
	colDBType   = 1 // B: 数据类型
	colLength   = 2 // C: 长度
	colUnsigned = 3 // D: 无符号
	colDefault  = 4 // E: 默认值
	colNotNull  = 5 // F: 非空
	colPK       = 6 // G: 主键
	colAutoInc  = 7 // H: 自增
	colProto    = 8 // I: Proto类型
	colComment  = 9 // J: 注释

	totalCols = 10
)

// Column represents a single database column definition.
type Column struct {
	FieldName  string
	DBType     string // MySQL type: int, bigint, varchar, ...
	Length     string // Length / precision
	Unsigned   bool
	DefaultVal string
	HasDefault bool
	NotNull    bool
	PrimaryKey bool
	AutoInc    bool
	ProtoType  string // Protobuf type (auto-derived if empty)
	Comment    string
}

// Table represents a complete database table definition parsed from Excel.
type Table struct {
	Name    string
	Comment string
	Engine  string
	Charset string
	Collate string
	Columns []Column
	PKs     []string // collected primary key column names
}

// ──────────────────────────────────────────────
// Main
// ──────────────────────────────────────────────

func main() {
	var (
		initTable = flag.String("init", "", "Create a template Excel for the given table name")
		demoFlag  = flag.Bool("demo", false, "Create demo Excel files for existing project tables")
		cleanFlag = flag.Bool("clean", false, "Remove all generated SQL/Proto files")
		tableDir  = flag.String("dir", "tables", "Input directory for Excel files")
		sqlDir    = flag.String("sql-out", "sql", "Output directory for SQL files")
		protoDir  = flag.String("proto-out", "spec/proto", "Output directory for proto files")
		single    = flag.String("table", "", "Process only this table")
		module    = flag.String("module", "github.com/gmaker/luffa", "Go module path for proto go_package")
		dbName    = flag.String("db", "gmaker", "Database name for combined SQL")
	)
	flag.Parse()

	switch {
	case *initTable != "":
		must(createTemplate(*initTable, *tableDir))
	case *demoFlag:
		must(createDemoFiles(*tableDir))
	case *cleanFlag:
		must(cleanGenerated(*tableDir, *sqlDir, *protoDir))
	default:
		must(processAll(*tableDir, *sqlDir, *protoDir, *single, *module, *dbName))
	}
}

func must(err error) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

// ──────────────────────────────────────────────
// Template Creation (--init)
// ──────────────────────────────────────────────

func createTemplate(name, dir string) error {
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("create directory: %w", err)
	}

	f := excelize.NewFile()
	s := "Sheet1"

	// Write header row
	headers := []string{"字段名", "数据类型", "长度", "无符号", "默认值", "非空", "主键", "自增", "Proto类型", "注释"}
	for i, h := range headers {
		cell, _ := excelize.CoordinatesToCellName(i+1, 1)
		_ = f.SetCellValue(s, cell, h)
	}

	// Header style: bold + gray background + centered + border
	styleID, _ := f.NewStyle(&excelize.Style{
		Font:      &excelize.Font{Bold: true, Color: "#333333"},
		Fill:      excelize.Fill{Type: "pattern", Pattern: 1, Color: []string{"#D9E1F2"}},
		Alignment: &excelize.Alignment{Horizontal: "center", Vertical: "center"},
		Border: []excelize.Border{
			{Type: "left", Color: "AAAAAA", Style: 1},
			{Type: "right", Color: "AAAAAA", Style: 1},
			{Type: "top", Color: "AAAAAA", Style: 1},
			{Type: "bottom", Color: "AAAAAA", Style: 1},
		},
	})
	_ = f.SetCellStyle(s, "A1", "J1", styleID)

	// Set column widths for readability
	colWidths := []struct{ col string; width float64 }{
		{"A", 20}, {"B", 14}, {"C", 8}, {"D", 8}, {"E", 14},
		{"F", 8}, {"G", 8}, {"H", 8}, {"I", 14}, {"J", 30},
	}
	for _, w := range colWidths {
		_ = f.SetColWidth(s, w.col, w.col, w.width)
	}

	// Sample data rows (typical game table columns)
	samples := [][]interface{}{
		{"id", "bigint", "20", "Y", "", "Y", "Y", "Y", "uint64", "主键ID"},
		{"name", "varchar", "64", "", "", "Y", "", "", "string", "名称"},
		{"status", "tinyint", "", "", "0", "Y", "", "", "int32", "状态: 0=正常 1=禁用"},
		{"created_at", "bigint", "", "", "0", "", "", "", "int64", "创建时间(unix)"},
		{"updated_at", "bigint", "", "", "0", "", "", "", "int64", "更新时间(unix)"},
	}
	for i, row := range samples {
		for j, val := range row {
			cell, _ := excelize.CoordinatesToCellName(j+1, i+2)
			_ = f.SetCellValue(s, cell, val)
		}
	}

	out := filepath.Join(dir, name+".xlsx")
	if err := f.SaveAs(out); err != nil {
		return fmt.Errorf("save file: %w", err)
	}
	fmt.Printf("✓ Created template: %s\n", out)
	return f.Close()
}

// ──────────────────────────────────────────────
// Demo Files (--demo)
// ──────────────────────────────────────────────

func createDemoFiles(dir string) error {
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	demos := []struct {
		name    string
		comment string
		cols    [][]interface{}
	}{
		{
			name: "accounts", comment: "账号认证表",
			cols: [][]interface{}{
				{"player_id", "bigint", "20", "Y", "", "Y", "Y", "", "uint64", "玩家ID"},
				{"account", "varchar", "64", "", "", "Y", "", "", "string", "账号"},
				{"password", "varchar", "128", "", "", "Y", "", "", "string", "密码"},
				{"status", "tinyint", "", "", "0", "", "", "", "int32", "0=正常 1=冻结 2=注销"},
				{"create_at", "bigint", "", "", "0", "", "", "", "int64", "创建时间"},
			},
		},
		{
			name: "player_profiles", comment: "用户业务资料表",
			cols: [][]interface{}{
				{"player_id", "bigint", "20", "Y", "", "Y", "Y", "", "uint64", "玩家ID"},
				{"nickname", "varchar", "64", "", "", "Y", "", "", "string", "昵称"},
				{"level", "int", "", "", "1", "", "", "", "int32", "等级"},
				{"exp", "bigint", "", "", "0", "", "", "", "int64", "经验值"},
				{"coin", "bigint", "", "", "0", "", "", "", "int64", "金币"},
				{"diamond", "bigint", "", "", "0", "", "", "", "int64", "钻石"},
				{"is_bot", "tinyint", "", "", "0", "", "", "", "int32", "0=普通用户 1=机器人"},
				{"create_at", "bigint", "", "", "0", "", "", "", "int64", "创建时间"},
				{"login_at", "bigint", "", "", "0", "", "", "", "int64", "登录时间"},
			},
		},
		{
			name: "chat_rooms", comment: "聊天房间表",
			cols: [][]interface{}{
				{"room_id", "bigint", "20", "Y", "", "Y", "Y", "", "uint64", "房间ID"},
				{"name", "varchar", "128", "", "", "Y", "", "", "string", "房间名称"},
				{"creator_id", "bigint", "20", "Y", "", "Y", "", "", "uint64", "创建者ID"},
				{"status", "tinyint", "", "", "0", "", "", "", "int32", "状态"},
				{"created_at", "bigint", "", "", "0", "", "", "", "int64", "创建时间"},
				{"closed_at", "bigint", "", "", "0", "", "", "", "int64", "关闭时间"},
			},
		},
		{
			name: "bot_accounts", comment: "机器人账号管理表",
			cols: [][]interface{}{
				{"bot_id", "int", "", "", "", "Y", "Y", "Y", "int32", "机器人ID"},
				{"player_id", "bigint", "20", "Y", "", "Y", "", "", "uint64", "关联玩家ID"},
				{"bot_type", "varchar", "32", "", "chatbot", "Y", "", "", "string", "类型: chatbot/npc/moderator"},
				{"config", "json", "", "", "", "", "", "", "string", "配置(JSON)"},
				{"status", "tinyint", "", "", "0", "", "", "", "int32", "0=启用 1=禁用"},
				{"create_at", "bigint", "", "", "0", "", "", "", "int64", "创建时间"},
			},
		},
		{
			name: "configs", comment: "配置管理中心表",
			cols: [][]interface{}{
				{"id", "bigint", "20", "Y", "", "Y", "Y", "Y", "uint64", "自增主键"},
				{"name", "varchar", "128", "", "", "Y", "", "", "string", "配置唯一标识"},
				{"namespace", "varchar", "64", "", "default", "Y", "", "", "string", "命名空间"},
				{"format", "varchar", "16", "", "json", "Y", "", "", "string", "数据格式: json/toml/yaml"},
				{"schema_def", "json", "", "", "", "", "", "", "string", "JSON Schema校验规则"},
				{"description", "varchar", "256", "", "", "", "", "", "string", "描述"},
				{"current_version", "bigint", "", "", "0", "Y", "", "", "int64", "当前生效版本号"},
				{"status", "tinyint", "", "", "0", "Y", "", "", "int32", "0=正常 1=禁用"},
				{"created_at", "bigint", "", "", "0", "Y", "", "", "int64", "创建时间"},
				{"updated_at", "bigint", "", "", "0", "Y", "", "", "int64", "更新时间"},
			},
		},
	}

	for _, d := range demos {
		if err := createDemoExcel(dir, d.name, d.comment, d.cols); err != nil {
			fmt.Fprintf(os.Stderr, "  ✗ %s: %v\n", d.name, err)
		} else {
			fmt.Printf("  ✓ %s.xlsx\n", d.name)
		}
	}

	fmt.Printf("\nCreated %d demo Excel files in %s/\n", len(demos), dir)
	return nil
}

func createDemoExcel(dir, name, comment string, cols [][]interface{}) error {
	f := excelize.NewFile()
	s := "Sheet1"

	// Header style
	styleID, _ := f.NewStyle(&excelize.Style{
		Font:      &excelize.Font{Bold: true, Color: "#333333"},
		Fill:      excelize.Fill{Type: "pattern", Pattern: 1, Color: []string{"#D9E1F2"}},
		Alignment: &excelize.Alignment{Horizontal: "center", Vertical: "center"},
		Border: []excelize.Border{
			{Type: "left", Color: "AAAAAA", Style: 1},
			{Type: "right", Color: "AAAAAA", Style: 1},
			{Type: "top", Color: "AAAAAA", Style: 1},
			{Type: "bottom", Color: "AAAAAA", Style: 1},
		},
	})

	// Comment row
	commentCell, _ := excelize.CoordinatesToCellName(1, 1)
	_ = f.SetCellValue(s, commentCell, fmt.Sprintf("#表注释: %s", comment))

	// Headers (row 2)
	headers := []string{"字段名", "数据类型", "长度", "无符号", "默认值", "非空", "主键", "自增", "Proto类型", "注释"}
	for i, h := range headers {
		cell, _ := excelize.CoordinatesToCellName(i+1, 2)
		_ = f.SetCellValue(s, cell, h)
	}
	_ = f.SetCellStyle(s, "A2", "J2", styleID)

	// Data rows (starting row 3)
	for i, row := range cols {
		for j, val := range row {
			cell, _ := excelize.CoordinatesToCellName(j+1, i+3)
			_ = f.SetCellValue(s, cell, val)
		}
	}

	// Column widths
	colWidths := []struct{ col string; width float64 }{
		{"A", 20}, {"B", 14}, {"C", 8}, {"D", 8}, {"E", 14},
		{"F", 8}, {"G", 8}, {"H", 8}, {"I", 14}, {"J", 30},
	}
	for _, w := range colWidths {
		_ = f.SetColWidth(s, w.col, w.col, w.width)
	}

	out := filepath.Join(dir, name+".xlsx")
	if err := f.SaveAs(out); err != nil {
		return err
	}
	return f.Close()
}

// ──────────────────────────────────────────────
// Processing (default mode)
// ──────────────────────────────────────────────

func processAll(tableDir, sqlDir, protoDir, single, module, dbName string) error {
	files, err := filepath.Glob(filepath.Join(tableDir, "*.xlsx"))
	if err != nil {
		return fmt.Errorf("scan directory: %w", err)
	}
	if len(files) == 0 {
		return fmt.Errorf("no .xlsx files found in %s/  (use --init or --demo to create)", tableDir)
	}

	if err := os.MkdirAll(sqlDir, 0755); err != nil {
		return err
	}
	if err := os.MkdirAll(protoDir, 0755); err != nil {
		return err
	}

	fmt.Println("Processing Excel → SQL + Proto ...")
	fmt.Println()

	var allSQL []string
	count := 0

	for _, f := range files {
		tblName := strings.TrimSuffix(filepath.Base(f), ".xlsx")
		if single != "" && tblName != single {
			continue
		}

		tbl, err := parseExcel(f)
		if err != nil {
			fmt.Fprintf(os.Stderr, "  ✗ %-30s %v\n", filepath.Base(f), err)
			continue
		}

		sql := genSQL(tbl, dbName)
		proto := genProto(tbl, module)

		sqlPath := filepath.Join(sqlDir, tblName+".sql")
		protoPath := filepath.Join(protoDir, tblName+".proto")

		if err := os.WriteFile(sqlPath, []byte(sql), 0644); err != nil {
			fmt.Fprintf(os.Stderr, "  ✗ %-30s write SQL: %v\n", filepath.Base(f), err)
			continue
		}
		if err := os.WriteFile(protoPath, []byte(proto), 0644); err != nil {
			fmt.Fprintf(os.Stderr, "  ✗ %-30s write proto: %v\n", filepath.Base(f), err)
			continue
		}

		allSQL = append(allSQL, sql)
		fmt.Printf("  ✓ %-30s → %s + %s\n", filepath.Base(f),
			filepath.Join(filepath.Base(sqlDir), tblName+".sql"),
			filepath.Join(filepath.Base(protoDir), tblName+".proto"))
		count++
	}

	// Combined all.sql
	if len(allSQL) > 0 {
		var sb strings.Builder
		sb.WriteString("-- ============================================================\n")
		sb.WriteString("-- Auto-generated by xlsx2all — DO NOT EDIT MANUALLY\n")
		sb.WriteString("-- ============================================================\n\n")
		sb.WriteString(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS %s CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n", dbName))
		sb.WriteString(fmt.Sprintf("USE %s;\n\n", dbName))
		sb.WriteString(strings.Join(allSQL, "\n"))
		combinedPath := filepath.Join(sqlDir, "all.sql")
		_ = os.WriteFile(combinedPath, []byte(sb.String()), 0644)
		fmt.Printf("\n  ✓ Combined SQL → %s\n", filepath.Join(filepath.Base(sqlDir), "all.sql"))
	}

	fmt.Printf("\nDone: %d table(s) processed\n", count)
	return nil
}

// ──────────────────────────────────────────────
// Excel Parsing
// ──────────────────────────────────────────────

func parseExcel(path string) (*Table, error) {
	f, err := excelize.OpenFile(path)
	if err != nil {
		return nil, fmt.Errorf("open file: %w", err)
	}
	defer f.Close()

	rows, err := f.GetRows(f.GetSheetName(0))
	if err != nil {
		return nil, fmt.Errorf("read rows: %w", err)
	}

	tbl := &Table{
		Name:    strings.TrimSuffix(filepath.Base(path), ".xlsx"),
		Engine:  "InnoDB",
		Charset: "utf8mb4",
		Collate: "utf8mb4_unicode_ci",
	}

	headerFound := false
	for _, row := range rows {
		if len(row) == 0 {
			continue
		}
		first := strings.TrimSpace(row[0])
		if first == "" {
			continue
		}

		// Metadata rows starting with #
		if strings.HasPrefix(first, "#") {
			parseMeta(tbl, first)
			continue
		}

		// Detect header row
		if !headerFound {
			if first == "字段名" || first == "字段" || first == "field" || first == "Field" {
				headerFound = true
			}
			continue
		}

		// Data rows (after header found)
		col := parseColumn(row)
		if col.FieldName == "" {
			continue
		}
		tbl.Columns = append(tbl.Columns, col)
		if col.PrimaryKey {
			tbl.PKs = append(tbl.PKs, col.FieldName)
		}
	}

	if !headerFound {
		return nil, fmt.Errorf("header row not found (expected '字段名' in column A)")
	}
	if len(tbl.Columns) == 0 {
		return nil, fmt.Errorf("no column definitions found")
	}

	return tbl, nil
}

func parseMeta(tbl *Table, line string) {
	line = strings.TrimSpace(line)
	// Try each prefix variant
	type metaKey struct {
		prefixes []string
		setter   func(val string)
	}
	keys := []metaKey{
		{
			prefixes: []string{"#表注释:", "#表注释：", "#注释:", "#注释：", "#comment:", "#Comment:"},
			setter:   func(v string) { tbl.Comment = v },
		},
		{
			prefixes: []string{"#引擎:", "#引擎：", "#engine:", "#Engine:"},
			setter:   func(v string) { tbl.Engine = v },
		},
		{
			prefixes: []string{"#字符集:", "#字符集：", "#charset:", "#Charset:"},
			setter:   func(v string) { tbl.Charset = v },
		},
	}
	for _, k := range keys {
		for _, p := range k.prefixes {
			if strings.HasPrefix(line, p) {
				k.setter(strings.TrimSpace(strings.TrimPrefix(line, p)))
				return
			}
		}
	}
	// Unknown # row → ignore (pure comment)
}

func parseColumn(row []string) Column {
	cell := func(i int) string {
		if i < len(row) {
			return strings.TrimSpace(row[i])
		}
		return ""
	}
	isYes := func(s string) bool {
		s = strings.ToUpper(strings.TrimSpace(s))
		return s == "Y" || s == "YES" || s == "1" || s == "TRUE"
	}

	col := Column{
		FieldName:  cell(colField),
		DBType:     strings.ToLower(cell(colDBType)),
		Length:     cell(colLength),
		Unsigned:   isYes(cell(colUnsigned)),
		NotNull:    isYes(cell(colNotNull)),
		PrimaryKey: isYes(cell(colPK)),
		AutoInc:    isYes(cell(colAutoInc)),
		ProtoType:  cell(colProto),
		Comment:    cell(colComment),
	}

	if def := cell(colDefault); def != "" {
		col.DefaultVal = def
		col.HasDefault = true
	}

	// Auto-derive proto type from DB type if not explicitly set
	if col.ProtoType == "" {
		col.ProtoType = dbTypeToProto(col.DBType, col.Unsigned)
	}

	return col
}

// ──────────────────────────────────────────────
// Type Mapping: MySQL → Protobuf
// ──────────────────────────────────────────────

func dbTypeToProto(dbType string, unsigned bool) string {
	t := strings.ToLower(strings.TrimSpace(dbType))
	// Strip length suffix: int(11) → int
	if idx := strings.Index(t, "("); idx > 0 {
		t = t[:idx]
	}

	switch t {
	case "bool", "boolean":
		return "bool"
	case "tinyint":
		if unsigned {
			return "uint32"
		}
		return "int32"
	case "smallint":
		if unsigned {
			return "uint32"
		}
		return "int32"
	case "mediumint":
		if unsigned {
			return "uint32"
		}
		return "int32"
	case "int", "integer":
		if unsigned {
			return "uint32"
		}
		return "int32"
	case "bigint":
		if unsigned {
			return "uint64"
		}
		return "int64"
	case "float":
		return "float"
	case "double", "real":
		return "double"
	case "decimal", "numeric":
		return "string" // avoid floating-point precision loss
	case "varchar", "char", "text", "tinytext", "mediumtext", "longtext":
		return "string"
	case "blob", "tinyblob", "mediumblob", "longblob", "binary", "varbinary":
		return "bytes"
	case "date", "datetime", "timestamp":
		return "int64" // unix timestamp
	case "time":
		return "int64" // duration in nanoseconds
	case "year":
		return "int32"
	case "enum", "set":
		return "string"
	case "json":
		return "string"
	default:
		return "string" // safe fallback
	}
}

// ──────────────────────────────────────────────
// SQL Generation
// ──────────────────────────────────────────────

func genSQL(tbl *Table, dbName string) string {
	var sb strings.Builder

	// Header comment
	sb.WriteString(fmt.Sprintf("-- %s", tbl.Name))
	if tbl.Comment != "" {
		sb.WriteString(fmt.Sprintf(" (%s)", tbl.Comment))
	}
	sb.WriteString("\n")

	sb.WriteString(fmt.Sprintf("CREATE TABLE IF NOT EXISTS %s (\n", tbl.Name))

	var lines []string
	for _, col := range tbl.Columns {
		lines = append(lines, "    "+buildColumnSQL(col))
	}

	// Primary key constraint
	if len(tbl.PKs) > 0 {
		lines = append(lines, fmt.Sprintf("    PRIMARY KEY (%s)", strings.Join(tbl.PKs, ", ")))
	}

	sb.WriteString(strings.Join(lines, ",\n"))
	sb.WriteString(fmt.Sprintf("\n) ENGINE=%s DEFAULT CHARSET=%s COLLATE=%s",
		tbl.Engine, tbl.Charset, tbl.Collate))

	if tbl.Comment != "" {
		sb.WriteString(fmt.Sprintf(" COMMENT='%s'", escapeSQL(tbl.Comment)))
	}

	sb.WriteString(";\n")
	return sb.String()
}

func buildColumnSQL(col Column) string {
	var sb strings.Builder

	sb.WriteString(col.FieldName)
	sb.WriteString(" ")

	// Type with optional length
	dbType := col.DBType
	if idx := strings.Index(dbType, "("); idx > 0 {
		// Type already contains length (e.g., varchar(64))
		sb.WriteString(dbType)
	} else if col.Length != "" {
		sb.WriteString(fmt.Sprintf("%s(%s)", dbType, col.Length))
	} else {
		sb.WriteString(dbType)
	}

	// Unsigned
	if col.Unsigned {
		sb.WriteString(" UNSIGNED")
	}

	// NOT NULL
	if col.NotNull {
		sb.WriteString(" NOT NULL")
	}

	// AUTO_INCREMENT (must come before DEFAULT)
	if col.AutoInc {
		sb.WriteString(" AUTO_INCREMENT")
	}

	// DEFAULT (skip for auto-increment columns)
	if col.HasDefault && !col.AutoInc {
		sb.WriteString(" DEFAULT ")
		sb.WriteString(formatSQLDefault(col.DefaultVal, col.DBType))
	}

	// COMMENT
	if col.Comment != "" {
		sb.WriteString(fmt.Sprintf(" COMMENT '%s'", escapeSQL(col.Comment)))
	}

	return sb.String()
}

func formatSQLDefault(val, dbType string) string {
	if val == "NULL" || val == "null" {
		return "NULL"
	}

	t := strings.ToLower(dbType)
	if idx := strings.Index(t, "("); idx > 0 {
		t = t[:idx]
	}

	switch t {
	case "int", "integer", "bigint", "tinyint", "smallint", "mediumint",
		"float", "double", "decimal", "numeric", "bool", "boolean":
		return val
	default:
		return fmt.Sprintf("'%s'", escapeSQL(val))
	}
}

func escapeSQL(s string) string {
	return strings.ReplaceAll(s, "'", "\\'")
}

// ──────────────────────────────────────────────
// Proto Generation
// ──────────────────────────────────────────────

func genProto(tbl *Table, module string) string {
	var sb strings.Builder

	sb.WriteString("syntax = \"proto3\";\n\n")

	pkgName := tbl.Name
	sb.WriteString(fmt.Sprintf("package %s;\n", pkgName))
	sb.WriteString(fmt.Sprintf("option go_package = \"%s/gen/go/%s\";\n", module, pkgName))
	sb.WriteString("\n")

	// Table comment as file-level comment
	if tbl.Comment != "" {
		sb.WriteString(fmt.Sprintf("// %s — %s\n", tbl.Name, tbl.Comment))
	}

	msgName := toPascalCase(tbl.Name)
	sb.WriteString(fmt.Sprintf("message %s {\n", msgName))

	for i, col := range tbl.Columns {
		fieldNum := i + 1
		protoField := col.FieldName

		if col.Comment != "" {
			sb.WriteString(fmt.Sprintf("    %-10s %s = %d; // %s\n",
				col.ProtoType, protoField, fieldNum, col.Comment))
		} else {
			sb.WriteString(fmt.Sprintf("    %-10s %s = %d;\n",
				col.ProtoType, protoField, fieldNum))
		}
	}

	sb.WriteString("}\n")
	return sb.String()
}

// ──────────────────────────────────────────────
// Utilities
// ──────────────────────────────────────────────

func toPascalCase(s string) string {
	parts := strings.Split(s, "_")
	var result strings.Builder
	for _, p := range parts {
		if len(p) == 0 {
			continue
		}
		runes := []rune(p)
		result.WriteRune(unicode.ToUpper(runes[0]))
		for _, r := range runes[1:] {
			result.WriteRune(unicode.ToLower(r))
		}
	}
	return result.String()
}

// ──────────────────────────────────────────────
// Clean (--clean)
// ──────────────────────────────────────────────

func cleanGenerated(tableDir, sqlDir, protoDir string) error {
	// Scan Excel files to know which tables were generated
	files, err := filepath.Glob(filepath.Join(tableDir, "*.xlsx"))
	if err != nil {
		return fmt.Errorf("scan tables: %w", err)
	}

	var tableNames []string
	for _, f := range files {
		tableNames = append(tableNames, strings.TrimSuffix(filepath.Base(f), ".xlsx"))
	}

	fmt.Println("Cleaning generated files...")
	fmt.Println()

	removed := 0

	// 1. Remove entire sql/ directory
	if err := removeDir(sqlDir); err == nil {
		fmt.Printf("  x %s/\n", sqlDir)
		removed++
	}

	// 2. Remove generated proto files (only those matching table names)
	for _, name := range tableNames {
		protoPath := filepath.Join(protoDir, name+".proto")
		if err := os.Remove(protoPath); err == nil {
			fmt.Printf("  x %s\n", protoPath)
			removed++
		}
	}

	if removed == 0 {
		fmt.Println("  Nothing to clean.")
	} else {
		fmt.Printf("\nDone: %d item(s) removed\n", removed)
	}
	return nil
}

func removeDir(path string) error {
	entries, err := os.ReadDir(path)
	if err != nil {
		return err
	}
	if len(entries) == 0 {
		return os.Remove(path)
	}
	return os.RemoveAll(path)
}
