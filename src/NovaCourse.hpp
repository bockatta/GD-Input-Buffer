// ============================================================================
//  Nova Run (2.2): the little level in the window's sidebar. One of your icons
//  at a time flies / jumps / rolls through GD-style obstacles; at the end of a
//  section it goes through a portal, the gamemode changes and so do the
//  obstacles.
//
//  This file is only the course itself - the path, the obstacles, the inputs -
//  with no cocos in it, so it can be tested on its own (tests/course_tests.cpp).
//  The window draws it (NovaUI.hpp).
//
//  Units: blocks (one GD block = the icon's size). x runs right, y is up from
//  the floor's top. A path sample is the icon's CENTRE. Sections are built
//  path-first: the inputs make the path, the obstacles are placed where the
//  path leaves room for them - so every section is beaten legitimately.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

namespace ncourse {

constexpr float SPEED = 6.f;       // blocks per second
constexpr float DX    = 0.05f;     // path step (blocks)
constexpr float DT    = DX / SPEED;
constexpr float CEIL  = 7.f;       // corridor height in the flying modes (blocks)
constexpr float HALF  = 0.5f;      // half the icon
constexpr float SEG_LEN = 30.f;    // section length (blocks)
constexpr float RUNWAY  = 5.f;     // nothing in the first blocks of a section

enum Mode { CUBE, SHIP, BALL, UFO, WAVE, SWING, ROBOT, SPIDER, MODE_N };

// Like GD: the cube and the robot have no ceiling; the others fly in a corridor.
inline bool hasCeiling(int m) { return m != CUBE && m != ROBOT; }

// 1 block wide, 1 tall. `base`: where it stands (floor 0 / top of a pillar),
// or for a hanging one (`down`) the y it hangs from.
struct Spike { float x, base; bool down; };
struct Block { float x, y, w, h; };
// An input the icon made (for the effects): 0 press / jump / flip, 1 release,
// 2 landed, 3 spider teleport.
struct Event { float x; int kind; };

struct Rng {
    uint32_t s = 0x9E3779B9u;
    explicit Rng(uint32_t seed = 1u) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return static_cast<float>(next() & 0xFFFFFF) / 16777216.f; }   // 0..1
    float r(float a, float b) { return a + (b - a) * f(); }
    int   i(int n) { return n > 0 ? static_cast<int>(next() % static_cast<uint32_t>(n)) : 0; }
};

struct Segment {
    int   mode = CUBE;
    int   nextMode = -1;          // the portal at x1 (set when the next one is built)
    float x0 = 0.f, x1 = 0.f;
    std::vector<float>   y, rot;  // path, one sample per DX from x0 (x1 included)
    std::vector<uint8_t> grounded;
    std::vector<uint8_t> held;    // the button is down: ship holding, robot boosting, swing pulled up
    std::vector<Spike>   spikes;
    std::vector<Block>   blocks;
    std::vector<float>   wallLo, wallHi; // wave: floor-side wall top / ceiling-side wall bottom (-1 = none)
    std::vector<Event>   events;
    float portalY = 1.5f;

    int   count() const { return static_cast<int>(y.size()); }
    float idx(float x) const { return std::clamp((x - x0) / DX, 0.f, static_cast<float>(std::max(0, count() - 1))); }
    template <class V> static float lerpAt(V const& v, float f) {
        if (v.empty()) return 0.f;
        int const i = static_cast<int>(f);
        int const j = std::min(i + 1, static_cast<int>(v.size()) - 1);
        float const t = f - static_cast<float>(i);
        return static_cast<float>(v[static_cast<size_t>(i)]) * (1.f - t) + static_cast<float>(v[static_cast<size_t>(j)]) * t;
    }
    float yAt(float x) const { return lerpAt(y, idx(x)); }
    float rotAt(float x) const {
        if (rot.empty()) return 0.f;
        float const f = idx(x);
        int const i = static_cast<int>(f);
        int const j = std::min(i + 1, count() - 1);
        float a = rot[static_cast<size_t>(i)], b = rot[static_cast<size_t>(j)];
        while (b - a > 180.f) b -= 360.f;
        while (a - b > 180.f) b += 360.f;
        return a + (b - a) * (f - static_cast<float>(i));
    }
    bool groundedAt(float x) const { return !grounded.empty() && grounded[static_cast<size_t>(idx(x) + 0.5f)] != 0; }
    bool heldAt(float x) const { return !held.empty() && held[static_cast<size_t>(idx(x) + 0.5f)] != 0; }
    void shift(float dx) {
        x0 += dx; x1 += dx;
        for (auto& s : spikes) s.x += dx;
        for (auto& b : blocks) b.x += dx;
        for (auto& e : events) e.x += dx;
    }
};

