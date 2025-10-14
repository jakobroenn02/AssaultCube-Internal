package main

import (
	"encoding/json"
	"fmt"
	"os"
)

// Config represents the application configuration
type Config struct {
	Database DatabaseConfig `json:"database"`
	App      AppConfig      `json:"app"`
}

// DatabaseConfig holds database connection settings
type DatabaseConfig struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	User     string `json:"user"`
	Password string `json:"password"`
	Database string `json:"database"`
}

// AppConfig holds general application settings
type AppConfig struct {
	Name    string `json:"name"`
	Version string `json:"version"`
}

// LoadConfig loads the configuration from a JSON file
func LoadConfig(filename string) (*Config, error) {
	// Check if file exists
	if _, err := os.Stat(filename); os.IsNotExist(err) {
		return nil, fmt.Errorf("config file '%s' not found. Please create it from appsettings.example.json", filename)
	}

	// Read the file
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("error reading config file: %w", err)
	}

	// Parse JSON
	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		return nil, fmt.Errorf("error parsing config file: %w", err)
	}

	return &config, nil
}

// GetDSN returns the MySQL Data Source Name connection string
func (c *DatabaseConfig) GetDSN() string {
	return fmt.Sprintf("%s:%s@tcp(%s:%d)/%s?parseTime=true&charset=utf8mb4&timeout=10s&readTimeout=30s&writeTimeout=30s",
		c.User,
		c.Password,
		c.Host,
		c.Port,
		c.Database,
	)
}
