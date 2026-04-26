package config

import (
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

func TestLoadAppliesEnvOverride(t *testing.T) {
	t.Setenv("THP_SERVER_PORT", "9090")

	configPath := filepath.Join(t.TempDir(), "config.yaml")
	configBody := []byte(`server:
  port: 8080
  shutdown_timeout: 5s
postgres:
  dsn: postgres://th:th@localhost:5432/th_platform?sslmode=disable
redis:
  addr: localhost:6379
jwt:
  private_key_path: config/keys/jwt-private.pem
  public_key_path: config/keys/jwt-public.pem
  access_ttl: 15m
  refresh_ttl: 720h
aes_key_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
log_level: info
`)

	require.NoError(t, os.WriteFile(configPath, configBody, 0o600))

	cfg, err := Load(configPath)
	require.NoError(t, err)
	require.Equal(t, 9090, cfg.Server.Port)
	require.Equal(t, 5*time.Second, cfg.Server.ShutdownTimeout)
	require.Equal(t, "info", cfg.LogLevel)
}