// ---- collisions (also what the tests check) ---------------------------------
// GD-like hitboxes: the icon's is a bit smaller than its picture, a spike's is
// its middle third.
inline bool hitsSpike(float cx, float cy, Spike const& s, float half = 0.38f) {
    float const sx0 = s.x + 0.32f, sx1 = s.x + 0.68f;
    float const sy0 = s.down ? s.base - 0.6f : s.base, sy1 = s.down ? s.base : s.base + 0.6f;
    return cx + half > sx0 && cx - half < sx1 && cy + half > sy0 && cy - half < sy1;
}
inline bool hitsBlock(float cx, float cy, Block const& b, float half = 0.42f) {
    return cx + half > b.x + 0.02f && cx - half < b.x + b.w - 0.02f && cy + half > b.y + 0.04f && cy - half < b.y + b.h - 0.04f;
}
// first sample index that collides, -1 = clean
inline int firstHit(Segment const& s) {
    for (int i = 0; i < s.count(); i++) {
        float const x = s.x0 + static_cast<float>(i) * DX, y = s.y[static_cast<size_t>(i)];
        if (y < HALF - 0.02f) return i;
        if (hasCeiling(s.mode) && y > CEIL - HALF + 0.02f) return i;
        for (auto const& sp : s.spikes) if (hitsSpike(x, y, sp)) return i;
        for (auto const& b : s.blocks) if (hitsBlock(x, y, b)) return i;
        if (!s.wallLo.empty()) {
            float const lo = s.wallLo[static_cast<size_t>(i)], hi = s.wallHi[static_cast<size_t>(i)];
            if (lo >= 0.f && y - 0.25f < lo) return i;
            if (hi >= 0.f && y + 0.25f > hi) return i;
        }
    }
    return -1;
}

// ---- cube: jumps over spikes, onto blocks -----------------------------------
constexpr float CUBE_JUMP_H = 2.0f, CUBE_AIR = 0.62f;
constexpr float CUBE_V0 = 4.f * CUBE_JUMP_H / CUBE_AIR;                 // 12.9
constexpr float CUBE_G  = 8.f * CUBE_JUMP_H / (CUBE_AIR * CUBE_AIR);    // 41.6
constexpr float CUBE_JUMP_LEN = CUBE_AIR * SPEED;                       // 3.72 blocks

// The robot: GD's variable jump - while the button is held it keeps rising at
// the same speed (with fire under its feet), then falls steeply.
constexpr float ROBOT_V = 8.5f, ROBOT_G = 48.f;
inline float robotJumpUp(float hold) { return hold + ROBOT_V / ROBOT_G; }                          // s to the top
inline float robotJumpH(float hold) { return ROBOT_V * hold + ROBOT_V * ROBOT_V / (2.f * ROBOT_G); } // blocks up
inline float robotJumpLen(float hold) {                                                            // blocks along
    return SPEED * (robotJumpUp(hold) + std::sqrt(2.f * robotJumpH(hold) / ROBOT_G));
}

struct Jump { float x; float hold; };

