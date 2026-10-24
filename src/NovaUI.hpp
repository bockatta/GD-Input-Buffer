// ============================================================================
//  Nova UI pieces (2.2): textures the window draws with, the neon glow, the
//  starfield, the Nova Run course in the sidebar and the rocket.
//  Included by main.cpp inside its namespace, after `aa` and IBLabel (no
//  includes of its own here - main.cpp has them all).
// ============================================================================

namespace nv {

// ---- baked textures ----------------------------------------------------------
// Made once by the mod (supersampled, so every edge is smooth), white unless
// they're the rocket / the galaxy, tinted when used. Remade after a graphics
// reload (reset()).
struct Px { float r = 1.f, g = 1.f, b = 1.f, a = 0.f; };

// fn(x, y) -> Px, with x right and y UP, in pixels. `ss` samples per pixel side.
template <class F>
CCTexture2D* bake(int w, int h, F&& fn, int ss = 3) {
    std::vector<uint8_t> px(static_cast<size_t>(w * h * 4));
    float const inv = 1.f / static_cast<float>(ss * ss);
    for (int row = 0; row < h; row++) {
        for (int x = 0; x < w; x++) {
            float pr = 0.f, pg = 0.f, pb = 0.f, pa = 0.f;
            for (int sy = 0; sy < ss; sy++) {
                for (int sx = 0; sx < ss; sx++) {
                    float const fx = static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / static_cast<float>(ss);
                    float const fy = static_cast<float>(h - 1 - row) + (static_cast<float>(sy) + 0.5f) / static_cast<float>(ss);
                    Px const p = fn(fx, fy);
                    float const a = std::clamp(p.a, 0.f, 1.f);
                    pr += p.r * a; pg += p.g * a; pb += p.b * a; pa += a;
                }
            }
            size_t const i = static_cast<size_t>((row * w + x) * 4);
            float const a = pa * inv;
            float const k = pa > 0.f ? 1.f / pa : 0.f;
            px[i]     = static_cast<uint8_t>(std::lround(255.f * std::clamp(pr * k, 0.f, 1.f)));
            px[i + 1] = static_cast<uint8_t>(std::lround(255.f * std::clamp(pg * k, 0.f, 1.f)));
            px[i + 2] = static_cast<uint8_t>(std::lround(255.f * std::clamp(pb * k, 0.f, 1.f)));
            px[i + 3] = static_cast<uint8_t>(std::lround(255.f * std::clamp(a, 0.f, 1.f)));
        }
    }
    auto* t = new CCTexture2D();
    if (!t->initWithData(px.data(), kCCTexture2DPixelFormat_RGBA8888, static_cast<unsigned>(w), static_cast<unsigned>(h),
                         CCSize(static_cast<float>(w), static_cast<float>(h)))) {
        t->release();
        return nullptr;
    }
    t->setAntiAliasTexParameters();
    return t; // one reference, kept until reset()
}

inline float hash2(int x, int y, int seed = 0) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFF) / 16777216.f;
}
inline float vnoise(float x, float y, int seed = 0) {
    int const ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    float const fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy);
    float const ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy);
    float const a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed), c = hash2(ix, iy + 1, seed), d = hash2(ix + 1, iy + 1, seed);
    return (a * (1.f - ux) + b * ux) * (1.f - uy) + (c * (1.f - ux) + d * ux) * uy;
}
inline float fbm(float x, float y, int seed = 0) {
    float v = 0.f, amp = 0.5f;
    for (int o = 0; o < 4; o++) { v += amp * vnoise(x, y, seed + o * 17); x *= 2.03f; y *= 2.03f; amp *= 0.5f; }
    return v;
}
inline float smooth(float e0, float e1, float x) {
    float const t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
// signed distance to a rounded box centred at 0 with half size (hx, hy), corner r
inline float sdRoundBox(float x, float y, float hx, float hy, float r) {
    float const qx = std::fabs(x) - (hx - r), qy = std::fabs(y) - (hy - r);
    return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
}
inline bool inPoly(float x, float y, std::initializer_list<CCPoint> poly) {
    bool in = false;
    CCPoint const* p = poly.begin();
    size_t const n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((p[i].y > y) != (p[j].y > y)) && (x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x)) in = !in;
    }
    return in;
}

// Outline glow for the cards: brightest on the edge of a rounded box, fading
// out (and a little in). 9-sliced to any card size.
constexpr int GLOW_TEX = 64, GLOW_R = 10, GLOW_S = 10; // texture, corner radius, spread (px)
inline CCTexture2D* s_glowEdge = nullptr;
inline CCTexture2D* glowEdge() {
    if (!s_glowEdge) {
        float const h = GLOW_TEX * 0.5f - GLOW_S;
        s_glowEdge = bake(GLOW_TEX, GLOW_TEX, [h](float x, float y) {
            float const sd = sdRoundBox(x - GLOW_TEX * 0.5f, y - GLOW_TEX * 0.5f, h, h, static_cast<float>(GLOW_R));
            float const a = sd >= 0.f ? std::exp(-(sd / 4.2f) * (sd / 4.2f)) : std::exp(-(sd / 1.4f) * (sd / 1.4f));
            return Px{1.f, 1.f, 1.f, a};
        }, 2);
    }
    return s_glowEdge;
}

// A soft round glow (smoke, fire, nebulae, halos).
inline CCTexture2D* s_soft = nullptr;
inline CCTexture2D* soft() {
    if (!s_soft) {
        s_soft = bake(64, 64, [](float x, float y) {
            float const d = std::hypot(x - 32.f, y - 32.f) / 32.f;
            float const a = std::exp(-d * d * 4.5f) * std::clamp(1.f - d * d, 0.f, 1.f);
            return Px{1.f, 1.f, 1.f, a};
        }, 1);
    }
    return s_soft;
}

// A four-point twinkle.
inline CCTexture2D* s_sparkle = nullptr;
inline CCTexture2D* sparkle() {
    if (!s_sparkle) {
        s_sparkle = bake(32, 32, [](float x, float y) {
            float const dx = x - 16.f, dy = y - 16.f;
            float const core = std::exp(-(dx * dx + dy * dy) / 6.f);
            float const rayH = std::exp(-(dy * dy) / 0.9f) * std::exp(-(dx * dx) / 70.f);
            float const rayV = std::exp(-(dx * dx) / 0.9f) * std::exp(-(dy * dy) / 70.f);
            return Px{1.f, 1.f, 1.f, std::min(1.f, core + rayH + rayV)};
        }, 2);
    }
    return s_sparkle;
}

// The settings cog: eight teeth round a ring with a hole.
inline CCTexture2D* s_gear = nullptr;
inline CCTexture2D* gear() {
    if (!s_gear) {
        s_gear = bake(64, 64, [](float x, float y) {
            float const dx = x - 32.f, dy = y - 32.f;
            float const r = std::hypot(dx, dy);
            float const ang = std::atan2(dy, dx);
            constexpr float pitch = 6.2831853f / 8.f;
            float const k = std::round(ang / pitch);
            float const da = std::fabs(ang - k * pitch);
            constexpr float rootR = 20.5f, tipR = 29.f, hole = 9.5f;
            float const t = std::clamp((r - rootR) / (tipR - rootR), 0.f, 1.f);
            float const halfW = 6.2f - 2.2f * t;                 // teeth narrow towards the tip
            bool in = r <= rootR || (r <= tipR && da * r <= halfW);
            if (r < hole) in = false;
            return Px{1.f, 1.f, 1.f, in ? 1.f : 0.f};
        }, 4);
    }
    return s_gear;
}

// The GD gamemode portals, one per mode, like the game's: a coloured oval ring
// with a white edge in front, the black drum with its blue grille behind it,
// a half-round tab on the right with the mode's symbol, and dots above and
// below. 72 x 144 px; the ring's centre is at x 24 (PORTAL_CX).
constexpr float PORTAL_W = 72.f, PORTAL_H = 144.f, PORTAL_CX = 24.f, PORTAL_RING_H = 104.f;
inline ccColor3B portalColor(int mode) {
    switch (mode) {
        case 0: return {92, 236, 82};    // cube: green
        case 1: return {255, 112, 230};  // ship: pink
        case 2: return {250, 92, 52};    // ball: red-orange
        case 3: return {255, 178, 42};   // ufo: orange
        case 4: return {72, 214, 255};   // wave: cyan
        case 5: return {255, 236, 54};   // swing: yellow
        case 6: return {238, 238, 246};  // robot: white
        default: return {162, 74, 252};  // spider: purple
    }
}
// the symbol on the tab, about 12 px across, (u, v) from its centre
inline bool portalGlyph(int mode, float u, float v) {
    auto seg = [](float px, float py, float ax, float ay, float bx, float by) {
        float const dx = bx - ax, dy = by - ay;
        float const t = std::clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0.f, 1.f);
        return std::hypot(px - ax - dx * t, py - ay - dy * t);
    };
    switch (mode) {
        case 0: return std::fabs(u) < 4.6f && std::fabs(v) < 4.6f;                                        // cube
        case 1: return ((u + 0.5f) / 6.2f) * ((u + 0.5f) / 6.2f) + ((v + 1.f) / 2.6f) * ((v + 1.f) / 2.6f) < 1.f
                       || std::hypot(u + 0.8f, v - 1.3f) < 2.6f;                                          // ship
        case 2: {                                                                                         // ball
            float const r = std::hypot(u, v), th = std::atan2(v, u);
            return r < 3.9f + 1.7f * std::pow(std::max(0.f, std::cos(8.f * th)), 3.f);
        }
        case 3: return (v > -0.6f && std::hypot(u, v + 0.6f) < 3.9f)                                       // ufo
                       || (u / 6.4f) * (u / 6.4f) + ((v + 1.4f) / 1.9f) * ((v + 1.4f) / 1.9f) < 1.f;
        case 4: return inPoly(u, v, {{-5.f, 5.f}, {6.f, 0.f}, {-5.f, -5.f}, {-2.2f, 0.f}});              // wave
        case 5: return std::hypot(u, v + 0.6f) < 3.8f || std::hypot(u - 4.f, v + 2.8f) < 1.7f            // swing
                       || std::hypot(u + 4.f, v + 2.8f) < 1.7f || (std::fabs(u) < 1.f && v > 2.f && v < 5.5f);
        case 6: return (std::fabs(u) < 4.4f && v > -0.6f && v < 4.8f)                                     // robot
                       || (u > -3.6f && u < -1.2f && v > -5.2f && v <= -0.6f) || (u > 1.2f && u < 3.6f && v > -5.2f && v <= -0.6f);
        default:                                                                                          // spider
            return (v > -1.5f && std::hypot(u, v + 1.5f) < 4.2f) || seg(u, v, -2.f, -1.2f, -5.5f, -5.f) < 0.85f
                   || seg(u, v, 2.f, -1.2f, 5.5f, -5.f) < 0.85f || seg(u, v, 0.f, -1.2f, 0.f, -5.2f) < 0.85f;
    }
}
inline CCTexture2D* s_portals[8] = {};
inline CCTexture2D* portal(int mode) {
    mode = std::clamp(mode, 0, 7);
    if (!s_portals[mode]) {
        ccColor3B const c = portalColor(mode);
        float const cr = c.r / 255.f, cg = c.g / 255.f, cb = c.b / 255.f;
        bool const robot = mode == 6;
        s_portals[mode] = bake(static_cast<int>(PORTAL_W), static_cast<int>(PORTAL_H), [=](float x, float y) {
            constexpr float fx = PORTAL_CX, bx = 40.f, cy = 72.f, a = 13.f, b = 52.f;
            Px const white{1.f, 1.f, 1.f, 1.f};
            Px const edge = robot ? Px{0.55f, 0.55f, 0.62f, 1.f} : white;
            Px const col{cr, cg, cb, 1.f};
            auto shaded = [&](float k) { return Px{std::min(1.f, cr * k), std::min(1.f, cg * k), std::min(1.f, cb * k), 1.f}; };
            // dots above / below the ring and at the drum's right
            float const dots[4][2] = {{fx, cy + b + 9.f}, {fx, cy - b - 9.f}, {bx + a - 2.f, cy + 30.f}, {bx + a - 2.f, cy - 30.f}};
            for (auto const& d : dots) {
                float const r = std::hypot(x - d[0], y - d[1]);
                if (r < 3.4f) return col;
                if (r < 4.6f) return edge;
            }
            // the tab with the symbol
            float const tx = fx + a - 1.f;
            float const tr = std::hypot(x - tx, y - cy);
            if (x >= fx + a - 8.f && tr < 16.f) {
                if (portalGlyph(mode, x - (tx + 4.f), y - cy)) return robot ? Px{0.16f, 0.16f, 0.2f, 1.f} : white;
                if (tr < 14.6f) return shaded(1.f - 0.18f * std::clamp((cy - y) / 14.f, -1.f, 1.f) * 0.5f);
                return edge;
            }
            // the front ring
            float const ex = (x - fx) / a, ey = (y - cy) / b;
            float const e = std::sqrt(ex * ex + ey * ey);
            if (e < 1.08f && e >= 1.f) return edge;
            if (e < 1.f && e >= 0.79f) {
                float const lift = 1.12f - 0.28f * std::clamp((x - fx + a) / (2.f * a), 0.f, 1.f) + 0.1f * std::clamp(ey, -1.f, 1.f);
                return shaded(lift);
            }
            if (e < 0.79f && e >= 0.73f) return edge;
            // the drum (and what you see of it through the ring): black, a blue grille
            bool inDrum = false;
            if (std::fabs(y - cy) < b) {
                float const w = a * std::sqrt(1.f - ((y - cy) / b) * ((y - cy) / b));
                inDrum = x >= fx - w && x <= bx + w;
                if (inDrum && x > bx + w - 1.6f) return Px{0.02f, 0.02f, 0.05f, 1.f}; // its back edge
            }
            if (inDrum || e < 0.73f) {
                float const row = std::fmod(y + 200.f, 8.f);
                float const off = std::fmod(std::floor((y + 200.f) / 8.f), 2.f) * 4.f;
                bool const dash = row > 2.4f && row < 5.f && std::fmod(x + off + 200.f, 9.f) < 5.6f;
                float const depth = e < 0.73f ? 0.75f : 1.f;
                return dash ? Px{0.24f * depth, 0.52f * depth, 1.f * depth, 1.f} : Px{0.03f, 0.03f, 0.06f, 1.f};
            }
            return Px{1.f, 1.f, 1.f, 0.f};
        }, 3);
    }
    return s_portals[mode];
}

