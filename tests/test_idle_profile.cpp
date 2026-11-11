#include "power/idle_profile.h"
#include <cassert>
#include <initializer_list>

int main() {
    using codex_power::allow_auto_sleep;
    for (bool page : {false, true})
        for (bool idle : {false, true})
            for (bool blocked : {false, true}) {
                assert(!allow_auto_sleep(page, idle, blocked, 0));
                assert(!allow_auto_sleep(page, idle, blocked, 4999999));
                assert(allow_auto_sleep(page, idle, blocked, 5000000) == (page && idle && !blocked));
                assert(allow_auto_sleep(page, idle, blocked, 86400000000LL) == (page && idle && !blocked));
            }
    // User input resets idle time; leaving Codex or starting a competing need
    // must restore the normal profile even after a long idle period.
    assert(allow_auto_sleep(true, true, false, 6000000));
    assert(!allow_auto_sleep(true, true, false, 1));
    assert(!allow_auto_sleep(false, true, false, 6000000));
    assert(!allow_auto_sleep(true, false, false, 6000000));
    assert(!allow_auto_sleep(true, true, true, 6000000));
}