// The ground modes (cube, robot): run, jump at the given x, land on the floor
// or a block's top.
inline void simGround(Segment& s, float y0, float rot0, std::vector<Jump> const& jumps, bool& crashed) {
    bool const robot = s.mode == ROBOT;
    crashed = false;
    float y = std::max(HALF, y0), vy = 0.f, rot = robot ? 0.f : rot0, boost = 0.f;
    size_t j = 0;
    int const n = static_cast<int>(std::lround((s.x1 - s.x0) / DX)) + 1;
    s.y.assign(static_cast<size_t>(n), HALF);
    s.rot.assign(static_cast<size_t>(n), 0.f);
    s.grounded.assign(static_cast<size_t>(n), 0);
    s.held.assign(static_cast<size_t>(n), 0);
    // the surface under the icon: the floor or a block's top
    auto support = [&](float x, float yy) {
        float g = 0.f;
        for (auto const& b : s.blocks) {
            if (x + 0.42f > b.x && x - 0.42f < b.x + b.w && yy - HALF >= b.y + b.h - 0.12f) g = std::max(g, b.y + b.h);
        }
        return g;
    };
    bool onGround = y <= HALF + 0.001f;
    for (int i = 0; i < n; i++) {
        float const x = s.x0 + static_cast<float>(i) * DX;
        float const g = support(x, y);
        onGround = onGround && y - HALF <= g + 0.001f;
        if (onGround && j < jumps.size() && x >= jumps[j].x) {
            vy = robot ? ROBOT_V : CUBE_V0;
            boost = robot ? jumps[j].hold : 0.f;
            onGround = false;
            s.events.push_back({x, 0});
            j++;
        }
        if (!onGround) {
            if (boost > 0.f) {
                boost -= DT;
                vy = ROBOT_V;
                if (boost <= 0.f) s.events.push_back({x, 1});
            } else {
                vy -= (robot ? ROBOT_G : CUBE_G) * DT;
            }
            y += vy * DT;
            if (!robot) rot += 180.f / CUBE_AIR * DT; // half a turn per jump
            float const g2 = support(x, y - vy * DT); // (the surface it was above)
            if (y - HALF <= g2 && vy <= 0.f) {
                y = g2 + HALF;
                vy = 0.f;
                onGround = true;
                s.events.push_back({x, 2});
            }
        } else {
            y = g + HALF;
            // settle onto the nearest flat side, quickly
            float const tgt = std::round(rot / 90.f) * 90.f;
            float const d = tgt - rot;
            rot += std::clamp(d, -900.f * DT, 900.f * DT);
        }
        // a block's side in front: crashed
        for (auto const& b : s.blocks) if (hitsBlock(x, y, b)) crashed = true;
        if (j < jumps.size() && x > jumps[j].x + 0.25f) crashed = true; // its jump came too late (in the air)
        s.y[static_cast<size_t>(i)] = y;
        s.rot[static_cast<size_t>(i)] = rot;
        s.grounded[static_cast<size_t>(i)] = onGround ? 1 : 0;
        s.held[static_cast<size_t>(i)] = boost > 0.f ? 1 : 0;
    }
    if (j < jumps.size()) crashed = true; // a jump never happened (it was in the air)
}

inline void buildCube(Segment& s, float y0, float rot0, Rng& rng) {
    for (int attempt = 0; attempt < 40; attempt++) {
        s.spikes.clear(); s.blocks.clear(); s.events.clear();
        std::vector<Jump> jumps;
        auto jump = [&](float x) { jumps.push_back({x, 0.f}); };
        float c = s.x0 + RUNWAY + (y0 > 1.f ? 2.f : 0.f) + rng.r(0.f, 1.f);
        float const end = s.x1 - 5.f;
        int lastPat = -1;
        while (c < end) {
            int pat = rng.i(6);
            if (pat == lastPat && rng.f() < 0.6f) pat = rng.i(6);
            lastPat = pat;
            float adv = 0.f;
            switch (pat) {
            case 0: // one spike
                s.spikes.push_back({c, 0.f, false});
                jump(c + 0.5f - CUBE_JUMP_LEN / 2.f - 0.05f);
                adv = 1.f;
                break;
            case 1: // two
                s.spikes.push_back({c, 0.f, false});
                s.spikes.push_back({c + 1.f, 0.f, false});
                jump(c + 1.f - CUBE_JUMP_LEN / 2.f);
                adv = 2.f;
                break;
            case 2: // three (the classic)
                for (int k = 0; k < 3; k++) s.spikes.push_back({c + static_cast<float>(k), 0.f, false});
                jump(c + 1.5f - CUBE_JUMP_LEN / 2.f);
                adv = 3.f;
                break;
            case 3: { // hop onto a block, drop off the end
                float const w = static_cast<float>(2 + rng.i(3));
                s.blocks.push_back({c, 0.f, w, 1.f});
                jump(c - 1.35f);
                adv = w;
                break;
            }
            case 4: // stairs: two blocks up
                s.blocks.push_back({c, 0.f, 3.f, 1.f});
                s.blocks.push_back({c + 3.f, 0.f, 2.5f, 2.f});
                jump(c - 1.35f);
                jump(c + 1.95f);
                adv = 5.5f;
                break;
            default: // a block with a spike on it
                s.blocks.push_back({c, 0.f, 5.f, 1.f});
                s.spikes.push_back({c + 3.5f, 1.f, false});
                jump(c - 1.35f);
                jump(c + 4.f - CUBE_JUMP_LEN / 2.f);
                adv = 5.f;
                break;
            }
            c += adv + rng.r(3.2f, 5.2f);
        }
        bool crashed = false;
        simGround(s, y0, rot0, jumps, crashed);
        if (!crashed && firstHit(s) < 0) return;
    }
    // nothing worked (shouldn't happen): a clear run
    s.spikes.clear(); s.blocks.clear(); s.events.clear();
    bool crashed = false;
    simGround(s, y0, rot0, {}, crashed);
}

