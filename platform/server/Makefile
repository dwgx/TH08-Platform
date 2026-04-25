# TH-Platform server Makefile

.PHONY: help
help:
	@echo "Targets:"
	@echo "  deps          go mod tidy + download"
	@echo "  build         build gateway + migrate binaries into ./bin"
	@echo "  run           run gateway (respects TH_* env)"
	@echo "  migrate       apply pending migrations"
	@echo "  migrate-down  revert last migration"
	@echo "  keys          generate RSA keypair at ./keys/"
	@echo "  db-up         start local postgres + redis via docker compose"
	@echo "  db-down       stop local postgres + redis"
	@echo "  fmt           go fmt + goimports"
	@echo "  test          run tests"

.PHONY: deps
deps:
	go mod tidy
	go mod download

.PHONY: build
build:
	mkdir -p bin
	go build -o bin/gateway ./cmd/gateway
	go build -o bin/migrate ./cmd/migrate

.PHONY: run
run:
	go run ./cmd/gateway

.PHONY: migrate
migrate:
	go run ./cmd/migrate

.PHONY: migrate-down
migrate-down:
	go run ./cmd/migrate --down 1

.PHONY: keys
keys:
	mkdir -p keys
	openssl genrsa -out keys/jwt.pem 2048
	openssl rsa -in keys/jwt.pem -pubout -out keys/jwt.pub
	chmod 600 keys/jwt.pem

.PHONY: db-up
db-up:
	docker compose -f docker-compose.dev.yml up -d

.PHONY: db-down
db-down:
	docker compose -f docker-compose.dev.yml down

.PHONY: fmt
fmt:
	gofmt -w .
	goimports -w .

.PHONY: test
test:
	go test ./...
