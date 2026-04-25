#pragma once

// Sub-phase 5i Batch C/D: enemy aim routing.
//
// Installed hooks:
//   - sub_420120  (ECL float resolver)
//   - sub_420950  (ECL pointer resolver)
//   - sub_41F420  (secondary ECL resolver)
//   - sub_422020  (enemy aim/bounce helper)
//   - sub_4224A0  (enemy side/wrap aim helper)
//   - sub_422720  (enemy bullet spawn distance gate)
//   - sub_428310  (vector helper with &g_Player.pos)
//   - EnemyManager::OnUpdate @ 0x42C660
//   - Spellcard::SpellcardOnUpdateImpl @ 0x416B90
//   - standalone target-acquisition block @ 0x426C40

#include <cstdint>

namespace th08_platform::state::enemy_aim {

bool install();
void uninstall();

struct PairCounters {
    unsigned long long p1 = 0;
    unsigned long long p2 = 0;
};

struct Counters {
    unsigned long long calls_total = 0;
    PairCounters float_resolver{};
    PairCounters pointer_resolver{};
    PairCounters secondary_resolver{};
    PairCounters bounce_helper{};
    PairCounters wrap_helper{};
    PairCounters spawn_gate{};
    PairCounters vector_helper{};
    PairCounters enemy_manager{};
    PairCounters spellcard{};
    PairCounters table_block{};
};

Counters snapshot_counters();

}  // namespace th08_platform::state::enemy_aim
