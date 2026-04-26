# TH-Platform Server

## Quickstart

1. Clone the monorepo and enter `server/`.
2. Run `docker compose -f deploy/docker-compose.yml up -d` to start PostgreSQL 16 and Redis 7.
3. Copy `config/config.example.yaml` to `config/config.yaml`.
4. Run `make run-api` to start the API locally on port `8080`.
5. Verify the process with `curl http://localhost:8080/healthz`.