// ---- robot: short hops over spikes, long held jumps over pillars and up steps
inline void buildRobot(Segment& s, float y0, Rng& rng) {
    for (int attempt = 0; attempt < 40; attempt++) {
        s.spikes.clear(); s.blocks.clear(); s.events.clear();
        std::vector<Jump> jumps;
        // a jump with this hold, centred over `mid`
        auto over = [&](float mid, float hold) { jumps.push_back({mid - robotJumpLen(hold) / 2.f, hold}); };
        float c = s.x0 + RUNWAY + (y0 > 1.f ? 2.f : 0.f) + rng.r(0.f, 1.f);
        float const end = s.x1 - 6.f;
        while (c < end) {
            int const pat = rng.i(6);
            float adv = 0.f;
            switch (pat) {
            case 0: // one spike: a tap
                s.spikes.push_back({c, 0.f, false});
                over(c + 0.5f, 0.07f);
                adv = 1.f;
                break;
            case 1: // two
                s.spikes.push_back({c, 0.f, false});
                s.spikes.push_back({c + 1.f, 0.f, false});
                over(c + 1.f, 0.13f);
                adv = 2.f;
                break;
            case 2: // three: a longer hold
                for (int k = 0; k < 3; k++) s.spikes.push_back({c + static_cast<float>(k), 0.f, false});
                over(c + 1.5f, 0.19f);
                adv = 3.f;
                break;
            case 3: // a pillar: hold all the way
                s.blocks.push_back({c, 0.f, 1.f, 2.f});
                s.spikes.push_back({c, 2.f, false});
                over(c + 0.5f, 0.32f);
                adv = 1.f;
                break;
            case 4: { // a step two blocks up, then off the end
                float const w = static_cast<float>(4 + rng.i(2));
                s.blocks.push_back({c, 0.f, w, 2.f});
                jumps.push_back({c - 1.7f, 0.24f});
                adv = w;
                break;
            }
            default: // a low wall with spikes in front
                s.spikes.push_back({c, 0.f, false});
                s.blocks.push_back({c + 1.f, 0.f, 1.f, 1.f});
                over(c + 1.f, 0.24f);
                adv = 2.f;
                break;
            }
            c += adv + rng.r(3.4f, 5.4f);
        }
        bool crashed = false;
        simGround(s, y0, 0.f, jumps, crashed);
        if (!crashed && firstHit(s) < 0) return;
    }
    s.spikes.clear(); s.blocks.clear(); s.events.clear();
    bool crashed = false;
    simGround(s, y0, 0.f, {}, crashed);
}

