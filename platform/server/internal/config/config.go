// Package config loads environment variables once at startup and hands
// out a typed struct. Fail fast on missing required fields — we'd rather
// the process refuse to start than boot with half-wired configuration.
package config

import (
	"errors"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

type Env string

const (
	EnvDevelopment Env = "development"
	EnvStaging     Env = "staging"
	EnvProduction  Env = "production"
)

type Config struct {
	BindAddr  string
	Env       Env
	LogLevel  string

	PGDSN        string
	RedisAddr    string
	RedisPass    string
	RedisDB      int

	JWTPrivateKeyPath string
	JWTPublicKeyPath  string
	JWTAccessTTL      time.Duration
	JWTRefreshTTL     time.Duration

	SMTPHost string
	SMTPPort int
	SMTPUser string
	SMTPPass string
	SMTPFrom string

	OAuthGitHubClientID     string
	OAuthGitHubClientSecret string
	OAuthDiscordClientID    string
	OAuthDiscordClientSecret string
	OAuthQQAppID            string
	OAuthQQAppKey           string

	RelayPublicHosts []string
	CORSAllowedOrigins []string
}

func Load() (*Config, error) {
	c := &Config{
		BindAddr: getEnvDefault("TH_BIND_ADDR", "0.0.0.0:8080"),
		Env:      Env(getEnvDefault("TH_ENV", "development")),
		LogLevel: getEnvDefault("TH_LOG_LEVEL", "info"),

		PGDSN:     requireEnv("TH_PG_DSN"),
		RedisAddr: getEnvDefault("TH_REDIS_ADDR", "localhost:6379"),
		RedisPass: os.Getenv("TH_REDIS_PASSWORD"),

		JWTPrivateKeyPath: getEnvDefault("TH_JWT_PRIVATE_KEY_PATH", "./keys/jwt.pem"),
		JWTPublicKeyPath:  getEnvDefault("TH_JWT_PUBLIC_KEY_PATH", "./keys/jwt.pub"),

		SMTPHost: os.Getenv("TH_SMTP_HOST"),
		SMTPUser: os.Getenv("TH_SMTP_USER"),
		SMTPPass: os.Getenv("TH_SMTP_PASS"),
		SMTPFrom: getEnvDefault("TH_SMTP_FROM", "noreply@example.com"),

		OAuthGitHubClientID:      os.Getenv("TH_OAUTH_GITHUB_CLIENT_ID"),
		OAuthGitHubClientSecret:  os.Getenv("TH_OAUTH_GITHUB_CLIENT_SECRET"),
		OAuthDiscordClientID:     os.Getenv("TH_OAUTH_DISCORD_CLIENT_ID"),
		OAuthDiscordClientSecret: os.Getenv("TH_OAUTH_DISCORD_CLIENT_SECRET"),
		OAuthQQAppID:             os.Getenv("TH_OAUTH_QQ_APP_ID"),
		OAuthQQAppKey:            os.Getenv("TH_OAUTH_QQ_APP_KEY"),
	}

	var err error
	c.RedisDB, err = parseIntEnv("TH_REDIS_DB", 0)
	if err != nil { return nil, err }

	c.JWTAccessTTL, err = parseDurationEnv("TH_JWT_ACCESS_TTL", 15*time.Minute)
	if err != nil { return nil, err }
	c.JWTRefreshTTL, err = parseDurationEnv("TH_JWT_REFRESH_TTL", 30*24*time.Hour)
	if err != nil { return nil, err }

	c.SMTPPort, err = parseIntEnv("TH_SMTP_PORT", 587)
	if err != nil { return nil, err }

	c.RelayPublicHosts = splitComma(os.Getenv("TH_RELAY_PUBLIC_HOSTS"))
	c.CORSAllowedOrigins = splitComma(getEnvDefault("TH_CORS_ALLOWED_ORIGINS", "http://localhost:5173,tauri://localhost"))

	if !c.isValidEnv() {
		return nil, fmt.Errorf("invalid TH_ENV: %s", c.Env)
	}
	return c, nil
}

func (c *Config) IsProd() bool { return c.Env == EnvProduction }

func (c *Config) isValidEnv() bool {
	switch c.Env {
	case EnvDevelopment, EnvStaging, EnvProduction:
		return true
	}
	return false
}

func getEnvDefault(key, def string) string {
	if v := os.Getenv(key); v != "" { return v }
	return def
}

func requireEnv(key string) string {
	v := os.Getenv(key)
	if v == "" {
		panic(errors.New("missing required env: " + key))
	}
	return v
}

func parseIntEnv(key string, def int) (int, error) {
	v := os.Getenv(key)
	if v == "" { return def, nil }
	n, err := strconv.Atoi(v)
	if err != nil { return 0, fmt.Errorf("env %s: %w", key, err) }
	return n, nil
}

func parseDurationEnv(key string, def time.Duration) (time.Duration, error) {
	v := os.Getenv(key)
	if v == "" { return def, nil }
	d, err := time.ParseDuration(v)
	if err != nil { return 0, fmt.Errorf("env %s: %w", key, err) }
	return d, nil
}

func splitComma(s string) []string {
	if s == "" { return nil }
	parts := strings.Split(s, ",")
	out := make([]string, 0, len(parts))
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p != "" { out = append(out, p) }
	}
	return out
}
