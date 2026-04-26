package config

import (
	"fmt"
	"path/filepath"
	"strings"
	"time"

	"github.com/go-viper/mapstructure/v2"
	"github.com/spf13/viper"
)

type Config struct {
	Server    ServerConfig   `mapstructure:"server"`
	Postgres  PostgresConfig `mapstructure:"postgres"`
	Redis     RedisConfig    `mapstructure:"redis"`
	JWT       JWTConfig      `mapstructure:"jwt"`
	AESKeyHex string         `mapstructure:"aes_key_hex"`
	LogLevel  string         `mapstructure:"log_level"`
}

type ServerConfig struct {
	Port            int           `mapstructure:"port"`
	ShutdownTimeout time.Duration `mapstructure:"shutdown_timeout"`
}

type PostgresConfig struct {
	DSN string `mapstructure:"dsn"`
}

type RedisConfig struct {
	Addr string `mapstructure:"addr"`
}

type JWTConfig struct {
	PrivateKeyPath string        `mapstructure:"private_key_path"`
	PublicKeyPath  string        `mapstructure:"public_key_path"`
	AccessTTL      time.Duration `mapstructure:"access_ttl"`
	RefreshTTL     time.Duration `mapstructure:"refresh_ttl"`
}

func Load(path string) (Config, error) {
	if path == "" {
		path = filepath.Join("config", "config.yaml")
	}

	v := viper.New()
	v.SetConfigFile(path)
	v.SetConfigType("yaml")
	v.SetEnvPrefix("THP")
	v.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	v.AutomaticEnv()

	setDefaults(v)
	if err := bindEnvs(v); err != nil {
		return Config{}, fmt.Errorf("bind envs: %w", err)
	}

	if err := v.ReadInConfig(); err != nil {
		return Config{}, fmt.Errorf("read config: %w", err)
	}

	var cfg Config
	if err := v.Unmarshal(&cfg, func(dc *mapstructure.DecoderConfig) {
		dc.DecodeHook = mapstructure.ComposeDecodeHookFunc(
			mapstructure.StringToTimeDurationHookFunc(),
		)
	}); err != nil {
		return Config{}, fmt.Errorf("unmarshal config: %w", err)
	}

	return cfg, nil
}

func setDefaults(v *viper.Viper) {
	v.SetDefault("server.port", 8080)
	v.SetDefault("server.shutdown_timeout", "5s")
	v.SetDefault("postgres.dsn", "postgres://th:th@localhost:5432/th_platform?sslmode=disable")
	v.SetDefault("redis.addr", "localhost:6379")
	v.SetDefault("jwt.private_key_path", "config/keys/jwt-private.pem")
	v.SetDefault("jwt.public_key_path", "config/keys/jwt-public.pem")
	v.SetDefault("jwt.access_ttl", "15m")
	v.SetDefault("jwt.refresh_ttl", "720h")
	v.SetDefault("aes_key_hex", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
	v.SetDefault("log_level", "info")
}

func bindEnvs(v *viper.Viper) error {
	keys := []string{
		"server.port",
		"server.shutdown_timeout",
		"postgres.dsn",
		"redis.addr",
		"jwt.private_key_path",
		"jwt.public_key_path",
		"jwt.access_ttl",
		"jwt.refresh_ttl",
		"aes_key_hex",
		"log_level",
	}

	for _, key := range keys {
		if err := v.BindEnv(key); err != nil {
			return err
		}
	}

	return nil
}