// The galaxy "O": a ring galaxy (bright blue-white inner edge, violet and pink
// dust, stars) with a small warm core. RING_* in px of a 128 texture.
constexpr float RING_TEX = 128.f, RING_OUT = 61.f, RING_IN = 39.f;
inline CCTexture2D* s_ring = nullptr;
inline CCTexture2D* galaxyRing() {
    if (!s_ring) {
        s_ring = bake(128, 128, [](float x, float y) {
            float const dx = x - 64.f, dy = y - 64.f;
            float const r = std::hypot(dx, dy);
            float const th = std::atan2(dy, dx);
            Px out{1.f, 1.f, 1.f, 0.f};
            // the core: a warm glow in the middle
            float const core = std::exp(-(r / 7.f) * (r / 7.f)) * 0.9f;
            if (r < RING_IN) {
                out = Px{1.f, 0.9f, 0.82f, core};
                return out;
            }
            if (r > RING_OUT) return out;
            float const u = (r - RING_IN) / (RING_OUT - RING_IN);         // 0 inner .. 1 outer edge
            float const sp = th + 2.4f * u;                               // spiral
            float const arms = 0.55f + 0.45f * std::cos(3.f * sp);
            float const n = fbm(std::cos(sp) * 3.f + u * 2.f, std::sin(sp) * 3.f + u * 2.f, 7);
            float const n2 = fbm(dx * 0.09f, dy * 0.09f, 31);
            // colours: blue-white inner edge -> violet -> pink dust outside
            float const t = std::clamp(u * 0.9f + (n - 0.5f) * 0.5f, 0.f, 1.f);
            float cr, cg, cb;
            if (t < 0.45f) { float const k = t / 0.45f; cr = 0.75f + (0.55f - 0.75f) * k; cg = 0.82f + (0.4f - 0.82f) * k; cb = 1.f; }
            else { float const k = (t - 0.45f) / 0.55f; cr = 0.55f + (0.93f - 0.55f) * k; cg = 0.4f + (0.36f - 0.4f) * k; cb = 1.f + (0.86f - 1.f) * k; }
            float const bright = std::clamp(0.62f + 0.38f * arms * (0.6f + 0.8f * n) - 0.18f * n2, 0.35f, 1.15f);
            float const edgeLift = 1.f + 0.35f * std::exp(-(u / 0.12f) * (u / 0.12f));  // the bright inner rim
            cr *= bright * edgeLift; cg *= bright * edgeLift; cb *= bright * edgeLift;
            // stars in the ring
            int const sx = static_cast<int>(std::floor(x / 6.f)), sy = static_cast<int>(std::floor(y / 6.f));
            if (hash2(sx, sy, 99) > 0.8f) {
                float const cx = (static_cast<float>(sx) + 0.2f + 0.6f * hash2(sx, sy, 5)) * 6.f;
                float const cy = (static_cast<float>(sy) + 0.2f + 0.6f * hash2(sx, sy, 6)) * 6.f;
                float const s = std::exp(-((x - cx) * (x - cx) + (y - cy) * (y - cy)) / (0.5f + 0.6f * hash2(sx, sy, 7)));
                cr += s; cg += s; cb += s;
            }
            return Px{std::min(cr, 1.f), std::min(cg, 1.f), std::min(cb, 1.f), 1.f};
        }, 3);
    }
    return s_ring;
}
// Its glow (white, for an additive violet bloom), same scale as galaxyRing
// but 160 px across (16 px of room for the glow round it).
inline CCTexture2D* s_ringGlow = nullptr;
inline CCTexture2D* ringGlow() {
    if (!s_ringGlow) {
        s_ringGlow = bake(160, 160, [](float x, float y) {
            float const r = std::hypot(x - 80.f, y - 80.f);
            float const d = std::max({RING_IN - r, r - RING_OUT, 0.f});
            return Px{1.f, 1.f, 1.f, std::exp(-(d / 7.f) * (d / 7.f))};
        }, 1);
    }
    return s_ringGlow;
}

// The spider's teleport streak, like GD's: a white flame that's thin where it
// left and wide where it arrived (the top, here; flipped when it goes down).
inline CCTexture2D* s_tpStreak = nullptr;
inline CCTexture2D* tpStreak() {
    if (!s_tpStreak) {
        s_tpStreak = bake(32, 128, [](float x, float y) {
            float const t = std::clamp(y / 128.f, 0.f, 1.f);                    // 0 start .. 1 end
            float const hw = 1.2f + 11.f * std::pow(t, 0.7f) * (1.f - std::pow(t, 14.f) * 0.6f);
            float const d = std::fabs(x - 16.f) - hw;
            float const a = std::clamp(0.5f - d / 1.6f, 0.f, 1.f) * (0.55f + 0.45f * t);
            return Px{1.f, 1.f, 1.f, a};
        }, 2);
    }
    return s_tpStreak;
}

// GD 1.0's background (Every End's): big squares with thin edges, some a
// little lighter than others. 128 px = 4 x 4 squares; tinted when used.
inline CCTexture2D* s_bgTile = nullptr;
inline CCTexture2D* bgTile() {
    if (!s_bgTile) {
        // big panels (some split in two) with gaps between them, each a little
        // lighter than the background, lighter still at its top edge - GD's
        // first background, the one Every End uses
        s_bgTile = bake(128, 128, [](float x, float y) {
            int const cx = static_cast<int>(x / 64.f), cy = static_cast<int>(y / 64.f);
            float lx = std::fmod(x, 64.f), ly = std::fmod(y, 64.f);
            // some panels are two halves
            float const split = hash2(cx, cy, 31);
            if (split > 0.55f && lx > 31.f) lx -= 32.f;
            float const w = split > 0.55f ? 32.f : 64.f;
            float const gap = 2.5f;
            bool const in = lx > gap && lx < w - gap && ly > gap && ly < 64.f - gap;
            if (!in) return Px{1.f, 1.f, 1.f, 0.f};
            float const fill = 0.14f + 0.14f * hash2(cx * 2 + (lx > 32.f ? 1 : 0), cy, 77);
            float const top = std::clamp(1.f - (64.f - gap - ly) / 3.f, 0.f, 1.f) * 0.12f; // a lighter top edge
            return Px{1.f, 1.f, 1.f, fill + top};
        }, 2);
    }
    return s_bgTile;
}

// A line with smooth edges (stretched along x): Nova Run's outlines. GD's own
// drawn lines have no anti-aliasing - diagonal edges came out as stairs.
// 8 x 16 px: solid from row 3 to 12, a soft px on each side.
inline CCTexture2D* s_lineTex = nullptr;
inline CCTexture2D* lineTex() {
    if (!s_lineTex) {
        s_lineTex = bake(8, 16, [](float, float y) {
            float const d = std::fabs(y - 8.f);                       // from the middle
            return Px{1.f, 1.f, 1.f, std::clamp(6.f - d, 0.f, 1.f)};  // solid to 5 px out, a soft edge after
        }, 3);
    }
    return s_lineTex;
}

inline void reset() {
    for (CCTexture2D** t : {&s_glowEdge, &s_soft, &s_sparkle, &s_gear, &s_ring, &s_ringGlow}) {
        if (*t) (*t)->release();
        *t = nullptr;
    }
    if (s_tpStreak) s_tpStreak->release();
    s_tpStreak = nullptr;
    if (s_lineTex) s_lineTex->release();
    s_lineTex = nullptr;
    if (s_bgTile) s_bgTile->release();
    s_bgTile = nullptr;
    for (auto*& t : s_portals) {
        if (t) t->release();
        t = nullptr;
    }
}