// ---- the flying modes: inputs first, then obstacles where there's room -------
inline void simFlying(Segment& s, float y0, float rot0, Rng& rng) {
    int const n = static_cast<int>(std::lround((s.x1 - s.x0) / DX)) + 1;
    s.y.assign(static_cast<size_t>(n), y0);
    s.rot.assign(static_cast<size_t>(n), 0.f);
    s.grounded.assign(static_cast<size_t>(n), 0);
    s.held.assign(static_cast<size_t>(n), 0);
    float const lo = HALF, hi = CEIL - HALF;
    float y = std::clamp(y0, lo, hi), vy = 0.f, rot = rot0;
    // where it wants to be: a new height every few blocks
    float target = std::clamp(y0, 1.6f, CEIL - 1.6f), nextTarget = s.x0 + RUNWAY - 1.f;
    bool held = false;
    int dir = 1;                // ball / swing / wave: which way it goes
    float wait = 0.f;           // ufo: time since the last tap; wave: blocks to the next turn
    float ballNext = s.x0 + RUNWAY - 1.5f;
    if (s.mode == BALL || s.mode == SWING) dir = y > CEIL / 2.f ? 1 : -1; // gravity direction (1 = up)
    if (s.mode == SPIDER) dir = -1;      // (it lands on the floor first, then teleports)
    if (s.mode == WAVE) { dir = 1; wait = rng.r(0.8f, 2.4f); }
    for (int i = 0; i < n; i++) {
        float const x = s.x0 + static_cast<float>(i) * DX;
        if (x >= nextTarget) {
            float const span = s.mode == UFO ? 2.6f : 3.4f;
            float t = rng.r(1.4f, CEIL - 1.4f);
            if (std::fabs(t - target) < 1.2f) t = std::clamp(target + (t > target ? span : -span) * 0.6f, 1.4f, CEIL - 1.4f);
            target = t;
            nextTarget = x + rng.r(2.8f, 5.f);
        }
        switch (s.mode) {
        case SHIP: {
            bool const want = y + vy * 0.22f < target;
            if (want != held) { held = want; s.events.push_back({x, held ? 0 : 1}); }
            vy += (held ? 22.f : -22.f) * DT;
            vy = std::clamp(vy, -5.2f, 5.2f);
            y += vy * DT;
            rot = -std::atan2(vy, SPEED) * 57.29578f;
            break;
        }
        case UFO: {
            wait += DT;
            if (y < target && vy < 0.6f && wait > 0.16f) {
                vy = 7.4f;
                wait = 0.f;
                s.events.push_back({x, 0});
            }
            vy = std::max(vy - 28.f * DT, -7.f);
            y += vy * DT;
            rot = std::clamp(-vy * 2.2f, -14.f, 14.f);
            break;
        }
        case SWING: {
            int want = dir;
            if (dir < 0 && y < target - 0.35f) want = 1;
            else if (dir > 0 && y > target + 0.35f) want = -1;
            if (want != dir) { dir = want; s.events.push_back({x, 0}); }
            vy = std::clamp(vy + static_cast<float>(dir) * 26.f * DT, -5.8f, 5.8f);
            y += vy * DT;
            rot = -std::atan2(vy, SPEED) * 57.29578f * 0.7f;
            break;
        }
        case BALL: {
            if (x >= ballNext) {
                dir = -dir;
                s.events.push_back({x, 0});
                ballNext = x + rng.r(4.2f, 7.f); // it rides each surface a while, like in GD
            }
            vy = std::clamp(vy + static_cast<float>(dir) * 85.f * DT, -16.f, 16.f); // GD's ball snaps across
            y += vy * DT;
            rot += (dir < 0 ? 1.f : -1.f) * SPEED / 3.14159265f * 360.f * DT; // rolls along its surface
            break;
        }
        case SPIDER: {
            // on a surface: a click teleports it to the other one, gravity flips
            bool const onSurface = (dir < 0 && y <= lo + 0.001f) || (dir > 0 && y >= hi - 0.001f);
            if (onSurface && x >= ballNext) {
                dir = -dir;
                y = dir > 0 ? hi : lo;
                vy = 0.f;
                s.events.push_back({x, 3});
                ballNext = x + rng.r(3.6f, 6.4f);
            } else if (!onSurface) {
                vy = std::clamp(vy + static_cast<float>(dir) * 45.f * DT, -12.f, 12.f);
                y += vy * DT;
            }
            rot = 0.f;
            break;
        }
        case WAVE: {
            wait -= DX;
            float const wlo = 1.35f, whi = CEIL - 1.35f;
            bool turn = wait <= 0.f || (dir > 0 && y >= whi) || (dir < 0 && y <= wlo);
            if (x < s.x0 + 1.f) turn = false;
            if (turn) {
                dir = (dir > 0 && y >= whi) ? -1 : (dir < 0 && y <= wlo) ? 1 : -dir;
                wait = rng.r(0.7f, 2.6f);
                s.events.push_back({x, dir > 0 ? 0 : 1});
            }
            vy = static_cast<float>(dir) * SPEED;
            y += vy * DT;
            rot = dir > 0 ? -45.f : 45.f;
            break;
        }
        default: break;
        }
        if (y < lo) { y = lo; if (vy < 0.f) vy = 0.f; }
        if (y > hi) { y = hi; if (vy > 0.f) vy = 0.f; }
        s.y[static_cast<size_t>(i)] = y;
        s.rot[static_cast<size_t>(i)] = rot;
        s.grounded[static_cast<size_t>(i)] = (y <= lo + 0.001f || y >= hi - 0.001f) ? 1 : 0;
        s.held[static_cast<size_t>(i)] = (s.mode == SHIP && held) || (s.mode == SWING && dir > 0) ? 1 : 0;
    }
}

