BEGIN;
DELETE FROM uid_vanity_pool;
DELETE FROM schema_migrations WHERE version = '0002_seed_vanity_uids';
COMMIT;
