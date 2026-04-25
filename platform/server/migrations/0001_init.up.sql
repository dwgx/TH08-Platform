-- TH-Platform initial schema
-- PostgreSQL 16+
-- Apply with: psql $DATABASE_URL -f 0001_init.up.sql

BEGIN;

-- ─────────────────────────────────────────────────────────────
-- Extensions
-- ─────────────────────────────────────────────────────────────
CREATE EXTENSION IF NOT EXISTS pgcrypto;       -- gen_random_uuid
CREATE EXTENSION IF NOT EXISTS citext;         -- case-insensitive text
CREATE EXTENSION IF NOT EXISTS pg_trgm;        -- trigram search for usernames

-- ─────────────────────────────────────────────────────────────
-- UID sequence + vanity pool + reserved
-- ─────────────────────────────────────────────────────────────
CREATE SEQUENCE IF NOT EXISTS uid_seq
  START WITH 10100
  INCREMENT BY 1
  NO CYCLE
  NO MAXVALUE;

CREATE TABLE IF NOT EXISTS uid_reserved (
  uid        BIGINT PRIMARY KEY,
  note       TEXT
);

-- Seed a few reserved UIDs. Populate 10000-10099 as admin slot range (reserved implicitly by seq starting at 10100).
INSERT INTO uid_reserved (uid, note) VALUES
  (19961, 'Reserved for ZUN / Team Shanghai Alice (东方 1994 start)'),
  (19940808, 'Reserved: 1994 - Aug - 08'),
  (10000, 'Admin slot'),
  (10001, 'Admin slot'),
  (10086, 'CMCC meme reserved')
ON CONFLICT DO NOTHING;

CREATE TABLE IF NOT EXISTS uid_vanity_pool (
  uid         BIGINT       PRIMARY KEY,
  pattern     TEXT         NOT NULL,
  rarity      INT          NOT NULL DEFAULT 1 CHECK (rarity BETWEEN 1 AND 5),
  reserved_at timestamptz
);
CREATE INDEX IF NOT EXISTS uid_vanity_pool_unreserved_idx
  ON uid_vanity_pool (rarity DESC, uid) WHERE reserved_at IS NULL;

-- ─────────────────────────────────────────────────────────────
-- users
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS users (
  uid                BIGINT       PRIMARY KEY,
  username           TEXT         NOT NULL,
  handle             CITEXT       NOT NULL,
  email              CITEXT,
  email_verified_at  timestamptz,
  password_hash      TEXT,
  avatar_url         TEXT,
  bio                TEXT,
  tags               TEXT[]       NOT NULL DEFAULT '{}',
  locale             TEXT         NOT NULL DEFAULT 'zh-CN',
  country            TEXT,
  status             TEXT         NOT NULL DEFAULT 'active'
                     CHECK (status IN ('active','suspended','banned','deleted')),
  roles              TEXT[]       NOT NULL DEFAULT '{}',
  created_at         timestamptz  NOT NULL DEFAULT now(),
  updated_at         timestamptz  NOT NULL DEFAULT now(),
  deleted_at         timestamptz,
  last_seen_at       timestamptz,
  CHECK (char_length(username) BETWEEN 1 AND 32),
  CHECK (char_length(handle)   BETWEEN 3 AND 24),
  CHECK (char_length(bio)      <= 200 OR bio IS NULL),
  CHECK (array_length(tags, 1) IS NULL OR array_length(tags, 1) <= 10)
);
CREATE UNIQUE INDEX IF NOT EXISTS users_handle_uidx ON users (handle);
CREATE UNIQUE INDEX IF NOT EXISTS users_email_uidx  ON users (email) WHERE email IS NOT NULL;
CREATE INDEX IF NOT EXISTS users_username_trgm      ON users USING gin (username gin_trgm_ops);