// lowest / highest the path goes in [a, b]
inline void pathRange(Segment const& s, float a, float b, float& mn, float& mx) {
    mn = 1e9f; mx = -1e9f;
    int const i0 = std::max(0, static_cast<int>((a - s.x0) / DX));
    int const i1 = std::min(s.count() - 1, static_cast<int>((b - s.x0) / DX) + 1);
    for (int i = i0; i <= i1; i++) { mn = std::min(mn, s.y[static_cast<size_t>(i)]); mx = std::max(mx, s.y[static_cast<size_t>(i)]); }
}

// A pillar of blocks with a spike on the end, from the floor (or hanging from
// the ceiling), as tall as `room` allows. Ball: spikes only (it hugs surfaces).
inline void placeColumn(Segment& s, float x, float w, float room, bool floor, bool spikesOnly, Rng& rng) {
    if (room < 1.f) return;
    if (spikesOnly || room < 2.f) {
        for (float k = 0.f; k < w - 0.01f; k += 1.f) s.spikes.push_back({x + k, floor ? 0.f : CEIL, !floor});
        return;
    }
    int const n = std::clamp(static_cast<int>(std::floor(room - 1.f)), 1, 4);
    bool const tip = rng.f() < 0.75f;
    int const blocksN = tip ? n : std::min(4, static_cast<int>(std::floor(room)));
    float const h = static_cast<float>(blocksN);
    s.blocks.push_back({x, floor ? 0.f : CEIL - h, w, h});
    if (tip) {
        for (float k = 0.f; k < w - 0.01f; k += 1.f) s.spikes.push_back({x + k, floor ? h : CEIL - h, !floor});
    }
}

inline void buildFlying(Segment& s, float y0, float rot0, Rng& rng) {
    simFlying(s, y0, rot0, rng);
    if (s.mode == WAVE) {
        // the GD wave corridor: slopes above and below that follow the path
        int const n = s.count();
        s.wallLo.assign(static_cast<size_t>(n), -1.f);
        s.wallHi.assign(static_cast<size_t>(n), -1.f);
        for (int i = 0; i < n; i++) {
            float const x = s.x0 + static_cast<float>(i) * DX;
            float const in = x - (s.x0 + RUNWAY), out = (s.x1 - 2.5f) - x;
            float const e = std::min(in, out);
            if (e < 0.f) continue;
            float const gap = 1.3f + std::max(0.f, 3.f - e) * 1.6f; // opens up at both ends
            float const yy = s.y[static_cast<size_t>(i)];
            float const l = yy - gap, h = yy + gap;
            s.wallLo[static_cast<size_t>(i)] = l > 0.15f ? l : -1.f;
            s.wallHi[static_cast<size_t>(i)] = h < CEIL - 0.15f ? h : -1.f;
        }
        return;
    }
    bool const ball = s.mode == BALL || s.mode == SPIDER; // (they hug the surfaces: spikes only)
    float p = s.x0 + RUNWAY + rng.r(0.5f, 1.5f);
    while (p < s.x1 - 3.5f) {
        float w = 1.f;
        if (ball) w = static_cast<float>(1 + rng.i(2));
        else if (rng.f() < 0.28f) w = static_cast<float>(2 + rng.i(2));
        float mn, mx;
        pathRange(s, p - 0.75f, p + w + 0.75f, mn, mx);
        float const below = mn - HALF - 0.3f;            // room between the floor and the icon
        float const above = (CEIL - 0.f) - (mx + HALF) - 0.3f;
        bool const both = below >= 1.f && above >= 1.f && rng.f() < 0.35f;
        if (both) {
            placeColumn(s, p, w, below, true, ball, rng);
            placeColumn(s, p, w, above, false, ball, rng);
        } else if (below >= above) {
            placeColumn(s, p, w, below, true, ball, rng);
        } else {
            placeColumn(s, p, w, above, false, ball, rng);
        }
        p += w + (ball ? rng.r(1.f, 2.2f) : rng.r(1.6f, 3.4f));
    }
    if (s.mode == SPIDER) {
        // why it teleports: spikes on the surface it just left, right where it was going
        for (auto const& e : s.events) {
            if (e.kind != 3) continue;
            int const i = std::max(0, static_cast<int>((e.x - s.x0) / DX) - 1);
            bool const fromFloor = s.y[static_cast<size_t>(i)] < CEIL / 2.f;
            int const count = 1 + rng.i(2);
            for (int k = 0; k < count; k++) {
                s.spikes.push_back({e.x + 0.9f + static_cast<float>(k), fromFloor ? 0.f : CEIL, !fromFloor});
                if (firstHit(s) >= 0) { s.spikes.pop_back(); break; }
            }
        }
    }
}

