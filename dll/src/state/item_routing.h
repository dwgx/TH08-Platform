#pragma once

// Sub-phase 5i Batch B: item routing.
//
// Collection semantics are mutually exclusive: each item is tested against
// the nearest eligible player only, then the vanilla ItemManager reward path
// runs once for that item.

namespace th08_platform::state::item_routing {

bool install();
void uninstall();

struct Counters {
    unsigned long long routed_calls;
    unsigned long long routed_p1;
    unsigned long long routed_p2;
    unsigned long long attracted_p1;
    unsigned long long attracted_p2;
};

Counters snapshot_counters();

}  // namespace th08_platform::state::item_routing
