VERSION ?= 0.1.0
COMMIT ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
BUILT_AT ?= $(shell date -u +"%Y-%m-%dT%H:%M:%SZ")
BIN_DIR ?= ./bin
LDFLAGS := -s -w -X main.version=$(VERSION) -X main.commit=$(COMMIT) -X main.builtAt=$(BUILT_AT)

.PHONY: run-api test build build-api build-relay build-migrate sqlc-generate migrate-up migrate-down docker-up docker-down fmt lint

run-api:
	go run ./cmd/api

test:
	go test ./...

build: build-api build-relay build-migrate

build-api:
	mkdir -p $(BIN_DIR)
	go build -trimpath -ldflags "$(LDFLAGS)" -o $(BIN_DIR)/api ./cmd/api

build-relay:
	mkdir -p $(BIN_DIR)
	go build -trimpath -ldflags "$(LDFLAGS)" -o $(BIN_DIR)/relay ./cmd/relay

build-migrate:
	mkdir -p $(BIN_DIR)
	go build -trimpath -ldflags "$(LDFLAGS)" -o $(BIN_DIR)/migrate ./cmd/migrate

sqlc-generate:
	sqlc generate -f sqlc.yaml

migrate-up:
	go run ./cmd/migrate up

migrate-down:
	go run ./cmd/migrate down

docker-up:
	docker compose -f deploy/docker-compose.yml up -d

docker-down:
	docker compose -f deploy/docker-compose.yml down

fmt:
	go fmt ./...

lint:
	go vet ./...
