-- Demo seed for the live preview deploy.
-- Idempotent. Insert-or-update via ON CONFLICT.
--
-- Password hash for ALL demo users is for "demo1234" (argon2id).
-- This is a public demo; do NOT use these credentials in real life.

BEGIN;

-- ── Users (hand-picked vanity-ish UIDs for demo flair) ──────────
INSERT INTO users (uid, username, handle, email, password_hash, avatar_url, bio, tags, locale, country, status, roles, created_at)
VALUES
  (10000, '管理员',           'admin',          'admin@th-platform.local',     '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '系统管理员', ARRAY['admin'],            'zh-CN', 'CN', 'active', ARRAY['admin'],     now() - interval '30 days'),
  (10086, 'dwgx',             'dwgx',           'dwgx@example.com',            '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, 'TH-Platform 作者', ARRAY['th08-main','rollback','tooling'], 'zh-CN', 'CN', 'active', ARRAY['admin'],     now() - interval '30 days'),
  (10628, '博丽霊夢',           'reimu',          'reimu@hakurei.shrine',        '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '博丽神社巫女', ARRAY['main:reimu','th08','th07','th06'], 'ja',    'JP', 'active', ARRAY[]::text[],    now() - interval '20 days'),
  (10720, '霧雨魔理沙',         'marisa',         'marisa@kirisame.house',       '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '普通的魔法使い', ARRAY['main:marisa','speedrun'],          'ja',    'JP', 'active', ARRAY[]::text[],    now() - interval '20 days'),
  (12345, '十六夜咲夜',         'sakuya',         'sakuya@scarlet.devil',        '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '完美而优雅的女仆', ARRAY['main:sakuya','th06','th07'],     'ja',    'JP', 'active', ARRAY[]::text[],    now() - interval '15 days'),
  (88888, '魂魄妖夢',           'youmu',          'youmu@netherworld.gh',        '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '半人半灵庭师', ARRAY['main:youmu','th07','coop-friendly'], 'ja',    'JP', 'active', ARRAY[]::text[],    now() - interval '10 days'),
  (66666, 'NightSparrow',     'nightsparrow',   'mystia@bamboo.forest',        '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '夜雀，唱歌很好听', ARRAY['th08','versus','english'],         'en',    'US', 'active', ARRAY[]::text[],    now() - interval '8 days'),
  (10001, '嗯呐！！',           'nnna',           'nnna@example.com',            '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, 'TH-Platform 测试组', ARRAY['playtest','feedback'],         'zh-CN', 'CN', 'active', ARRAY['mod'],       now() - interval '5 days'),
  (11122, 'TenshiTester',     'tenshi',         'tenshi@celestials.sky',       '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, NULL,            ARRAY['casual'],                          'zh-CN', 'CN', 'active', ARRAY[]::text[],    now() - interval '3 days'),
  (33445, 'LunarReimu',       'lunar_reimu',    'lunar@example.com',           '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, 'Lunatic 难度爱好者', ARRAY['lunatic','speedrun','th08'],      'zh-CN', 'CN', 'active', ARRAY[]::text[],    now() - interval '2 days'),
  (10324, 'PCB_Yuyuko',       'yuyuko',         'yuyuko@hakugyokurou.gh',      '$argon2id$v=19$m=65536,t=3,p=4$ZGVtb3NhbHQxMjM0NTY3OA$abc',  NULL, '幽幽子，永远饥饿', ARRAY['main:yuyuko','th07'],            'ja',    'JP', 'active', ARRAY[]::text[],    now() - interval '6 days')
ON CONFLICT (uid) DO UPDATE SET
  username   = EXCLUDED.username,
  handle     = EXCLUDED.handle,
  bio        = EXCLUDED.bio,
  updated_at = now();

-- Make sure the sequence will skip these manually-inserted UIDs
SELECT setval('uid_seq', GREATEST((SELECT max(uid) FROM users), 10100));

-- ── Friendships (bidirectional rows) ───────────────────────────
INSERT INTO friendships (user_uid, friend_uid, status, created_at) VALUES
  (10086, 10628, 'accepted', now() - interval '15 days'),
  (10628, 10086, 'accepted', now() - interval '15 days'),
  (10086, 10720, 'accepted', now() - interval '15 days'),
  (10720, 10086, 'accepted', now() - interval '15 days'),
  (10086, 10001, 'accepted', now() - interval '5 days'),
  (10001, 10086, 'accepted', now() - interval '5 days'),
  (10628, 10720, 'accepted', now() - interval '20 days'),
  (10720, 10628, 'accepted', now() - interval '20 days'),
  (10628, 12345, 'accepted', now() - interval '12 days'),
  (12345, 10628, 'accepted', now() - interval '12 days'),
  (10086, 33445, 'pending_out', now() - interval '1 day'),
  (33445, 10086, 'pending_in', now() - interval '1 day')
ON CONFLICT (user_uid, friend_uid) DO NOTHING;

-- ── Groups + channels ──────────────────────────────────────────
INSERT INTO groups (id, owner_uid, name, description, visibility, member_count, created_at) VALUES
  ('00000000-0000-0000-0000-000000000001', 10086, 'TH-Platform 内测群', 'TH-Platform 项目的内测频道，欢迎反馈 bug 和建议。', 'public',  4, now() - interval '20 days'),
  ('00000000-0000-0000-0000-000000000002', 10628, '幻想乡联机俱乐部',     '所有东方游戏的联机讨论，TH06-TH18 都欢迎。',       'public',  6, now() - interval '15 days')
ON CONFLICT (id) DO NOTHING;

INSERT INTO group_members (group_id, user_uid, role, joined_at) VALUES
  ('00000000-0000-0000-0000-000000000001', 10086, 'owner', now() - interval '20 days'),
  ('00000000-0000-0000-0000-000000000001', 10001, 'admin', now() - interval '20 days'),
  ('00000000-0000-0000-0000-000000000001', 10628, 'member', now() - interval '15 days'),
  ('00000000-0000-0000-0000-000000000001', 10720, 'member', now() - interval '15 days'),
  ('00000000-0000-0000-0000-000000000002', 10628, 'owner', now() - interval '15 days'),
  ('00000000-0000-0000-0000-000000000002', 10720, 'admin', now() - interval '15 days'),
  ('00000000-0000-0000-0000-000000000002', 12345, 'member', now() - interval '14 days'),
  ('00000000-0000-0000-0000-000000000002', 88888, 'member', now() - interval '13 days'),
  ('00000000-0000-0000-0000-000000000002', 66666, 'member', now() - interval '8 days'),
  ('00000000-0000-0000-0000-000000000002', 10324, 'member', now() - interval '6 days')
ON CONFLICT (group_id, user_uid) DO NOTHING;

INSERT INTO group_channels (id, group_id, name, slug, topic, position) VALUES
  ('10000000-0000-0000-0000-000000000001', '00000000-0000-0000-0000-000000000001', '公告',      'announcements', '只读 · 重要更新',      0),
  ('10000000-0000-0000-0000-000000000002', '00000000-0000-0000-0000-000000000001', '一般讨论',  'general',       '随便聊',                1),
  ('10000000-0000-0000-0000-000000000003', '00000000-0000-0000-0000-000000000001', 'bug反馈',  'bugs',          '遇到 bug 来这里发',     2),
  ('10000000-0000-0000-0000-000000000004', '00000000-0000-0000-0000-000000000002', 'th08永夜抄', 'th08',          '永夜抄联机',            0),
  ('10000000-0000-0000-0000-000000000005', '00000000-0000-0000-0000-000000000002', 'th07妖妖梦', 'th07',          '妖妖梦联机',            1),
  ('10000000-0000-0000-0000-000000000006', '00000000-0000-0000-0000-000000000002', '速通讨论',   'speedrun',      'Anything But Lunatic', 2)
ON CONFLICT (group_id, slug) DO NOTHING;

INSERT INTO group_messages (channel_id, sender_uid, content, kind, created_at) VALUES
  ('10000000-0000-0000-0000-000000000001', 10086, '欢迎大家来到 TH-Platform 内测群！这里是首发版本的反馈频道。',                  'text', now() - interval '20 days'),
  ('10000000-0000-0000-0000-000000000001', 10086, 'V0.1 已经部署到 demo 服。功能列表见 #公告。',                                    'text', now() - interval '5 minutes'),
  ('10000000-0000-0000-0000-000000000002', 10001, '试了一下创建房间，体验不错。',                                                      'text', now() - interval '4 hours'),
  ('10000000-0000-0000-0000-000000000002', 10628, '什么时候支持 TH07？',                                                              'text', now() - interval '3 hours'),
  ('10000000-0000-0000-0000-000000000002', 10086, 'TH08 byte-match 完成后就开 TH07 的项目。',                                        'text', now() - interval '2 hours 50 minutes'),
  ('10000000-0000-0000-0000-000000000002', 10720, '[doge]',                                                                            'text', now() - interval '2 hours 49 minutes'),
  ('10000000-0000-0000-0000-000000000003', 10001, '注册时邮箱字段没校验大小写，建议修。',                                              'text', now() - interval '1 day'),
  ('10000000-0000-0000-0000-000000000003', 10086, 'CITEXT 已经做了，是不是显示问题？复现一下截图发我',                                  'text', now() - interval '1 day'),
  ('10000000-0000-0000-0000-000000000004', 10628, '今晚 TH08 联机么？我做房主。',                                                       'text', now() - interval '6 hours'),
  ('10000000-0000-0000-0000-000000000004', 10720, '来！',                                                                              'text', now() - interval '5 hours 58 minutes'),
  ('10000000-0000-0000-0000-000000000004', 12345, '我可以观战吗？',                                                                     'text', now() - interval '5 hours 50 minutes'),
  ('10000000-0000-0000-0000-000000000005', 10324, 'TH07 玩家在哪 都没人……',                                                            'text', now() - interval '2 days'),
  ('10000000-0000-0000-0000-000000000005', 10720, '+1',                                                                                'text', now() - interval '2 days'),
  ('10000000-0000-0000-0000-000000000006', 33445, 'TH08 NMNB 速通新纪录！19m04s。',                                                    'text', now() - interval '12 hours'),
  ('10000000-0000-0000-0000-000000000006', 10086, '🔥',                                                                                'emoji', now() - interval '11 hours 50 minutes')
ON CONFLICT DO NOTHING;

-- ── Rooms (active demo rooms) ──────────────────────────────────
INSERT INTO rooms (id, code, host_uid, game_id, name, kind, visibility, join_mode, max_players, max_spectators, region, state, player_count, spectator_count, created_at, last_active_at) VALUES
  ('20000000-0000-0000-0000-000000000001', 'TH8-A001', 10628, 'th08-1.00d', '随便玩玩 · 新手欢迎',                'personal',  'public', 'open',   2, 8, 'cn-sh', 'lobby',   1, 0, now() - interval '8 minutes',  now() - interval '20 seconds'),
  ('20000000-0000-0000-0000-000000000002', 'TH8-B042', 10720, 'th08-1.00d', 'Marisa 速通房',                       'personal',  'public', 'ask',    2, 4, 'cn-sh', 'lobby',   1, 0, now() - interval '12 minutes', now() - interval '90 seconds'),
  ('20000000-0000-0000-0000-000000000003', 'TH8-C123', 12345, 'th08-1.00d', 'Lunatic 试炼 · 高手匹配',             'personal',  'public', 'open',   2, 6, 'jp-tk', 'playing', 2, 3, now() - interval '23 minutes', now() - interval '5 seconds'),
  ('20000000-0000-0000-0000-000000000004', 'TH8-D777', 88888, 'th08-1.00d', '日服 · 妖梦主战',                     'personal',  'public', 'open',   2, 4, 'jp-tk', 'lobby',   1, 0, now() - interval '5 minutes',  now() - interval '10 seconds'),
  ('20000000-0000-0000-0000-000000000005', 'TH8-OFF1', 10000, 'th08-1.00d', '官方 0 号房 · 中国上海',               'official',  'public', 'open',   2, 8, 'cn-sh', 'lobby',   0, 0, now() - interval '3 days',     now() - interval '3 days'),
  ('20000000-0000-0000-0000-000000000006', 'TH8-OFF2', 10000, 'th08-1.00d', '官方 1 号房 · 日本东京',               'official',  'public', 'open',   2, 8, 'jp-tk', 'lobby',   0, 0, now() - interval '3 days',     now() - interval '3 days'),
  ('20000000-0000-0000-0000-000000000007', 'TH8-PRV1', 10086, 'th08-1.00d', '内测私房',                            'personal',  'private','ask',    2, 2, 'cn-sh', 'lobby',   1, 0, now() - interval '15 minutes', now() - interval '2 minutes')
ON CONFLICT (id) DO NOTHING;

-- Members
INSERT INTO room_members (room_id, user_uid, role, character_id, slot, joined_at) VALUES
  ('20000000-0000-0000-0000-000000000001', 10628, 'host',   'reimu_yukari',    1, now() - interval '8 minutes'),
  ('20000000-0000-0000-0000-000000000002', 10720, 'host',   'marisa_alice',    1, now() - interval '12 minutes'),
  ('20000000-0000-0000-0000-000000000003', 12345, 'host',   'sakuya_remilia',  1, now() - interval '23 minutes'),
  ('20000000-0000-0000-0000-000000000003', 33445, 'player', 'youmu_yuyuko',    2, now() - interval '20 minutes'),
  ('20000000-0000-0000-0000-000000000003', 66666, 'spectator', NULL,           NULL, now() - interval '15 minutes'),
  ('20000000-0000-0000-0000-000000000003', 10324, 'spectator', NULL,           NULL, now() - interval '12 minutes'),
  ('20000000-0000-0000-0000-000000000003', 11122, 'spectator', NULL,           NULL, now() - interval '8 minutes'),
  ('20000000-0000-0000-0000-000000000004', 88888, 'host',   'youmu_yuyuko',    1, now() - interval '5 minutes'),
  ('20000000-0000-0000-0000-000000000007', 10086, 'host',   'reimu_yukari',    1, now() - interval '15 minutes')
ON CONFLICT DO NOTHING;

-- ── Lobby messages (public chat) ───────────────────────────────
INSERT INTO lobby_messages (sender_uid, content, kind, created_at) VALUES
  (10086, '欢迎来到 TH-Platform 公开测试！',                                              'text', now() - interval '2 hours'),
  (10086, '这是一个为东方系列设计的联机平台 (Discord + Parsec 二合一)',                  'text', now() - interval '2 hours'),
  (10086, 'V1 仅支持 TH08 永夜抄。其他游戏正在排期。',                                     'text', now() - interval '2 hours'),
  (10628, '欸 这个 UI 比 GameRanger 好看好多',                                              'text', now() - interval '1 hour 50 minutes'),
  (10720, '能不能加个房间过滤好友的功能',                                                  'text', now() - interval '1 hour 30 minutes'),
  (10086, '在做了，下个版本',                                                              'text', now() - interval '1 hour 28 minutes'),
  (12345, 'Lunatic 房间求 P2',                                                            'text', now() - interval '1 hour 25 minutes'),
  (33445, '来了',                                                                          'text', now() - interval '1 hour 24 minutes'),
  (66666, 'Hi everyone! Just joined the platform from the EN community.',                'text', now() - interval '1 hour'),
  (10001, '欢迎！界面有翻译可以切换语言（左下角）',                                         'text', now() - interval '58 minutes'),
  (10628, '今晚有人组队 NN 通关吗',                                                         'text', now() - interval '40 minutes'),
  (10720, '8 点',                                                                           'text', now() - interval '38 minutes'),
  (88888, 'Wishing for a good night...',                                                    'text', now() - interval '30 minutes'),
  (10324, '幽幽子来了！',                                                                  'text', now() - interval '28 minutes'),
  (11122, '怎么用啊',                                                                       'text', now() - interval '20 minutes'),
  (10001, '@TenshiTester 看右上角问号，有快速指引',                                          'text', now() - interval '19 minutes'),
  (10628, '我开了一个 TH8-A001，新手欢迎',                                                 'room_invite', now() - interval '8 minutes'),
  (10720, '我也开了 TH8-B042，速通主题',                                                    'room_invite', now() - interval '12 minutes'),
  (10086, '今天上线人数破 100 了，谢谢各位',                                                'text', now() - interval '2 minutes'),
  (33445, '加油 dwgx 大佬！',                                                              'text', now() - interval '1 minute')
ON CONFLICT DO NOTHING;

-- ── Notifications (small batch for the "@dwgx" demo user) ──────
INSERT INTO notifications (user_uid, kind, title, body, link, created_at) VALUES
  (10086, 'friend_request', '新好友请求', 'LunarReimu 向你发送了好友请求', '/friends/pending',          now() - interval '1 day'),
  (10086, 'mention',        '提及',       'TenshiTester 在大厅提到了你',     '/lobby',                    now() - interval '20 minutes'),
  (10086, 'room_invite',    '房间邀请',   '霊夢 邀请你加入 TH8-A001',      '/rooms/20000000-0000-0000-0000-000000000001', now() - interval '8 minutes')
ON CONFLICT DO NOTHING;

-- ── Match history (a few past matches) ─────────────────────────
INSERT INTO match_history (room_id, game_id, started_at, ended_at, duration_ms, result, dll_version) VALUES
  ('20000000-0000-0000-0000-000000000003', 'th08-1.00d',
    now() - interval '50 minutes', now() - interval '23 minutes',
    1620000,
    '{"winner_uid":12345,"scores":{"12345":823400,"33445":698000},"deaths":{"12345":2,"33445":5},"cause":"cleared_stage_6"}',
    '0.1.0-alpha'),
  ('20000000-0000-0000-0000-000000000003', 'th08-1.00d',
    now() - interval '1 hour 30 minutes', now() - interval '55 minutes',
    2100000,
    '{"winner_uid":33445,"scores":{"12345":712000,"33445":924100},"deaths":{"12345":4,"33445":1},"cause":"cleared_stage_6"}',
    '0.1.0-alpha')
ON CONFLICT DO NOTHING;

INSERT INTO schema_migrations (version) VALUES ('0003_seed_demo') ON CONFLICT DO NOTHING;

COMMIT;
