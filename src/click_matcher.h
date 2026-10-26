#pragma once
#include <cstdint>
#include <cstdlib>

// Portable gesture logic. A non-blank, modified, held or dragged click breaks a pair.
class ClickMatcher {
    bool armed_ = false;
    int x_ = 0, y_ = 0;
    std::uint32_t time_ = 0;
    std::uintptr_t surface_ = 0;
public:
    void reset() { armed_ = false; }
    bool accept(int x, int y, std::uint32_t time, std::uintptr_t surface,
                bool eligible, unsigned interval, int width, int height) {
        if (!eligible || !surface) { reset(); return false; }
        const bool pair = armed_ && surface == surface_ &&
            std::uint32_t(time - time_) <= interval &&
            std::abs(x - x_) <= width / 2 && std::abs(y - y_) <= height / 2;
        if (pair) { reset(); return true; }
        armed_ = true; x_ = x; y_ = y; time_ = time; surface_ = surface;
        return false;
    }
};
