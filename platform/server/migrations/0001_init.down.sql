-- Reverse of 0001_init.up.sql. Destructive: drops every table.

BEGIN;

DROP TRIGGER IF EXISTS users_touch_updated_at ON users;
DROP TRIGGER IF EXISTS friendships_touch_updated_at ON friendships;
DROP TRIGGER IF EXISTS groups_touch_updated_at ON groups;

DROP FUNCTION IF EXISTS touch_updated_at();
DROP FUNCTION IF EXISTS allocate_uid();

DROP TABLE IF EXISTS reports;
DROP TABLE IF EXISTS lobby_messages;
DROP TABLE IF EXISTS notifications;
DROP TABLE IF EXISTS match_history;
DROP TABLE IF EXISTS room_invitations;
DROP TABLE IF EXISTS room_members;
DROP TABLE IF EXISTS rooms;
DROP TABLE IF EXISTS games;
DROP TABLE IF EXISTS group_messages;
DROP TABLE IF EXISTS group_channels;
DROP TABLE IF EXISTS group_members;
DROP TABLE IF EXISTS groups;
DROP TABLE IF EXISTS dm_messages;
DROP TABLE IF EXISTS dm_threads;
DROP TABLE IF EXISTS blocks;
DROP TABLE IF EXISTS friendships;
DROP TABLE IF EXISTS user_sessions;
DROP TABLE IF EXISTS user_oauth;
DROP TABLE IF EXISTS users;
DROP TABLE IF EXISTS uid_vanity_pool;
DROP TABLE IF EXISTS uid_reserved;
DROP SEQUENCE IF EXISTS uid_seq;

DELETE FROM schema_migrations WHERE version = '0001_init';

COMMIT;