-- ─────────────────────────────────────────────────────────────
-- user_oauth
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS user_oauth (
  id               UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  user_uid         BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  provider         TEXT         NOT NULL CHECK (provider IN ('qq','discord','github','steam')),
  provider_user_id TEXT         NOT NULL,
  provider_username TEXT,
  access_token     TEXT,
  refresh_token    TEXT,
  scope            TEXT,
  bound_at         timestamptz  NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS user_oauth_provider_uidx
  ON user_oauth (provider, provider_user_id);
CREATE UNIQUE INDEX IF NOT EXISTS user_oauth_user_provider_uidx
  ON user_oauth (user_uid, provider);

-- ─────────────────────────────────────────────────────────────
-- user_sessions (JWT refresh tracking + revocation)
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS user_sessions (
  id                  UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  user_uid            BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  refresh_token_hash  TEXT         NOT NULL UNIQUE,
  issued_at           timestamptz  NOT NULL DEFAULT now(),
  expires_at          timestamptz  NOT NULL,
  last_used_at        timestamptz  NOT NULL DEFAULT now(),
  user_agent          TEXT,
  ip                  INET,
  revoked_at          timestamptz
);
CREATE INDEX IF NOT EXISTS user_sessions_user_active_idx
  ON user_sessions (user_uid) WHERE revoked_at IS NULL;

-- ─────────────────────────────────────────────────────────────
-- friendships (bidirectional rows)
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS friendships (
  user_uid    BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  friend_uid  BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  status      TEXT         NOT NULL
              CHECK (status IN ('pending_out','pending_in','accepted','blocked_out')),
  note        TEXT,
  tags        TEXT[]       NOT NULL DEFAULT '{}',
  created_at  timestamptz  NOT NULL DEFAULT now(),
  updated_at  timestamptz  NOT NULL DEFAULT now(),
  PRIMARY KEY (user_uid, friend_uid),
  CHECK (user_uid <> friend_uid)
);
CREATE INDEX IF NOT EXISTS friendships_friend_idx ON friendships (friend_uid, status);

-- ─────────────────────────────────────────────────────────────
-- blocks
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS blocks (
  user_uid     BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  blocked_uid  BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  reason       TEXT,
  created_at   timestamptz  NOT NULL DEFAULT now(),
  PRIMARY KEY (user_uid, blocked_uid),
  CHECK (user_uid <> blocked_uid)
);

-- ─────────────────────────────────────────────────────────────
-- dm_threads + dm_messages
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS dm_threads (
  id               UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  user_a_uid       BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  user_b_uid       BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  created_at       timestamptz  NOT NULL DEFAULT now(),
  last_message_at  timestamptz,
  CHECK (user_a_uid <> user_b_uid)
);
CREATE UNIQUE INDEX IF NOT EXISTS dm_threads_pair_uidx
  ON dm_threads (LEAST(user_a_uid, user_b_uid), GREATEST(user_a_uid, user_b_uid));

CREATE TABLE IF NOT EXISTS dm_messages (
  id          BIGSERIAL    PRIMARY KEY,
  thread_id   UUID         NOT NULL REFERENCES dm_threads(id) ON DELETE CASCADE,
  sender_uid  BIGINT       NOT NULL REFERENCES users(uid),
  content     TEXT         NOT NULL,
  kind        TEXT         NOT NULL DEFAULT 'text'
              CHECK (kind IN ('text','emoji','room_invite','system')),
  meta        JSONB        NOT NULL DEFAULT '{}',
  created_at  timestamptz  NOT NULL DEFAULT now(),
  edited_at   timestamptz,
  deleted_at  timestamptz
);
CREATE INDEX IF NOT EXISTS dm_messages_thread_desc_idx
  ON dm_messages (thread_id, id DESC);

-- ─────────────────────────────────────────────────────────────
-- groups + group_members + group_channels + group_messages
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS groups (
  id            UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  owner_uid     BIGINT       NOT NULL REFERENCES users(uid),
  name          TEXT         NOT NULL,
  icon_url      TEXT,
  description   TEXT,
  visibility    TEXT         NOT NULL DEFAULT 'private'
                CHECK (visibility IN ('public','private','unlisted')),
  member_count  INT          NOT NULL DEFAULT 1,
  created_at    timestamptz  NOT NULL DEFAULT now(),
  updated_at    timestamptz  NOT NULL DEFAULT now(),
  deleted_at    timestamptz,
  CHECK (char_length(name) BETWEEN 1 AND 64)
);

CREATE TABLE IF NOT EXISTS group_members (
  group_id     UUID         NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  user_uid     BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  role         TEXT         NOT NULL DEFAULT 'member'
               CHECK (role IN ('owner','admin','member')),
  nickname     TEXT,
  joined_at    timestamptz  NOT NULL DEFAULT now(),
  muted_until  timestamptz,
  PRIMARY KEY (group_id, user_uid)
);
CREATE INDEX IF NOT EXISTS group_members_user_idx ON group_members (user_uid);

CREATE TABLE IF NOT EXISTS group_channels (
  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  group_id    UUID         NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  name        TEXT         NOT NULL,
  slug        TEXT         NOT NULL,
  topic       TEXT,
  position    INT          NOT NULL DEFAULT 0,
  created_at  timestamptz  NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS group_channels_slug_uidx
  ON group_channels (group_id, slug);

CREATE TABLE IF NOT EXISTS group_messages (
  id           BIGSERIAL    PRIMARY KEY,
  channel_id   UUID         NOT NULL REFERENCES group_channels(id) ON DELETE CASCADE,
  sender_uid   BIGINT       NOT NULL REFERENCES users(uid),
  content      TEXT         NOT NULL,
  kind         TEXT         NOT NULL DEFAULT 'text'
               CHECK (kind IN ('text','emoji','room_invite','system')),
  meta         JSONB        NOT NULL DEFAULT '{}',
  reply_to_id  BIGINT       REFERENCES group_messages(id) ON DELETE SET NULL,
  created_at   timestamptz  NOT NULL DEFAULT now(),
  edited_at    timestamptz,
  deleted_at   timestamptz
);
CREATE INDEX IF NOT EXISTS group_messages_channel_desc_idx
  ON group_messages (channel_id, id DESC);

-- ─────────────────────────────────────────────────────────────
-- games (game version registry)
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS games (
  id                  TEXT         PRIMARY KEY,
  family              TEXT         NOT NULL,
  display_name        TEXT         NOT NULL,
  version_label       TEXT         NOT NULL,
  expected_exe_sha256 TEXT         NOT NULL,
  supported_modes     TEXT[]       NOT NULL DEFAULT '{}',
  max_players         INT          NOT NULL DEFAULT 2,
  characters          JSONB        NOT NULL DEFAULT '[]',
  config_schema       JSONB        NOT NULL DEFAULT '{}',
  dll_version_min     TEXT         NOT NULL,
  status              TEXT         NOT NULL DEFAULT 'active'
                      CHECK (status IN ('planned','alpha','active','deprecated')),
  created_at          timestamptz  NOT NULL DEFAULT now()
);

-- Seed TH08 as the first active game
INSERT INTO games (id, family, display_name, version_label, expected_exe_sha256,
                   supported_modes, max_players, characters, dll_version_min, status)
VALUES (
  'th08-1.00d',
  'th08',
  'Imperishable Night (东方永夜抄)',
  '1.00d',
  '330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924',
  ARRAY['coop','versus','spectate'],
  2,
  '[
    {"id":"reimu_yukari","label_cn":"灵梦 & 紫","label_ja":"霊夢 & 紫","label_en":"Reimu & Yukari","color":"#ec5c8f"},
    {"id":"marisa_alice","label_cn":"魔理沙 & 爱丽丝","label_ja":"魔理沙 & アリス","label_en":"Marisa & Alice","color":"#f5d033"},
    {"id":"sakuya_remilia","label_cn":"咲夜 & 蕾米","label_ja":"咲夜 & レミリア","label_en":"Sakuya & Remilia","color":"#5b7db8"},
    {"id":"youmu_yuyuko","label_cn":"妖梦 & 幽幽子","label_ja":"妖夢 & 幽々子","label_en":"Youmu & Yuyuko","color":"#9dc29a"}
  ]'::jsonb,
  'alpha'
) ON CONFLICT (id) DO NOTHING;

-- ─────────────────────────────────────────────────────────────
-- rooms
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS rooms (
  id              UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  code            TEXT         NOT NULL,
  host_uid        BIGINT       NOT NULL REFERENCES users(uid),
  game_id         TEXT         NOT NULL REFERENCES games(id),
  name            TEXT         NOT NULL,
  description     TEXT,
  kind            TEXT         NOT NULL DEFAULT 'personal'
                  CHECK (kind IN ('official','personal','community')),
  visibility      TEXT         NOT NULL DEFAULT 'public'
                  CHECK (visibility IN ('public','private','unlisted')),
  join_mode       TEXT         NOT NULL DEFAULT 'open'
                  CHECK (join_mode IN ('open','ask','closed')),
  password_hash   TEXT,
  max_players     INT          NOT NULL DEFAULT 2,
  max_spectators  INT          NOT NULL DEFAULT 8,
  config          JSONB        NOT NULL DEFAULT '{}',
  config_dirty    BOOLEAN      NOT NULL DEFAULT false,
  state           TEXT         NOT NULL DEFAULT 'lobby'
                  CHECK (state IN ('lobby','playing','paused','closed')),
  player_count    INT          NOT NULL DEFAULT 0,
  spectator_count INT          NOT NULL DEFAULT 0,
  region          TEXT,
  created_at      timestamptz  NOT NULL DEFAULT now(),
  last_active_at  timestamptz  NOT NULL DEFAULT now(),
  closed_at       timestamptz
);
CREATE UNIQUE INDEX IF NOT EXISTS rooms_code_active_uidx
  ON rooms (code) WHERE closed_at IS NULL;
CREATE INDEX IF NOT EXISTS rooms_browse_idx
  ON rooms (state, visibility, kind) WHERE closed_at IS NULL;
CREATE INDEX IF NOT EXISTS rooms_host_active_idx
  ON rooms (host_uid) WHERE closed_at IS NULL;

-- ─────────────────────────────────────────────────────────────
-- room_members
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS room_members (
  room_id       UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  user_uid      BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  role          TEXT         NOT NULL DEFAULT 'player'
                CHECK (role IN ('host','player','spectator')),
  character_id  TEXT,
  slot          SMALLINT,
  joined_at     timestamptz  NOT NULL DEFAULT now(),
  PRIMARY KEY (room_id, user_uid)
);
CREATE INDEX IF NOT EXISTS room_members_user_idx ON room_members (user_uid);

-- ─────────────────────────────────────────────────────────────
-- room_invitations
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS room_invitations (
  id            UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  room_id       UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  inviter_uid   BIGINT       NOT NULL REFERENCES users(uid),
  invitee_uid   BIGINT       NOT NULL REFERENCES users(uid),
  status        TEXT         NOT NULL DEFAULT 'pending'
                CHECK (status IN ('pending','accepted','declined','expired','revoked')),
  expires_at    timestamptz  NOT NULL DEFAULT (now() + interval '5 minutes'),
  created_at    timestamptz  NOT NULL DEFAULT now(),
  responded_at  timestamptz
);
CREATE INDEX IF NOT EXISTS room_invitations_invitee_idx
  ON room_invitations (invitee_uid, status);

-- ─────────────────────────────────────────────────────────────
-- match_history
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS match_history (
  id           BIGSERIAL    PRIMARY KEY,
  room_id      UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  game_id      TEXT         NOT NULL REFERENCES games(id),
  started_at   timestamptz  NOT NULL,
  ended_at     timestamptz  NOT NULL,
  duration_ms  INT          NOT NULL,
  result       JSONB        NOT NULL,
  replay_hash  TEXT,
  dll_version  TEXT         NOT NULL,
  created_at   timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS match_history_room_desc_idx
  ON match_history (room_id, id DESC);

-- ─────────────────────────────────────────────────────────────
-- notifications
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS notifications (
  id          BIGSERIAL    PRIMARY KEY,
  user_uid    BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  kind        TEXT         NOT NULL,
  title       TEXT         NOT NULL,
  body        TEXT,
  link        TEXT,
  meta        JSONB        NOT NULL DEFAULT '{}',
  read_at     timestamptz,
  created_at  timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS notifications_user_desc_idx
  ON notifications (user_uid, id DESC);
CREATE INDEX IF NOT EXISTS notifications_user_unread_idx
  ON notifications (user_uid, read_at) WHERE read_at IS NULL;

-- ─────────────────────────────────────────────────────────────
-- lobby_messages (global public chat)
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS lobby_messages (
  id          BIGSERIAL    PRIMARY KEY,
  sender_uid  BIGINT       NOT NULL REFERENCES users(uid),
  content     TEXT         NOT NULL,
  kind        TEXT         NOT NULL DEFAULT 'text'
              CHECK (kind IN ('text','emoji','room_invite','system')),
  meta        JSONB        NOT NULL DEFAULT '{}',
  created_at  timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS lobby_messages_desc_idx
  ON lobby_messages (id DESC);

-- ─────────────────────────────────────────────────────────────
-- reports
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS reports (
  id             BIGSERIAL    PRIMARY KEY,
  reporter_uid   BIGINT       NOT NULL REFERENCES users(uid),
  target_kind    TEXT         NOT NULL CHECK (target_kind IN ('user','message','room')),
  target_id      TEXT         NOT NULL,
  reason         TEXT         NOT NULL,
  detail         TEXT,
  status         TEXT         NOT NULL DEFAULT 'open'
                 CHECK (status IN ('open','reviewing','resolved','dismissed')),
  created_at     timestamptz  NOT NULL DEFAULT now(),
  resolved_at    timestamptz,
  moderator_uid  BIGINT       REFERENCES users(uid)
);
CREATE INDEX IF NOT EXISTS reports_status_idx ON reports (status, created_at DESC);

-- ─────────────────────────────────────────────────────────────
-- helper functions
-- ─────────────────────────────────────────────────────────────

-- Allocate the next UID. Returns either a vanity pick (5% chance) or a
-- fresh sequence value. Skips reserved UIDs.
CREATE OR REPLACE FUNCTION allocate_uid() RETURNS BIGINT AS $$
DECLARE
  new_uid       BIGINT;
  vanity_uid    BIGINT;
  use_vanity    BOOLEAN;
BEGIN
  use_vanity := (random() < 0.05);

  IF use_vanity THEN
    SELECT uid INTO vanity_uid
      FROM uid_vanity_pool
      WHERE reserved_at IS NULL
      ORDER BY rarity DESC, uid
      LIMIT 1
      FOR UPDATE SKIP LOCKED;

    IF vanity_uid IS NOT NULL THEN
      UPDATE uid_vanity_pool SET reserved_at = now() WHERE uid = vanity_uid;
      RETURN vanity_uid;
    END IF;
  END IF;

  -- Fall through: allocate from sequence, skipping reserved
  LOOP
    new_uid := nextval('uid_seq');
    EXIT WHEN NOT EXISTS (SELECT 1 FROM uid_reserved WHERE uid = new_uid);
  END LOOP;
  RETURN new_uid;
END;
$$ LANGUAGE plpgsql;

-- Trigger: updated_at bump
CREATE OR REPLACE FUNCTION touch_updated_at() RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at := now();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_touch_updated_at
  BEFORE UPDATE ON users
  FOR EACH ROW EXECUTE FUNCTION touch_updated_at();

CREATE TRIGGER friendships_touch_updated_at
  BEFORE UPDATE ON friendships
  FOR EACH ROW EXECUTE FUNCTION touch_updated_at();

CREATE TRIGGER groups_touch_updated_at
  BEFORE UPDATE ON groups
  FOR EACH ROW EXECUTE FUNCTION touch_updated_at();

-- ─────────────────────────────────────────────────────────────
-- schema_migrations bookkeeping
-- ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS schema_migrations (
  version     TEXT        PRIMARY KEY,
  applied_at  timestamptz NOT NULL DEFAULT now()
);
INSERT INTO schema_migrations (version) VALUES ('0001_init') ON CONFLICT DO NOTHING;

COMMIT;
