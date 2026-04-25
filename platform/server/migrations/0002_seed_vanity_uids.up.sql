-- Seed the vanity UID pool with ~1000 "pretty" numbers.
-- Called once after initial install. Idempotent via ON CONFLICT.

BEGIN;

-- Repeat-4 (rarity 5):  11110..99999 per-digit
INSERT INTO uid_vanity_pool (uid, pattern, rarity)
SELECT d * 11111, 'REPEAT5', 5
FROM generate_series(1, 9) d
ON CONFLICT DO NOTHING;

-- Repeat-6 (rarity 5):
INSERT INTO uid_vanity_pool (uid, pattern, rarity)
SELECT d * 111111, 'REPEAT6', 5
FROM generate_series(1, 9) d
ON CONFLICT DO NOTHING;

-- AABB 4-digit and 6-digit — rarity 4
INSERT INTO uid_vanity_pool (uid, pattern, rarity)
SELECT a * 11000 + b * 110, 'AABB4', 4
FROM generate_series(1, 9) a, generate_series(0, 9) b
WHERE a <> b
ON CONFLICT DO NOTHING;

INSERT INTO uid_vanity_pool (uid, pattern, rarity)
SELECT a * 110000 + b * 1100 + c * 11, 'AABBCC6', 4
FROM generate_series(1, 9) a, generate_series(0, 9) b, generate_series(0, 9) c
WHERE a <> b AND b <> c
ON CONFLICT DO NOTHING;

-- ABAB 4-digit — rarity 3
INSERT INTO uid_vanity_pool (uid, pattern, rarity)
SELECT a * 1010 + b * 101, 'ABAB4', 3
FROM generate_series(1, 9) a, generate_series(0, 9) b
WHERE a <> b
ON CONFLICT DO NOTHING;

-- Straights 12345, 123456, 234567 … — rarity 4
INSERT INTO uid_vanity_pool (uid, pattern, rarity) VALUES
  (12345, 'STRAIGHT5', 4),
  (23456, 'STRAIGHT5', 4),
  (34567, 'STRAIGHT5', 4),
  (45678, 'STRAIGHT5', 4),
  (56789, 'STRAIGHT5', 4),
  (123456, 'STRAIGHT6', 4),
  (234567, 'STRAIGHT6', 4),
  (345678, 'STRAIGHT6', 4),
  (456789, 'STRAIGHT6', 4),
  (1234567, 'STRAIGHT7', 4)
ON CONFLICT DO NOTHING;

-- Lots of 8s (lucky in Chinese) — rarity 3
INSERT INTO uid_vanity_pool (uid, pattern, rarity) VALUES
  (88888, 'ALL8_5',  5),
  (888888, 'ALL8_6', 5),
  (18888, 'LUCKY8',  3),
  (28888, 'LUCKY8',  3),
  (38888, 'LUCKY8',  3),
  (68888, 'LUCKY8',  3),
  (188888, 'LUCKY8', 3),
  (288888, 'LUCKY8', 3),
  (388888, 'LUCKY8', 3),
  (688888, 'LUCKY8', 3)
ON CONFLICT DO NOTHING;

-- 666 (Chinese meme "smooth") — rarity 2
INSERT INTO uid_vanity_pool (uid, pattern, rarity) VALUES
  (66666, 'ALL6_5', 5),
  (666666, 'ALL6_6', 5),
  (16666, 'LUCKY6', 2),
  (36666, 'LUCKY6', 2),
  (166666, 'LUCKY6', 2)
ON CONFLICT DO NOTHING;

-- Touhou-lore UIDs — rarity 5 (cannot be bought, only rolled)
INSERT INTO uid_vanity_pool (uid, pattern, rarity) VALUES
  (10628, 'TOUHOU_REIMU', 5),       -- 10/06/28 Reimu
  (10720, 'TOUHOU_MARISA', 5),      -- 10/07/20 Marisa
  (19960815, 'TOUHOU_PC98_END', 5), -- PC-98 final date
  (20040815, 'TOUHOU_IN', 5)        -- TH08 release 2004-08-15
ON CONFLICT DO NOTHING;

INSERT INTO schema_migrations (version) VALUES ('0002_seed_vanity_uids') ON CONFLICT DO NOTHING;

COMMIT;
