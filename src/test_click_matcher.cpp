#include "click_matcher.h"
#include <cassert>
#include <iostream>
int main() {
    ClickMatcher m;
    auto click = [&](int x, int y, unsigned t, bool blank = true,
                      std::uintptr_t surface = 1) {
        return m.accept(x, y, t, surface, blank, 500, 8, 8);
    };
    assert(!click(10, 10, 100));
    assert(click(12, 11, 300));
    assert(!click(10, 10, 350)); // third click does not toggle again
    m.reset();
    assert(!click(10, 10, 1000));
    assert(!click(10, 10, 1600)); // too slow
    assert(click(10, 10, 1700));
    assert(!click(10, 10, 2000));
    assert(!click(50, 50, 2100)); // too far apart
    assert(!click(50, 50, 2200, false)); // icon, drag, modifier or held click resets
    assert(!click(50, 50, 2300));
    assert(click(50, 50, 2400));
    assert(!click(-1900, -500, 2500));
    assert(click(-1899, -499, 2600)); // monitors left/above primary
    assert(!click(1, 1, 3000));
    assert(!click(1, 1, 3100, true, 2)); // different desktop surface
    m.reset();
    assert(!click(1, 1, 0xfffffff0u));
    assert(click(1, 1, 32)); // Windows millisecond counter rollover
    m.reset();
    assert(!click(1, 1, 4000));
    assert(click(5, 5, 4500)); // inclusive configured time and rectangle boundaries
    std::cout << "Click matcher: all regression checks passed\n";
}