inline Segment build(int mode, float x0, float y0, float rot0, Rng& rng) {
    Segment s;
    s.mode = mode;
    s.x0 = x0;
    s.x1 = x0 + SEG_LEN;
    if (mode == CUBE) buildCube(s, y0, rot0, rng);
    else if (mode == ROBOT) buildRobot(s, y0, rng);
    else buildFlying(s, y0, rot0, rng);
    float const ye = s.y.empty() ? 1.5f : s.y.back();
    s.portalY = std::clamp(ye, 1.5f, CEIL - 1.5f);
    return s;
}

// ---- the running course: sections, one after another -------------------------
struct Course {
    Rng rng;
    std::deque<Segment> segs;
    int order[MODE_N] = {CUBE, SHIP, BALL, UFO, WAVE, SWING, ROBOT, SPIDER};
    int orderAt = 0;

    explicit Course(uint32_t seed = 7u) : rng(seed) { shuffle(-1); }
    void shuffle(int notFirst) {
        for (int i = MODE_N - 1; i > 0; i--) std::swap(order[i], order[rng.i(i + 1)]);
        if (order[0] == notFirst) std::swap(order[0], order[MODE_N - 1]);
        orderAt = 0;
    }
    int nextMode() {
        if (orderAt >= MODE_N) shuffle(segs.empty() ? -1 : segs.back().mode);
        return order[orderAt++];
    }
    void start(int firstMode) {
        segs.clear();
        // the first section is the one asked for (the rest of the order shuffles round it)
        for (int i = 0; i < MODE_N; i++) if (order[i] == firstMode) std::swap(order[i], order[0]);
        orderAt = 1;
        segs.push_back(build(firstMode, 0.f, HALF, 0.f, rng));
        ensureAhead(0.f);
    }
    // sections built far enough ahead of x; old ones dropped
    void ensureAhead(float x) {
        while (segs.back().x1 < x + SEG_LEN * 1.2f) {
            Segment const& last = segs.back();
            int const m = nextMode();
            segs.back().nextMode = m;
            float const ye = last.y.empty() ? HALF : last.y.back();
            float const re = last.rot.empty() ? 0.f : last.rot.back();
            // into the cube from the air: it falls; into the others it keeps its height
            (void)re;
            segs.push_back(build(m, last.x1, ye, 0.f, rng));
        }
        while (segs.size() > 2 && segs[1].x1 < x - 20.f) segs.pop_front();
    }
    Segment const* at(float x) const {
        for (auto const& s : segs) if (x >= s.x0 && x < s.x1) return &s;
        return segs.empty() ? nullptr : &segs.back();
    }
    // keep the numbers small: everything moves back by dx (blocks)
    void shift(float dx) { for (auto& s : segs) s.shift(dx); }
};

} // namespace ncourse