inline CCSprite* sprite(CCTexture2D* t, float size, ccColor3B col = {255, 255, 255}, GLubyte op = 255) {
    if (!t) return nullptr;
    auto* s = CCSprite::createWithTexture(t);
    float const big = std::max(t->getContentSize().width, t->getContentSize().height);
    s->setScale(size / std::max(0.01f, big));
    s->setColor(col);
    s->setOpacity(op);
    return s;
}
inline void additive(CCSprite* s) {
    if (s) s->setBlendFunc(ccBlendFunc{GL_SRC_ALPHA, GL_ONE});
}

// A card's outline glow: sized to a w x h rounded box (corner r) at (x, y),
// spreading round it. Returns the node (anchored bottom-left, positioned).
inline CCNode* glowBox(float x, float y, float w, float h, float r, ccColor3B col, GLubyte op) {
    CCTexture2D* t = glowEdge();
    if (!t) return nullptr;
    float const tp = aa::pts(static_cast<float>(GLOW_TEX));
    float const k = r / aa::pts(static_cast<float>(GLOW_R));   // texture px corners -> r
    float const spread = aa::pts(static_cast<float>(GLOW_S)) * k;
    float const cap = aa::pts(static_cast<float>(GLOW_S + GLOW_R));
    auto* frame = CCSpriteFrame::createWithTexture(t, CCRect(0.f, 0.f, tp, tp));
    auto* s = extension::CCScale9Sprite::createWithSpriteFrame(frame, CCRect(cap, cap, tp - 2.f * cap, tp - 2.f * cap));
    if (!s) return nullptr;
    s->setContentSize({(w + 2.f * spread) / k, (h + 2.f * spread) / k});
    s->setScale(k);
    s->setAnchorPoint({0.f, 0.f});
    s->setPosition({x - spread, y - spread});
    s->setColor(col);
    s->setOpacity(op);
    return s;
}

// ---- bloom: a blurred copy of any node (the neon glow) ------------------------
// Draws `n` into a texture (w x h points, the node placed by its own position),
// blurs it, and returns a white sprite of the blur (alpha only), same size.
inline void blurAlpha(std::vector<float>& a, int w, int h, float sigma) {
    if (sigma < 0.3f) return;
    int const rad = std::max(1, static_cast<int>(std::ceil(sigma * 2.6f)));
    std::vector<float> k(static_cast<size_t>(2 * rad + 1));
    float sum = 0.f;
    for (int i = -rad; i <= rad; i++) { float const v = std::exp(-0.5f * static_cast<float>(i * i) / (sigma * sigma)); k[static_cast<size_t>(i + rad)] = v; sum += v; }
    for (auto& v : k) v /= sum;
    std::vector<float> tmp(a.size());
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float s = 0.f;
            for (int i = -rad; i <= rad; i++) {
                int const xx = std::clamp(x + i, 0, w - 1);
                s += a[static_cast<size_t>(y * w + xx)] * k[static_cast<size_t>(i + rad)];
            }
            tmp[static_cast<size_t>(y * w + x)] = s;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float s = 0.f;
            for (int i = -rad; i <= rad; i++) {
                int const yy = std::clamp(y + i, 0, h - 1);
                s += tmp[static_cast<size_t>(yy * w + x)] * k[static_cast<size_t>(i + rad)];
            }
            a[static_cast<size_t>(y * w + x)] = s;
        }
    }
}
inline CCSprite* bloom(CCNode* n, float w, float h, float sigma, float gain = 1.6f) {
    int const iw = std::max(4, static_cast<int>(std::ceil(w))), ih = std::max(4, static_cast<int>(std::ceil(h)));
    auto* rt = CCRenderTexture::create(iw, ih, kCCTexture2DPixelFormat_RGBA8888);
    if (!rt) return nullptr;
    rt->beginWithClear(0.f, 0.f, 0.f, 0.f);
    n->visit();
    rt->end();
    CCImage* img = rt->newCCImage(true);
    if (!img) return nullptr;
    int const pw = img->getWidth(), ph = img->getHeight();
    unsigned char const* d = img->getData();
    if (!d || pw <= 0 || ph <= 0) { img->release(); return nullptr; }
    std::vector<float> a(static_cast<size_t>(pw * ph));
    for (int i = 0; i < pw * ph; i++) a[static_cast<size_t>(i)] = d[i * 4 + 3] / 255.f;
    img->release();
    blurAlpha(a, pw, ph, sigma * static_cast<float>(pw) / static_cast<float>(iw));
    std::vector<uint8_t> px(static_cast<size_t>(pw * ph * 4));
    for (int i = 0; i < pw * ph; i++) {
        px[static_cast<size_t>(i * 4)] = 255; px[static_cast<size_t>(i * 4 + 1)] = 255; px[static_cast<size_t>(i * 4 + 2)] = 255;
        px[static_cast<size_t>(i * 4 + 3)] = static_cast<uint8_t>(std::lround(255.f * std::clamp(a[static_cast<size_t>(i)] * gain, 0.f, 1.f)));
    }
    auto* t = new CCTexture2D();
    if (!t->initWithData(px.data(), kCCTexture2DPixelFormat_RGBA8888, static_cast<unsigned>(pw), static_cast<unsigned>(ph),
                         CCSize(static_cast<float>(pw), static_cast<float>(ph)))) {
        t->release();
        return nullptr;
    }
    t->setAntiAliasTexParameters();
    auto* s = CCSprite::createWithTexture(t);
    t->release(); // the sprite keeps it
    if (!s) return nullptr;
    s->setScale(static_cast<float>(iw) / std::max(1.f, s->getContentSize().width)); // back to w x h points
    return s;
}

// ---- your icons ----------------------------------------------------------------
inline int iconId(IconType t) {
    auto* gm = GameManager::sharedState();
    if (!gm) return 1;
    switch (t) {
        case IconType::Ship: return gm->getPlayerShip();
        case IconType::Ball: return gm->getPlayerBall();
        case IconType::Ufo: return gm->getPlayerBird();
        case IconType::Wave: return gm->getPlayerDart();
        case IconType::Robot: return gm->getPlayerRobot();
        case IconType::Spider: return gm->getPlayerSpider();
        case IconType::Swing: return gm->getPlayerSwing();
        default: return gm->getPlayerFrame();
    }
}
// Your icon for a gamemode, in your colours (and glow). The ship and the UFO
// carry your cube, like in the game.
inline SimplePlayer* playerIcon(IconType type, float scale) {
    auto* gm = GameManager::sharedState();
    if (!gm) return nullptr;
    auto* p = SimplePlayer::create(1);
    if (!p) return nullptr;
    ccColor3B const c1 = gm->colorForIdx(gm->getPlayerColor()), c2 = gm->colorForIdx(gm->getPlayerColor2());
    p->updatePlayerFrame(iconId(type), type);
    p->setColor(c1);
    p->setSecondColor(c2);
    if (gm->getPlayerGlow()) p->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
    if (type == IconType::Ship || type == IconType::Ufo) {
        if (auto* rider = SimplePlayer::create(1)) {
            rider->updatePlayerFrame(gm->getPlayerFrame(), IconType::Cube);
            rider->setColor(c1);
            rider->setSecondColor(c2);
            rider->setScale(0.55f);
            rider->setPosition({0.f, type == IconType::Ship ? 5.5f : 5.f});
            p->addChild(rider, -1);
        }
    }
    p->setScale(scale);
    return p;
}
// Changes when you pick another icon or colour in the garage.
inline long long iconSig() {
    auto* gm = GameManager::sharedState();
    if (!gm) return 0;
    long long h = 17;
    for (int v : {gm->getPlayerFrame(), gm->getPlayerShip(), gm->getPlayerBall(), gm->getPlayerBird(), gm->getPlayerDart(),
                  gm->getPlayerRobot(), gm->getPlayerSpider(), gm->getPlayerSwing(), gm->getPlayerColor(), gm->getPlayerColor2(),
                  gm->getPlayerGlow() ? 1 : 0, gm->getPlayerGlowColor()}) h = h * 1000003 + v;
    return h;
}
inline ccColor3B playerColor(bool second = false) {
    auto* gm = GameManager::sharedState();
    if (!gm) return {255, 255, 255};
    return gm->colorForIdx(second ? gm->getPlayerColor2() : gm->getPlayerColor());
}

// A rounded rectangle as a stencil / plain shape (jagged edges are fine here).
inline CCDrawNode* roundRectNode(float w, float h, float r) {
    auto* dn = CCDrawNode::create();
    std::vector<CCPoint> pts;
    constexpr int SEG = 8;
    r = std::clamp(r, 0.f, std::min(w, h) / 2.f);
    float const cs[4][3] = {{w - r, h - r, 0.f}, {r, h - r, 90.f}, {r, r, 180.f}, {w - r, r, 270.f}};
    for (auto const& c : cs) {
        for (int j = 0; j <= SEG; j++) {
            float const a = (c[2] + 90.f * static_cast<float>(j) / SEG) * 0.01745329f;
            pts.push_back({c[0] + r * std::cos(a), c[1] + r * std::sin(a)});
        }
    }
    dn->drawPolygon(pts.data(), static_cast<unsigned>(pts.size()), ccColor4F{1.f, 1.f, 1.f, 1.f}, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
    return dn;
}
// Content clipped to a rounded rectangle (falls back to no clipping).
inline CCNode* clipped(float w, float h, float r) {
    if (auto* c = CCClippingNode::create(roundRectNode(w, h, r))) {
        c->setContentSize({w, h});
        return c;
    }
    auto* n = CCNode::create();
    n->setContentSize({w, h});
    return n;
}

struct Rand {
    uint32_t s;
    explicit Rand(uint32_t seed) : s(seed ? seed : 1u) {}
    float f() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return static_cast<float>(s & 0xFFFFFF) / 16777216.f; }
    float r(float a, float b) { return a + (b - a) * f(); }
};

