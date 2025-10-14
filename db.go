package main

import (
	"database/sql"
	"fmt"
	"log"

	_ "github.com/go-sql-driver/mysql"
)

var DB *sql.DB

// InitDB initializes the database connection using the provided config
func InitDB(config *Config) error {
	// Get the DSN from config
	dsn := config.Database.GetDSN()

	log.Printf("Attempting to connect to MySQL at %s:%d...", config.Database.Host, config.Database.Port)

	var err error
	DB, err = sql.Open("mysql", dsn)
	if err != nil {
		return fmt.Errorf("error opening database: %w", err)
	}

	// Set connection pool settings
	DB.SetMaxOpenConns(10)
	DB.SetMaxIdleConns(5)

	// Test the connection
	if err = DB.Ping(); err != nil {
		return fmt.Errorf("error connecting to database at %s:%d (user: %s, db: %s): %w",
			config.Database.Host,
			config.Database.Port,
			config.Database.User,
			config.Database.Database,
			err)
	}

	log.Printf("Successfully connected to MySQL database '%s' at %s:%d",
		config.Database.Database,
		config.Database.Host,
		config.Database.Port)
	return nil
}

// CloseDB closes the database connection
func CloseDB() {
	if DB != nil {
		if err := DB.Close(); err != nil {
			log.Printf("Error closing database: %v", err)
		}
	}
}