// ---- starfield: slow parallax stars that twinkle, and soft nebulae ------------
class Starfield : public CCNode {
    struct Star { CCSprite* s; float x, y, speed, base, ph, tw; };
    std::vector<Star> m_stars;
    struct Cloud { CCSprite* s; float x, y, base, ph; };
    std::vector<Cloud> m_clouds;
    float m_w = 0.f, m_h = 0.f, m_t = 0.f;
    Rand m_rng{7u};

public:
    // `alpha`: the window's opacity setting; `drift`: how fast the closest stars move
    static Starfield* create(float w, float h, int count, unsigned seed, float alpha, float drift, bool clouds) {
        auto* p = new Starfield();
        if (p->init(w, h, count, seed, alpha, drift, clouds)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init(float w, float h, int count, unsigned seed, float alpha, float drift, bool clouds) {
        if (!CCNode::init()) return false;
        m_w = w; m_h = h;
        m_rng = Rand(seed);
        setContentSize({w, h});
        float const a = std::clamp(alpha, 0.3f, 1.f);
        if (clouds) {
            struct C { float fx, fy, size; ccColor3B c; float op; };
            C const cs[3] = {{0.78f, 0.26f, 1.05f, {104, 66, 214}, 46.f}, {0.18f, 0.74f, 0.85f, {58, 86, 214}, 34.f}, {0.5f, 0.5f, 0.6f, {196, 78, 186}, 18.f}};
            for (auto const& c : cs) {
                float const sz = std::min(w, h) * c.size;
                if (auto* s = sprite(soft(), sz, c.c, 0)) {
                    additive(s);
                    addChild(s, 0);
                    m_clouds.push_back({s, w * c.fx, h * c.fy, c.op * a, m_rng.r(0.f, 6.28f)});
                }
            }
        }
        for (int i = 0; i < count; i++) {
            int const layer = i % 3;                    // far, middle, near
            float const size = layer == 0 ? 0.55f : layer == 1 ? 0.85f : 1.2f;
            float const op = (layer == 0 ? 70.f : layer == 1 ? 110.f : 160.f) * a;
            bool const twinkle = layer == 2 && m_rng.f() < 0.22f;
            CCSprite* s = twinkle ? sprite(sparkle(), size * 3.6f, {235, 228, 255}) : aa::circleSprite(size, {255, 255, 255});
            if (!s) continue;
            float const tint = m_rng.f();
            if (!twinkle) s->setColor(tint < 0.2f ? ccColor3B{255, 228, 196} : tint < 0.5f ? ccColor3B{196, 208, 255} : ccColor3B{246, 242, 255});
            addChild(s, 1);
            m_stars.push_back({s, m_rng.r(0.f, w), m_rng.r(0.f, h), drift * (layer == 0 ? 0.3f : layer == 1 ? 0.6f : 1.f), op,
                               m_rng.r(0.f, 6.28f), m_rng.r(0.6f, 2.2f)});
        }
        step(0.f);
        return true;
    }
    void step(float dt) {
        m_t += dt;
        for (auto& c : m_clouds) {
            float const br = 0.75f + 0.25f * std::sin(m_t * 0.35f + c.ph);
            c.s->setOpacity(static_cast<GLubyte>(std::clamp(c.base * br, 0.f, 255.f)));
            c.s->setPosition({c.x + 5.f * std::sin(m_t * 0.11f + c.ph), c.y + 3.f * std::cos(m_t * 0.13f + c.ph)});
        }
        for (auto& st : m_stars) {
            st.x -= st.speed * dt;
            if (st.x < -2.f) { st.x += m_w + 4.f; st.y = m_rng.r(0.f, m_h); }
            // fades in / out at the left and right edges, twinkles
            float const edge = std::clamp(std::min(st.x, m_w - st.x) / 8.f, 0.f, 1.f);
            float const tw = 0.62f + 0.38f * std::sin(m_t * st.tw + st.ph);
            st.s->setOpacity(static_cast<GLubyte>(std::clamp(st.base * edge * tw, 0.f, 255.f)));
            st.s->setPosition({st.x, st.y});
        }
    }
};

// Play one of GD's robot / spider animations on an icon, only if its sprite
// has one by that name (`cur` remembers what's playing).
inline void playIconAnim(SimplePlayer* icon, std::string& cur, char const* name) {
    if (!icon || cur == name) return;
    CCAnimatedSprite* a = nullptr;
    if (icon->m_robotSprite) a = icon->m_robotSprite;
    else if (icon->m_spiderSprite) a = icon->m_spiderSprite;
    if (!a || !a->m_animationManager || !a->m_animationManager->m_animateDict) return;
    CCDictionary* dict = a->m_animationManager->m_animateDict;
    static bool listed = false; // (once: which animations GD's sprite has, for the log)
    if (!listed) {
        listed = true;
        std::string keys;
        if (CCArray* ks = dict->allKeys()) {
            for (auto* k : CCArrayExt<CCString*>(ks)) if (k) { keys += k->getCString(); keys += ' '; }
        }
        log::info("[Nova] robot / spider animations: {}", keys);
    }
    if (!dict->objectForKey(name)) return;
    a->runAnimation(name);
    cur = name;
}

// ---- Nova Run: the course in the sidebar ---------------------------------------
class CourseView : public CCNode {
public:
    static constexpr float BLK = 6.5f;     // units per block
    static constexpr float GROUND = 7.f;   // floor line, up from the bottom
    static constexpr float LEAD = 3.4f;    // the icon sits this many blocks from the left

    static CourseView* create(float w, float h, float alpha) {
        auto* p = new CourseView();
        if (p->init(w, h, alpha)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }

    // a click at `p` (this node's space): on the icon -> it explodes
    bool clickAt(CCPoint p) {
        if (m_dead > 0.f || !m_icon) return false;
        CCPoint const c = iconPos();
        if (std::hypot(p.x - c.x, p.y - c.y) > BLK * 0.95f) return false;
        explode();
        return true;
    }

    void step(float dt) {
        dt = std::min(dt, 0.05f);
        m_iconCheck -= dt;
        if (m_iconCheck <= 0.f) {
            m_iconCheck = 0.5f;
            if (iconSig() != m_sig) makeIcons();
        }
        float const prevX = m_x;
        if (m_dead > 0.f) {
            m_dead -= dt;
            if (m_dead <= 0.f) respawn();
        } else {
            m_x += ncourse::SPEED * dt;
        }
        m_course.ensureAhead(m_x);
        ncourse::Segment const* seg = m_course.at(m_x);
        if (seg && seg->mode != m_mode) enterMode(seg->mode, m_mode >= 0);
        if (m_dead <= 0.f) fireEvents(prevX, m_x);
        // keep the numbers small (at a portal, where nothing is mid-air)
        if (seg && seg->x0 > 400.f && m_x - seg->x0 < 0.5f) rebase(seg->x0);
        m_cam = m_x - LEAD;
        if (m_world) m_world->setPosition({-m_cam * BLK, 0.f});
        stepBackground(dt);
        drawWorld(dt);
        placeIcon(dt);
    }

private:
    float m_w = 80.f, m_h = 60.f, m_alpha = 1.f;
    ncourse::Course m_course{0xC0FFEEu};
    float m_x = 0.f, m_cam = 0.f, m_dead = 0.f;
    int   m_mode = -1, m_attempt = 1, m_lastMode = -1;
    CCLayerGradient* m_bg = nullptr;
    struct BgLayer { CCNode* node = nullptr; std::vector<CCSprite*> tiles; float size = 40.f, parallax = 0.2f; int cols = 0; };
    BgLayer m_bgLayers[2];
    CCNode* m_world = nullptr;
    CCDrawNode* m_draw = nullptr;
    CCDrawNode* m_trailDraw = nullptr;
    CCNode* m_fx = nullptr;
    CCSprite* m_portals[ncourse::MODE_N] = {};   // one per mode (only one is ever on screen)
    float m_portalFx = 0.f;
    std::string m_anim;                          // robot / spider: the animation playing
    SimplePlayer* m_icons[ncourse::MODE_N] = {};
    SimplePlayer* m_icon = nullptr;
    std::deque<CCPoint> m_trail;          // world units
    std::deque<CCPoint> m_oldWave;        // the wave's line after it went through a portal (scrolls away)
    float m_emit = 0.f, m_iconCheck = 0.f;
    long long m_sig = 0;
    float m_bgTop[3] = {44, 70, 230}, m_bgBot[3] = {20, 30, 150};
    Rand m_rng{0x51u};

    static void bgColors(int m, float top[3], float bot[3]) {
        struct C { float t[3], b[3]; };
        static C const cs[ncourse::MODE_N] = {
            // (2.3) GD's own bright colours, like Every End's sections
            {{44, 70, 230}, {20, 30, 150}},   // cube: blue
            {{150, 40, 200}, {70, 14, 110}},  // ship: purple
            {{200, 46, 90}, {100, 16, 44}},   // ball: crimson
            {{220, 120, 30}, {110, 50, 10}},  // ufo: orange
            {{30, 140, 220}, {10, 60, 120}},  // wave: cyan blue
            {{170, 160, 30}, {80, 74, 10}},   // swing: gold
            {{110, 110, 140}, {44, 44, 64}},  // robot: steel
            {{110, 40, 220}, {46, 14, 110}},  // spider: violet
        };
        for (int i = 0; i < 3; i++) { top[i] = cs[m].t[i]; bot[i] = cs[m].b[i]; }
    }
    static IconType iconType(int m) {
        switch (m) {
            case ncourse::SHIP: return IconType::Ship;
            case ncourse::BALL: return IconType::Ball;
            case ncourse::UFO: return IconType::Ufo;
            case ncourse::WAVE: return IconType::Wave;
            case ncourse::SWING: return IconType::Swing;
            case ncourse::ROBOT: return IconType::Robot;
            case ncourse::SPIDER: return IconType::Spider;
            default: return IconType::Cube;
        }
    }
    static float iconScale(int m) {
        switch (m) {
            case ncourse::SHIP: return 0.2f;
            case ncourse::UFO: return 0.235f;
            case ncourse::WAVE: return 0.2f;
            case ncourse::ROBOT: return BLK / 33.f;
            case ncourse::SPIDER: return BLK / 34.f;
            default: return BLK / 30.f;
        }
    }

    bool init(float w, float h, float alpha) {
        if (!CCNode::init()) return false;
        m_w = w; m_h = h; m_alpha = alpha;
        setContentSize({w, h});
        CCNode* clip = clipped(w, h, 4.5f);
        addChild(clip);
        m_bg = CCLayerGradient::create(ccColor4B{46, 30, 96, 255}, ccColor4B{18, 12, 38, 255});
        m_bg->setContentSize({w, h});
        clip->addChild(m_bg, 0);
        // (2.3) GD 1.0's background in two layers, like Every End's: big
        // squares far back, smaller ones nearer, each sliding at its own speed
        for (int l = 0; l < 2; l++) {
            BgLayer& bl = m_bgLayers[l];
            bl.size = l == 0 ? 52.f : 30.f;
            bl.parallax = l == 0 ? 0.12f : 0.3f;
            bl.node = CCNode::create();
            clip->addChild(bl.node, 1);
            int const cols = static_cast<int>(std::ceil(w / bl.size)) + 2, rows = static_cast<int>(std::ceil(h / bl.size)) + 1;
            for (int i = 0; i < cols * rows; i++) {
                if (auto* t = sprite(bgTile(), bl.size, {255, 255, 255}, l == 0 ? 120 : 70)) {
                    t->setAnchorPoint({0.f, 0.f});
                    bl.node->addChild(t);
                    bl.tiles.push_back(t);
                }
            }
            bl.cols = cols;
        }
        m_world = CCNode::create();
        clip->addChild(m_world, 2);
        for (int m = 0; m < ncourse::MODE_N; m++) {
            // GD's portals are about 3 blocks tall (the ring is 104 of the texture's 144 px)
            CCSprite* p = sprite(portal(m), 3.f * BLK * PORTAL_H / PORTAL_RING_H);
            if (!p) continue;
            p->setAnchorPoint({PORTAL_CX / PORTAL_W, 0.5f});
            p->setVisible(false);
            m_world->addChild(p, 1);
            m_portals[m] = p;
        }
        m_draw = CCDrawNode::create();
        m_world->addChild(m_draw, 2);
        m_lineNode = CCNode::create();
        m_world->addChild(m_lineNode, 3);
        m_trailDraw = CCDrawNode::create();
        m_world->addChild(m_trailDraw, 3);
        m_fx = CCNode::create();
        m_world->addChild(m_fx, 4);
        m_course.start(ncourse::CUBE);
        makeIcons();
        m_x = 0.5f;
        step(0.f);
        return true;
    }

    void makeIcons() {
        for (auto*& ic : m_icons) {
            if (ic) ic->removeFromParent();
            ic = nullptr;
        }
        for (int m = 0; m < ncourse::MODE_N; m++) {
            m_icons[m] = playerIcon(iconType(m), iconScale(m));
            if (m_icons[m]) {
                m_icons[m]->setVisible(false);
                m_world->addChild(m_icons[m], 5);
            }
        }
        m_sig = iconSig();
        m_anim.clear();
        m_icon = m_mode >= 0 ? m_icons[m_mode] : nullptr;
        if (m_icon) m_icon->setVisible(m_dead <= 0.f);
    }

    CCPoint world(float x, float y) const { return {x * BLK, GROUND + y * BLK}; }
    CCPoint iconPos() const {
        ncourse::Segment const* s = m_course.at(m_x);
        float const y = s ? s->yAt(m_x) : 0.5f;
        CCPoint const w = world(m_x, y);
        return {w.x - m_cam * BLK, w.y};
    }

    void enterMode(int m, bool portal) {
        if (m_icon) m_icon->setVisible(false);
        m_mode = m;
        m_icon = m_icons[m];
        m_anim.clear();
        if (m_icon) m_icon->setVisible(m_dead <= 0.f);
        bgColors(m, m_bgTargetTop, m_bgTargetBot);
        if (m_mode != ncourse::WAVE && !m_trail.empty() && m_lastMode == ncourse::WAVE) m_oldWave = m_trail; // (GD keeps it on screen)
        m_lastMode = m;
        m_trail.clear();
        if (!portal) {
            for (int i = 0; i < 3; i++) { m_bgTop[i] = m_bgTargetTop[i]; m_bgBot[i] = m_bgTargetBot[i]; }
            return;
        }
        // through a portal: a ring of its colour bursts out
        ncourse::Segment const* s = m_course.at(m_x);
        CCPoint const at = world(m_x, s ? s->yAt(m_x) : 1.5f);
        if (auto* ring = aa::circleSprite(BLK * 1.2f, portalColor(m))) {
            ring->setPosition(at);
            ring->setOpacity(200);
            float const sc = ring->getScale();
            ring->runAction(CCEaseOut::create(CCScaleTo::create(0.35f, sc * 3.4f), 2.f));
            ring->runAction(CCSequence::create(CCFadeTo::create(0.35f, 0), CCRemoveSelf::create(), nullptr));
            additive(ring);
            m_fx->addChild(ring);
        }
        for (int i = 0; i < 10; i++) {
            float const ang = m_rng.r(0.f, 6.2831853f), dist = m_rng.r(6.f, 14.f);
            square(at, i % 2 ? portalColor(m) : ccColor3B{255, 255, 255}, m_rng.r(0.9f, 1.8f),
                   {std::cos(ang) * dist, std::sin(ang) * dist}, m_rng.r(0.3f, 0.5f));
        }
    }
    float m_bgTargetTop[3] = {44, 70, 230}, m_bgTargetBot[3] = {20, 30, 150};

    void stepBackground(float dt) {
        float const k = std::min(1.f, dt * 3.f);
        for (int i = 0; i < 3; i++) {
            m_bgTop[i] += (m_bgTargetTop[i] - m_bgTop[i]) * k;
            m_bgBot[i] += (m_bgTargetBot[i] - m_bgBot[i]) * k;
        }
        auto c3 = [](float const* c) { return ccColor3B{static_cast<GLubyte>(c[0]), static_cast<GLubyte>(c[1]), static_cast<GLubyte>(c[2])}; };
        if (m_bg) {
            m_bg->setStartColor(c3(m_bgTop));
            m_bg->setEndColor(c3(m_bgBot));
        }
        // the background squares: tinted a little lighter than the section's
        // colour, sliding slower than the level (far ones slowest)
        ccColor3B const tint{static_cast<GLubyte>(std::min(255.f, m_bgTop[0] * 1.3f + 30.f)),
                             static_cast<GLubyte>(std::min(255.f, m_bgTop[1] * 1.3f + 30.f)),
                             static_cast<GLubyte>(std::min(255.f, m_bgTop[2] * 1.3f + 30.f))};
        for (auto& bl : m_bgLayers) {
            if (!bl.node || bl.cols <= 0) continue;
            float const off = std::fmod(m_cam * BLK * bl.parallax, bl.size);
            for (size_t i = 0; i < bl.tiles.size(); i++) {
                int const cx = static_cast<int>(i) % bl.cols, cy = static_cast<int>(i) / bl.cols;
                bl.tiles[i]->setPosition({-off + static_cast<float>(cx) * bl.size, static_cast<float>(cy) * bl.size});
                bl.tiles[i]->setColor(tint);
            }
        }
    }

    // A GD-style square particle (world space): drifts, fades out.
    void square(CCPoint at, ccColor3B c, float size, CCPoint drift, float life, GLubyte alpha = 230) {
        auto* d = CCLayerColor::create(ccColor4B{c.r, c.g, c.b, alpha}, size, size);
        d->setPosition({at.x - size / 2.f, at.y - size / 2.f});
        d->runAction(CCEaseOut::create(CCMoveBy::create(life, drift), 1.6f));
        d->runAction(CCSequence::create(CCFadeTo::create(life, 0), CCRemoveSelf::create(), nullptr));
        m_fx->addChild(d);
    }

    void fireEvents(float from, float to) {
        for (auto const& s : m_course.segs) {
            if (s.x1 < from || s.x0 > to) continue;
            for (auto const& e : s.events) {
                if (e.x <= from || e.x > to) continue;
                CCPoint const at = world(e.x, s.yAt(e.x));
                ccColor3B const c1 = playerColor(), c2 = playerColor(true);
                switch (s.mode) {
                    case ncourse::CUBE:
                        if (e.kind == 2) // landed: a little puff
                            for (int i = 0; i < 5; i++)
                                square({at.x - m_rng.r(-2.f, 3.f), at.y - BLK * 0.45f}, c1, m_rng.r(0.7f, 1.3f),
                                       {-m_rng.r(2.f, 6.f), m_rng.r(1.f, 4.f)}, m_rng.r(0.25f, 0.4f));
                        break;
                    case ncourse::UFO:
                        if (e.kind == 0)
                            for (int i = 0; i < 5; i++)
                                square({at.x - m_rng.r(0.f, 3.f), at.y - BLK * 0.35f}, i % 2 ? c1 : ccColor3B{255, 255, 255},
                                       m_rng.r(0.8f, 1.5f), {-m_rng.r(3.f, 8.f), -m_rng.r(3.f, 7.f)}, m_rng.r(0.3f, 0.5f));
                        break;
                    case ncourse::ROBOT:
                        if (e.kind == 0) // the jump: a burst of fire from its feet
                            for (int i = 0; i < 6; i++)
                                flame({at.x - m_rng.r(-1.5f, 1.5f), at.y - BLK * 0.5f}, m_rng.r(2.f, 3.2f),
                                      {m_rng.r(-3.f, 3.f), -m_rng.r(5.f, 10.f)}, m_rng.r(0.2f, 0.35f));
                        if (e.kind == 2)
                            for (int i = 0; i < 4; i++)
                                square({at.x - m_rng.r(-2.f, 3.f), at.y - BLK * 0.45f}, c1, m_rng.r(0.7f, 1.2f),
                                       {-m_rng.r(2.f, 6.f), m_rng.r(1.f, 3.f)}, m_rng.r(0.25f, 0.4f));
                        break;
                    case ncourse::SPIDER:
                        if (e.kind == 3) {
                            // the teleport: a line of light from where it was to where it is, like GD's
                            float const from = s.yAt(e.x - ncourse::DX), to = s.yAt(e.x + ncourse::DX);
                            CCPoint const a = world(e.x, from), b = world(e.x, to);
                            if (CCTexture2D* tex = tpStreak()) {
                                auto* ln = CCSprite::createWithTexture(tex);
                                float const len = std::max(1.f, std::fabs(b.y - a.y));
                                ln->setAnchorPoint({0.5f, 0.f});
                                ln->setScaleX(BLK * 0.9f / ln->getContentSize().width);
                                ln->setScaleY((b.y > a.y ? 1.f : -1.f) * len / ln->getContentSize().height);
                                ln->setPosition(a);
                                ln->runAction(CCSequence::create(CCDelayTime::create(0.06f), CCFadeTo::create(0.3f, 0), CCRemoveSelf::create(), nullptr));
                                m_fx->addChild(ln);
                            }
                            // a faint ring where it left
                            if (auto* ring = aa::circleSprite(BLK * 0.9f, {255, 255, 255}, 110)) {
                                ring->setPosition(a);
                                ring->runAction(CCSequence::create(CCFadeTo::create(0.35f, 0), CCRemoveSelf::create(), nullptr));
                                m_fx->addChild(ring);
                            }
                            for (int i = 0; i < 6; i++)
                                square({b.x, b.y}, i % 2 ? c2 : ccColor3B{255, 255, 255}, m_rng.r(0.7f, 1.3f),
                                       {-m_rng.r(2.f, 7.f), m_rng.r(-4.f, 4.f)}, m_rng.r(0.25f, 0.4f));
                        }
                        break;
                    default: break;
                }
            }
        }
    }

    void respawn() {
        // back to the start of this section, like practice: "Attempt N"
        ncourse::Segment const* s = m_course.at(m_x);
        if (!s) return;
        float const x0 = s->x0;
        m_x = x0 + 0.01f;
        m_attempt++;
        m_trail.clear();
        m_oldWave.clear();
        m_fx->removeAllChildren();
        if (m_icon) m_icon->setVisible(true);
        auto* l = IBLabel::make(fmt::format("Attempt {}", m_attempt).c_str(), "bigFont.fnt");
        if (l) {
            l->setScale(0.22f);
            l->setAnchorPoint({0.f, 0.5f});
            l->setPosition(world(x0 + 1.2f, ncourse::hasCeiling(s->mode) ? 4.2f : 4.6f));
            l->setOpacity(0);
            l->runAction(CCSequence::create(CCFadeTo::create(0.25f, 230), CCDelayTime::create(2.5f), CCFadeTo::create(0.6f, 0),
                                            CCRemoveSelf::create(), nullptr));
            m_fx->addChild(l);
        }
    }

    void explode() {
        CCPoint const at = m_icon ? m_icon->getPosition() : world(m_x, 1.f);
        m_dead = 1.0f;
        if (m_icon) m_icon->setVisible(false);
        m_trail.clear();
        ccColor3B const c1 = playerColor(), c2 = playerColor(true);
        if (auto* wave = CCCircleWave::create(1.f, BLK * 2.4f, 0.35f, false, true)) {
            wave->m_color = c1;
            wave->setPosition(at);
            m_fx->addChild(wave);
        }
        for (int i = 0; i < 16; i++) {
            float const ang = m_rng.r(0.f, 6.2831853f), dist = m_rng.r(6.f, 15.f);
            ccColor3B const c = i % 3 == 0 ? ccColor3B{255, 255, 255} : (i % 3 == 1 ? c1 : c2);
            square(at, c, m_rng.r(0.9f, 1.9f), {std::cos(ang) * dist, std::sin(ang) * dist}, m_rng.r(0.35f, 0.6f), 255);
        }
        if (auto* fm = FMODAudioEngine::sharedEngine()) fm->playEffect("explode_11.ogg");
    }

    void rebase(float by) {
        m_course.shift(-by);
        m_x -= by;
        m_cam -= by;
        for (auto& p : m_trail) p.x -= by * BLK;
        for (auto& p : m_oldWave) p.x -= by * BLK;
        if (m_fx && m_fx->getChildren()) {
            for (auto* c : CCArrayExt<CCNode*>(m_fx->getChildren())) c->setPositionX(c->getPositionX() - by * BLK);
        }
    }

    // ---- drawing: floor, ceilings, obstacles, portals ----
    static ccColor4F pm(ccColor3B c, float a) { return {c.r / 255.f * a, c.g / 255.f * a, c.b / 255.f * a, a}; }
    // (2.3) smooth lines: a pool of stretched line sprites, laid out each frame
    CCNode* m_lineNode = nullptr;
    std::vector<CCSprite*> m_linePool;
    size_t m_lineUsed = 0;
    void line(CCPoint a, CCPoint b, float r, ccColor4F c) {
        if (!m_lineNode || c.a <= 0.f) return;
        float const dx = b.x - a.x, dy = b.y - a.y;
        float const len = std::hypot(dx, dy);
        if (len < 0.01f) return;
        if (m_lineUsed >= m_linePool.size()) {
            if (m_linePool.size() >= 1200) return;
            CCTexture2D* t = lineTex();
            if (!t) return;
            auto* sp = CCSprite::createWithTexture(t);
            m_lineNode->addChild(sp);
            m_linePool.push_back(sp);
        }
        CCSprite* sp = m_linePool[m_lineUsed++];
        CCSize const cs = sp->getContentSize();
        // the solid part (10 of 16 px) is the line's width; a little extra length for the joins
        sp->setScaleX((len + 2.f * r) / std::max(0.01f, cs.width));
        sp->setScaleY(2.f * r * 16.f / 10.f / std::max(0.01f, cs.height));
        sp->setRotation(-std::atan2(dy, dx) * 57.29578f);
        sp->setPosition({(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f});
        sp->setColor({static_cast<GLubyte>(std::clamp(c.r / c.a, 0.f, 1.f) * 255.f), static_cast<GLubyte>(std::clamp(c.g / c.a, 0.f, 1.f) * 255.f),
                      static_cast<GLubyte>(std::clamp(c.b / c.a, 0.f, 1.f) * 255.f)});
        sp->setOpacity(static_cast<GLubyte>(std::clamp(c.a, 0.f, 1.f) * 255.f));
        sp->setVisible(true);
    }
    // A block of GD 1.0 tiles (Every End's look): black, a white outline round
    // the whole block, faint lines between its tiles.
    void drawBlock(ncourse::Block const& bl) {
        CCPoint const a = world(bl.x, bl.y), b = world(bl.x + bl.w, bl.y + bl.h);
        ccColor3B const fill{static_cast<GLubyte>(m_bgBot[0] * 0.7f), static_cast<GLubyte>(m_bgBot[1] * 0.7f), static_cast<GLubyte>(m_bgBot[2] * 0.75f)};
        quad(a, {b.x, a.y}, b, {a.x, b.y}, pm(fill, 1.f));
        ccColor3B const lite{static_cast<GLubyte>(std::min(255.f, m_bgTop[0] + 90.f)), static_cast<GLubyte>(std::min(255.f, m_bgTop[1] + 90.f)),
                             static_cast<GLubyte>(std::min(255.f, m_bgTop[2] + 90.f))};
        ccColor4F const grid = pm(lite, 0.45f);
        for (float cx = 1.f; cx < bl.w - 0.01f; cx += 1.f) line(world(bl.x + cx, bl.y), world(bl.x + cx, bl.y + bl.h), 0.17f, grid);
        for (float cy = 1.f; cy < bl.h - 0.01f; cy += 1.f) line(world(bl.x, bl.y + cy), world(bl.x + bl.w, bl.y + cy), 0.17f, grid);
        // the outline glows (a wide faint line under the white one)
        ccColor4F const glow = pm(lite, 0.28f);
        ccColor4F const e = pm({250, 246, 255}, 0.98f);
        for (auto const& [r, c] : {std::pair<float, ccColor4F>{0.95f, glow}, std::pair<float, ccColor4F>{0.3f, e}}) {
            line(a, {b.x, a.y}, r, c); line({b.x, a.y}, b, r, c); line(b, {a.x, b.y}, r, c); line({a.x, b.y}, a, r, c);
        }
    }
    void quad(CCPoint a, CCPoint b, CCPoint c, CCPoint d, ccColor4F col) {
        CCPoint pts[4] = {a, b, c, d};
        m_draw->drawPolygon(pts, 4, col, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
    }
    void drawSpike(ncourse::Spike const& s) {
        float const tipY = s.down ? s.base - 1.f : s.base + 1.f;
        CCPoint const a = world(s.x + 0.06f, s.base), b = world(s.x + 0.94f, s.base), t = world(s.x + 0.5f, tipY);
        CCPoint pts[3] = {a, b, t};
        m_draw->drawPolygon(pts, 3, ccColor4F{0.02f, 0.016f, 0.04f, 1.f}, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
        ccColor4F const g = pm({255, 255, 255}, 0.22f); // a soft glow, then the white edge (like the game's)
        line(a, t, 0.85f, g); line(t, b, 0.85f, g);
        ccColor4F const e = pm({250, 246, 255}, 0.98f);
        line(a, t, 0.28f, e); line(t, b, 0.28f, e); line(b, a, 0.2f, pm({240, 236, 255}, 0.5f));
    }

    void drawWorld(float dt) {
        if (!m_draw) return;
        m_draw->clear();
        m_lineUsed = 0;
        struct HideRest { CourseView* v; ~HideRest() { for (size_t i = v->m_lineUsed; i < v->m_linePool.size(); i++) v->m_linePool[i]->setVisible(false); } } hideRest{this};
        float const L = m_cam - 1.5f, R = m_cam + m_w / BLK + 1.5f;
        float const top = (m_h - GROUND) / BLK;        // the view's top, in blocks above the floor
        ccColor3B const groundCol{static_cast<GLubyte>(m_bgBot[0] * 0.55f), static_cast<GLubyte>(m_bgBot[1] * 0.55f), static_cast<GLubyte>(m_bgBot[2] * 0.6f)};
        // the floor: a dark strip, faint tiles, a bright line with a glow
        quad(world(L, -2.f), world(R, -2.f), world(R, 0.f), world(L, 0.f), pm(groundCol, 1.f));
        for (float gx = std::floor(L / 2.f) * 2.f; gx < R; gx += 2.f) line(world(gx, -2.f), world(gx, 0.f), 0.14f, pm({255, 255, 255}, 0.06f));
        line(world(L, 0.f), world(R, 0.f), 1.1f, pm({200, 170, 255}, 0.12f));
        line(world(L, 0.f), world(R, 0.f), 0.35f, pm({255, 255, 255}, 0.85f));
        for (auto const& s : m_course.segs) {
            if (s.x1 < L || s.x0 > R) continue;
            float const a = std::max(L, s.x0), b = std::min(R, s.x1);
            {   // (2.3) a top always - the cube and the robot just never reach it
                quad(world(a, ncourse::CEIL), world(b, ncourse::CEIL), world(b, top + 1.f), world(a, top + 1.f), pm(groundCol, 1.f));
                for (float gx = std::floor(a / 2.f) * 2.f; gx < b; gx += 2.f)
                    if (gx >= a) line(world(gx, ncourse::CEIL), world(gx, top + 1.f), 0.14f, pm({255, 255, 255}, 0.06f));
                line(world(a, ncourse::CEIL), world(b, ncourse::CEIL), 1.1f, pm({200, 170, 255}, 0.12f));
                line(world(a, ncourse::CEIL), world(b, ncourse::CEIL), 0.35f, pm({255, 255, 255}, 0.85f));
            }
            for (auto const& bl : s.blocks) {
                if (bl.x + bl.w < L || bl.x > R) continue;
                drawBlock(bl);
            }
            for (auto const& sp : s.spikes) if (sp.x + 1.f >= L && sp.x <= R) drawSpike(sp);
            if (!s.wallLo.empty()) drawWaveWalls(s, L, R);
        }
        // portals: the oval at the end of a section, in the next mode's colour
        bool shown[ncourse::MODE_N] = {};
        for (auto const& s : m_course.segs) {
            if (s.nextMode < 0 || s.nextMode >= ncourse::MODE_N || s.x1 < L - 1.f || s.x1 > R + 1.f) continue;
            CCSprite* p = m_portals[s.nextMode];
            if (!p || shown[s.nextMode]) continue;
            shown[s.nextMode] = true;
            p->setVisible(true);
            p->setPosition(world(s.x1, s.portalY));
            // GD's portals give off squares of their colour
            m_portalFx -= dt;
            if (m_portalFx <= 0.f && m_dead <= 0.f) {
                m_portalFx = 0.06f;
                CCPoint const at = p->getPosition();
                square({at.x + m_rng.r(-2.f, 6.f), at.y + m_rng.r(-BLK * 1.3f, BLK * 1.3f)}, portalColor(s.nextMode), m_rng.r(0.7f, 1.4f),
                       {-m_rng.r(3.f, 9.f), m_rng.r(-2.f, 2.f)}, m_rng.r(0.4f, 0.7f), 200);
            }
        }
        for (int m = 0; m < ncourse::MODE_N; m++) if (m_portals[m] && !shown[m]) m_portals[m]->setVisible(false);
    }

    void drawWaveWalls(ncourse::Segment const& s, float L, float R) {
        int const n = s.count();
        int const i0 = std::max(0, static_cast<int>((L - s.x0) / ncourse::DX));
        int const i1 = std::min(n - 1, static_cast<int>((R - s.x0) / ncourse::DX) + 1);
        constexpr int STEP = 2;
        ccColor4F const fill{0.02f, 0.016f, 0.04f, 1.f};
        ccColor4F const edge = pm({240, 236, 255}, 0.9f);
        // a straight run of the edge is one smooth line (the slope only changes where the wave turned)
        struct Run { bool on = false; CCPoint a, b; float slope = 0.f; };
        Run lo, hi;
        auto feed = [&](Run& r, bool have, CCPoint a, CCPoint b) {
            float const sl = b.x - a.x > 0.001f ? (b.y - a.y) / (b.x - a.x) : 0.f;
            if (r.on && (!have || std::fabs(sl - r.slope) > 0.02f)) { line(r.a, r.b, 0.3f, edge); r.on = false; }
            if (!have) return;
            if (!r.on) { r.on = true; r.a = a; r.slope = sl; }
            r.b = b;
        };
        for (int i = i0; i + STEP <= i1; i += STEP) {
            float const xa = s.x0 + static_cast<float>(i) * ncourse::DX, xb = s.x0 + static_cast<float>(i + STEP) * ncourse::DX;
            float const la = s.wallLo[static_cast<size_t>(i)], lb = s.wallLo[static_cast<size_t>(i + STEP)];
            float const ha = s.wallHi[static_cast<size_t>(i)], hb = s.wallHi[static_cast<size_t>(i + STEP)];
            bool const haveLo = la >= 0.f && lb >= 0.f, haveHi = ha >= 0.f && hb >= 0.f;
            if (haveLo) quad(world(xa, 0.f), world(xb, 0.f), world(xb, lb), world(xa, la), fill);
            if (haveHi) quad(world(xa, ha), world(xb, hb), world(xb, ncourse::CEIL), world(xa, ncourse::CEIL), fill);
            feed(lo, haveLo, world(xa, la), world(xb, lb));
            feed(hi, haveHi, world(xa, ha), world(xb, hb));
        }
        feed(lo, false, {}, {});
        feed(hi, false, {}, {});
    }

    // ---- the icon, its trail and particles (like GD, 2.2) ----
    //  cube   no trail; little squares off the ground while it runs, a puff when it lands
    //  ship   your trail (colour 2) and, while you hold, fire out of its back
    //  ball   no trail; squares off whichever surface it rolls on
    //  ufo    your trail; a puff of squares under it on every click
    //  wave   the solid wave trail (colour 2, GD's default), into the back of the arrow
    //  swing  your trail, and fire on the side it's being pushed from
    //  robot  no trail; runs (animated), fire under its feet while the jump is held
    //  spider no trail; runs (animated, upside down on the ceiling), a line of light when it teleports
    void flame(CCPoint at, float size, CCPoint drift, float life) {
        auto* f = sprite(soft(), size, {255, 236, 170}, 255);
        if (!f) return;
        additive(f);
        f->setPosition(at);
        float const sc = f->getScale();
        f->runAction(CCEaseOut::create(CCMoveBy::create(life, drift), 1.5f));
        f->runAction(CCScaleTo::create(life, sc * 0.3f));
        f->runAction(CCSequence::create(CCTintTo::create(life * 0.4f, 255, 140, 50), CCTintTo::create(life * 0.6f, 200, 50, 60), nullptr));
        f->runAction(CCSequence::create(CCFadeTo::create(life, 0), CCRemoveSelf::create(), nullptr));
        m_fx->addChild(f);
    }
    // robot / spider: play one of GD's animations on it (only if the sprite has it)
    void playAnim(char const* name) { playIconAnim(m_icon, m_anim, name); }
    void placeIcon(float dt) {
        if (!m_icon) return;
        ncourse::Segment const* s = m_course.at(m_x);
        if (!s) return;
        float const y = s->yAt(m_x);
        float const rot = s->rotAt(m_x);
        bool const grounded = s->groundedAt(m_x), held = s->heldAt(m_x);
        bool const onCeiling = ncourse::hasCeiling(m_mode) && y > ncourse::CEIL / 2.f;
        CCPoint const p = world(m_x, y);
        m_icon->setPosition(p);
        m_icon->setRotation(rot);
        float const sc = iconScale(m_mode);
        m_icon->setScaleX(sc);
        m_icon->setScaleY(m_mode == ncourse::SPIDER && onCeiling ? -sc : sc); // the spider walks the ceiling upside down
        m_trailDraw->clear();
        ccColor3B const c1 = playerColor(), c2 = playerColor(true);
        if (!m_oldWave.empty()) {
            while (m_oldWave.size() > 1 && m_oldWave[1].x < m_cam * BLK - 4.f) m_oldWave.pop_front();
            if (m_oldWave.size() < 2) m_oldWave.clear();
            for (size_t i = 1; i < m_oldWave.size(); i++) m_trailDraw->drawSegment(m_oldWave[i - 1], m_oldWave[i], m_waveR, pm(c2, 1.f));
        }
        if (m_dead > 0.f) return;
        m_emit -= dt;
        bool const emit = m_emit <= 0.f;
        if (emit) m_emit = 0.03f;
        float const surfaceY = onCeiling ? p.y + BLK * 0.47f : p.y - BLK * 0.47f;
        auto groundBits = [&](ccColor3B c) {
            if (emit && grounded)
                square({p.x - BLK * 0.45f, surfaceY}, c, m_rng.r(0.6f, 1.2f),
                       {-m_rng.r(2.f, 6.f), (onCeiling ? -1.f : 1.f) * m_rng.r(0.5f, 3.5f)}, m_rng.r(0.25f, 0.4f));
        };
        switch (m_mode) {
            case ncourse::CUBE:
                groundBits(c1);
                break;
            case ncourse::BALL:
                groundBits(c1);
                m_trail.clear();
                break;
            case ncourse::SHIP:
                streak(p, c2);
                if (held && emit) // the ship's fire, out of its back while you hold
                    for (int i = 0; i < 2; i++)
                        flame({p.x - BLK * 0.6f, p.y - BLK * 0.12f}, m_rng.r(1.8f, 3.f), {-m_rng.r(6.f, 11.f), m_rng.r(-2.5f, 1.f)},
                              m_rng.r(0.16f, 0.28f));
                break;
            case ncourse::UFO:
                streak(p, c2);
                break;
            case ncourse::SWING:
                streak(p, c2);
                if (emit) // pushed up: fire below it; pushed down: fire above it
                    flame({p.x - BLK * 0.15f, p.y + (held ? -1.f : 1.f) * BLK * 0.5f}, m_rng.r(1.6f, 2.6f),
                          {-m_rng.r(3.f, 6.f), (held ? -1.f : 1.f) * m_rng.r(3.f, 6.f)}, m_rng.r(0.15f, 0.25f));
                break;
            case ncourse::ROBOT:
                playAnim(grounded ? "run" : "fall_loop");
                groundBits(c1);
                if (held && emit) // the held jump's fire under its feet
                    for (int i = 0; i < 2; i++)
                        flame({p.x + m_rng.r(-1.5f, 1.f), p.y - BLK * 0.55f}, m_rng.r(1.8f, 3.f), {m_rng.r(-3.f, 1.f), -m_rng.r(5.f, 9.f)},
                              m_rng.r(0.15f, 0.26f));
                break;
            case ncourse::SPIDER:
                playAnim("run");
                groundBits(c1);
                break;
            case ncourse::WAVE: {
                // GD's wave: a solid line (colour 2 by default), into the back of the arrow
                m_trail.push_back(p);
                while (m_trail.size() > 2 && m_trail.front().x < p.x - (m_w + 10.f)) m_trail.pop_front();
                float tw = 1.4f;
                if (m_icon->m_firstLayer) tw = std::clamp(m_icon->m_firstLayer->boundingBox().size.height * sc * 0.24f, 0.8f, 2.f);
                m_waveR = tw * 0.5f;
                for (size_t i = 1; i < m_trail.size(); i++) m_trailDraw->drawSegment(m_trail[i - 1], m_trail[i], m_waveR, pm(c2, 1.f));
                if (m_icon->m_firstLayer) {
                    CCRect const bb = m_icon->m_firstLayer->boundingBox();
                    float const lx = bb.origin.x + bb.size.width * 0.2f, ly = bb.getMidY();
                    float const r = rot * 0.01745329f;
                    m_icon->setPosition({p.x - (lx * std::cos(r) + ly * std::sin(r)) * sc, p.y - (-lx * std::sin(r) + ly * std::cos(r)) * sc});
                }
                break;
            }
            default: break;
        }
    }
    float m_waveR = 0.7f;
    // GD's player trail: your second colour, bright at the icon, fading behind it
    void streak(CCPoint p, ccColor3B c) {
        m_trail.push_back(p);
        float const len = 0.3f * ncourse::SPEED * BLK;
        while (m_trail.size() > 2 && m_trail.front().x < p.x - len) m_trail.pop_front();
        size_t const n = m_trail.size();
        for (size_t i = 1; i < n; i++) {
            float const t = static_cast<float>(i) / static_cast<float>(n);
            m_trailDraw->drawSegment(m_trail[i - 1], m_trail[i], BLK * 0.2f * (0.3f + 0.7f * t), pm(c, 0.75f * t));
        }
    }
};

// ---- the robot (easter egg) --------------------------------------------------------
// Your robot drops in at the left of the page and jumps from button to button
// (and along the tops of the cards) across to the right, GD's held-jump fire
// under its feet on the way up; a button bounces when it lands on it. Then it
// jumps off the right edge. Click it and it explodes.
struct Perch {
    float x0, x1, y;          // the top edge it can stand on (parent space)
    Ref<CCNode> node;         // bounces when it lands (a button), or none (a card)
};
class RobotHop : public CCNode {
public:
    static RobotHop* create(CCNode* fxParent, std::vector<Perch> const& perches, float startX, float topY, float exitX,
                            float bottomY) {
        auto* p = new RobotHop();
        if (p->init(fxParent, perches, startX, topY, exitX, bottomY)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool done() const { return m_done; }
    // `p` in the parent's space
    bool hit(CCPoint p) {
        if (m_done) return false;
        CCPoint const c = getPosition();
        return std::fabs(p.x - c.x) < 8.f && p.y > c.y - 1.f && p.y < c.y + 2.f * m_half + 2.f;
    }
    void blowUp() {
        if (m_done) return;
        CCPoint const at{getPositionX(), getPositionY() + m_half};
        ccColor3B const c1 = playerColor(), c2 = playerColor(true);
        if (auto* wave = CCCircleWave::create(1.f, 16.f, 0.35f, false, true)) {
            wave->m_color = c1;
            wave->setPosition(at);
            m_fxParent->addChild(wave, 58);
        }
        for (int i = 0; i < 18; i++) {
            float const ang = m_rng.r(0.f, 6.2831853f), dist = m_rng.r(8.f, 20.f);
            ccColor3B const c = i % 3 == 0 ? ccColor3B{255, 255, 255} : (i % 3 == 1 ? c1 : c2);
            square(at, c, m_rng.r(1.f, 2.2f), {std::cos(ang) * dist, std::sin(ang) * dist}, m_rng.r(0.35f, 0.6f));
        }
        if (auto* fm = FMODAudioEngine::sharedEngine()) fm->playEffect("explode_11.ogg");
        finish();
    }

    void update(float dt) override {
        if (m_done) return;
        dt = std::min(dt, 0.05f);
        if (m_i >= m_moves.size()) { finish(); return; }
        Move const& mv = m_moves[m_i];
        m_t += dt;
        float const t = std::min(m_t, mv.T);
        float const k = mv.T > 0.f ? t / mv.T : 1.f;
        float const x = mv.a.x + (mv.b.x - mv.a.x) * k;
        float y = mv.b.y;
        if (mv.jump) {
            y = t < mv.t1 ? mv.apex - 0.5f * mv.g * (mv.t1 - t) * (mv.t1 - t) : mv.apex - 0.5f * mv.g * (t - mv.t1) * (t - mv.t1);
            playIconAnim(m_icon, m_anim, "fall_loop");
            // the held jump: fire under its feet on the way up
            m_emit -= dt;
            if (t < mv.t1 && mv.apex > mv.a.y + 1.f && m_emit <= 0.f) {
                m_emit = 0.025f;
                flame({x + m_rng.r(-1.5f, 1.5f), y - 0.5f}, m_rng.r(2.f, 3.4f), {m_rng.r(-3.f, 3.f), -m_rng.r(6.f, 11.f)}, m_rng.r(0.16f, 0.28f));
            }
        } else {
            playIconAnim(m_icon, m_anim, "run");
            m_emit -= dt;
            if (m_emit <= 0.f) {
                m_emit = 0.05f;
                square({x - 2.5f, y + 0.6f}, playerColor(), m_rng.r(0.6f, 1.1f), {-m_rng.r(2.f, 5.f), m_rng.r(0.5f, 3.f)}, m_rng.r(0.25f, 0.4f));
            }
        }
        setPosition({x, y});
        if (m_t >= mv.T) {
            if (mv.jump && mv.perch) landOn(mv);
            m_i++;
            m_t = 0.f;
        }
    }

private:
    struct Move {
        bool jump = false;
        CCPoint a, b;
        float T = 0.f, apex = 0.f, t1 = 0.f, g = 0.f;
        Perch const* perch = nullptr;
    };
    std::vector<Perch> m_perches;
    std::vector<Move> m_moves;
    size_t m_i = 0;
    float m_t = 0.f, m_emit = 0.f, m_half = 4.f;
    bool m_done = false;
    CCNode* m_fxParent = nullptr;
    SimplePlayer* m_icon = nullptr;
    std::string m_anim;
    Rand m_rng{0xB07u};

    static Move jumpMove(CCPoint a, CCPoint b, float apex, float T, Perch const* perch) {
        Move m;
        m.jump = true; m.a = a; m.b = b; m.T = T; m.perch = perch;
        m.apex = std::max({apex, a.y, b.y});
        float const d0 = std::sqrt(std::max(0.f, m.apex - a.y)), d1 = std::sqrt(std::max(0.f, m.apex - b.y));
        m.t1 = d0 + d1 > 0.f ? T * d0 / (d0 + d1) : 0.f;
        m.g = T > 0.f ? 2.f * (d0 + d1) * (d0 + d1) / (T * T) : 0.f; // the same pull on the way up and down
        return m;
    }
    static Move runMove(CCPoint a, CCPoint b) {
        Move m;
        m.a = a; m.b = b;
        m.T = std::fabs(b.x - a.x) / 52.f;
        return m;
    }

    bool init(CCNode* fxParent, std::vector<Perch> const& perches, float startX, float topY, float exitX, float bottomY) {
        if (!CCNode::init()) return false;
        m_fxParent = fxParent;
        m_perches = perches;
        m_rng = Rand(static_cast<uint32_t>(perches.size() * 7919u + static_cast<uint32_t>(startX * 31.f)) + 1u);
        m_icon = playerIcon(IconType::Robot, 0.36f);
        if (!m_icon) return false;
        m_half = 6.f;
        m_icon->setPosition({0.f, m_half});
        addChild(m_icon);
        plan(startX, topY, exitX, bottomY);
        if (m_moves.empty()) return false;
        setPosition(m_moves.front().a);
        scheduleUpdate();
        return true;
    }

    // from the left to the right, perch to perch
    void plan(float startX, float topY, float exitX, float bottomY) {
        // first: drop in onto something near the left
        Perch const* cur = nullptr;
        for (auto const& p : m_perches) {
            if (p.x1 - p.x0 < 8.f || p.x1 < startX + 4.f || p.x0 > startX + 45.f) continue;
            if (!cur || p.y > cur->y) cur = &p;
        }
        if (!cur) return;
        float cx = std::clamp(startX + 8.f, cur->x0 + 3.f, cur->x1 - 3.f);
        m_moves.push_back(jumpMove({cx - 10.f, topY + 34.f}, {cx, cur->y}, topY + 34.f, 0.55f, cur));
        for (int hop = 0; hop < 9; hop++) {
            std::vector<Perch const*> next;
            for (auto const& q : m_perches) {
                if (&q == cur || q.x1 - q.x0 < 8.f) continue;
                float const lo = std::max(q.x0 + 3.f, cx + 20.f), hi = std::min(q.x1 - 3.f, cx + 85.f);
                if (hi < lo) continue;
                if (q.y - cur->y > 28.f || cur->y - q.y > 95.f) continue;
                next.push_back(&q);
            }
            if (next.empty() || cx > exitX - 40.f) break;
            Perch const* q = next[static_cast<size_t>(m_rng.f() * static_cast<float>(next.size())) % next.size()];
            float const lo = std::max(q->x0 + 3.f, cx + 20.f), hi = std::min(q->x1 - 3.f, cx + 85.f);
            float const land = m_rng.r(lo, hi);
            // run a little along this one first (not past its end), then jump
            float const take = std::max(cx, std::min({cx + m_rng.r(0.f, 12.f), cur->x1 - 2.f, land - 18.f}));
            if (take > cx + 0.5f) m_moves.push_back(runMove({cx, cur->y}, {take, cur->y}));
            float const dx = land - take;
            float const apex = std::max(cur->y, q->y) + m_rng.r(9.f, 17.f);
            m_moves.push_back(jumpMove({take, cur->y}, {land, q->y}, apex, std::clamp(dx / 80.f, 0.34f, 0.8f), q));
            cur = q;
            cx = land;
        }
        // and off the right edge
        float const take = std::max(cx, cur->x1 - 3.f);
        if (take > cx + 0.5f) m_moves.push_back(runMove({cx, cur->y}, {take, cur->y}));
        m_moves.push_back(jumpMove({take, cur->y}, {exitX, bottomY - 30.f}, cur->y + 14.f, 0.9f, nullptr));
    }

    void landOn(Move const& mv) {
        for (int i = 0; i < 4; i++)
            square({mv.b.x + m_rng.r(-3.f, 3.f), mv.b.y + 0.6f}, playerColor(), m_rng.r(0.7f, 1.2f),
                   {m_rng.r(-5.f, 5.f), m_rng.r(1.f, 4.f)}, m_rng.r(0.25f, 0.4f));
        if (mv.perch && mv.perch->node) {
            CCNode* n = mv.perch->node.data();
            n->stopActionByTag(4711);
            auto* a = CCSequence::create(CCScaleTo::create(0.05f, 0.93f), CCEaseBackOut::create(CCScaleTo::create(0.2f, 1.f)), nullptr);
            a->setTag(4711);
            n->runAction(a);
        }
    }
    void finish() {
        m_done = true;
        setVisible(false);
        unscheduleUpdate();
    }
    void square(CCPoint at, ccColor3B c, float size, CCPoint drift, float life) {
        if (!m_fxParent) return;
        auto* d = CCLayerColor::create(ccColor4B{c.r, c.g, c.b, 230}, size, size);
        d->setPosition({at.x - size / 2.f, at.y - size / 2.f});
        d->runAction(CCEaseOut::create(CCMoveBy::create(life, drift), 1.6f));
        d->runAction(CCSequence::create(CCFadeTo::create(life, 0), CCRemoveSelf::create(), nullptr));
        m_fxParent->addChild(d, 58);
    }
    void flame(CCPoint at, float size, CCPoint drift, float life) {
        if (!m_fxParent) return;
        auto* f = sprite(soft(), size, {255, 236, 170}, 255);
        if (!f) return;
        additive(f);
        f->setPosition(at);
        float const sc = f->getScale();
        f->runAction(CCEaseOut::create(CCMoveBy::create(life, drift), 1.5f));
        f->runAction(CCScaleTo::create(life, sc * 0.3f));
        f->runAction(CCSequence::create(CCTintTo::create(life * 0.4f, 255, 140, 50), CCTintTo::create(life * 0.6f, 200, 50, 60), nullptr));
        f->runAction(CCSequence::create(CCFadeTo::create(life, 0), CCRemoveSelf::create(), nullptr));
        m_fxParent->addChild(f, 58);
    }
};

// ---- Extreme (the top stage) looks the part ----------------------------------------
// On the Extreme button: a hot pink edge glow (pulsing when it's the stage you're
// on), a light that sweeps across it, and embers rising off it while selected.
// (2.3) Also on the Discord tile: its own glow colour, no embers.
class ExtremeFx : public CCNode {
public:
    static ExtremeFx* create(float w, float h, float r, bool selected, ccColor3B glow = {255, 86, 176}, bool embers = true) {
        auto* p = new ExtremeFx();
        p->m_embers = embers;
        if (p->init(w, h, r, selected, glow)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    void update(float dt) override {
        dt = std::min(dt, 0.05f);
        m_t += dt;
        if (m_sheen) {
            float const cycle = std::fmod(m_t, 2.6f) / 1.3f;           // sweeps in the first half, rests in the second
            m_sheen->setVisible(cycle < 1.f);
            m_sheen->setPositionX(-m_w * 0.3f + std::min(cycle, 1.f) * m_w * 1.6f);
        }
        if (!m_sel || !m_embers) return;
        m_emit -= dt;
        while (m_emit <= 0.f) {
            m_emit += 0.07f;
            static ccColor3B const cols[3] = {{255, 128, 60}, {255, 84, 176}, {255, 206, 96}};
            auto* e = sprite(soft(), m_rng.r(1.4f, 2.6f), cols[static_cast<int>(m_rng.f() * 2.99f)], 255);
            if (!e) return;
            additive(e);
            e->setPosition({m_rng.r(4.f, m_w - 4.f), m_h - 1.f});
            float const life = m_rng.r(0.5f, 0.95f);
            e->runAction(CCEaseOut::create(CCMoveBy::create(life, {m_rng.r(-3.f, 3.f), m_rng.r(6.f, 14.f)}), 1.3f));
            e->runAction(CCSequence::create(CCFadeTo::create(life, 0), CCRemoveSelf::create(), nullptr));
            addChild(e, 3);
        }
    }

private:
    float m_w = 0.f, m_h = 0.f, m_t = 0.f, m_emit = 0.f;
    bool m_sel = false, m_embers = true;
    CCSprite* m_sheen = nullptr;
    Rand m_rng{0xE7u};

    bool init(float w, float h, float r, bool selected, ccColor3B glow) {
        if (!CCNode::init()) return false;
        m_w = w; m_h = h; m_sel = selected;
        setContentSize({w, h});
        if (CCNode* g = glowBox(0.f, 0.f, w, h, r, glow, selected ? 170 : 80)) {
            if (selected) {
                g->runAction(CCRepeatForever::create(CCSequence::create(CCFadeTo::create(0.9f, 235), CCFadeTo::create(0.9f, 115), nullptr)));
            }
            addChild(g, -1);
        }
        CCNode* clip = clipped(w, h, r);
        addChild(clip, 1);
        m_sheen = sprite(soft(), h * 2.4f, {255, 255, 255}, selected ? 120 : 60);
        if (m_sheen) {
            additive(m_sheen);
            m_sheen->setScaleX(m_sheen->getScaleX() * 0.45f);
            m_sheen->setRotation(18.f);
            m_sheen->setPosition({-w, h / 2.f});
            clip->addChild(m_sheen);
        }
        scheduleUpdate();
        return true;
    }
};

} // namespace nv
