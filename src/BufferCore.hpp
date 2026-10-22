// ============================================================================
//  BufferCore.hpp  -  the parts of the input buffer that don't touch the game
//
//  Kept free of Geode / cocos so they can be unit-tested on their own
//  (see tests/core_tests.cpp):
//
//    DelaySearch  : given "how good is a click delayed by d ticks?", finds the
//                   delay to use (smallest safe one, CBF timing guard, safety
//                   margin, optional aim). Every delay is evaluated at most once.
//
//    InputQueue   : schedules the held input and every input that arrives
//                   while it is held, in GAME TIME, in order, with their
//                   original spacing. Nothing is dropped or duplicated.
//
//    Clearance    : how close a path came to a hazard, how much room a path
//                   needs (from measured prediction error), moving-hazard
//                   tracking, and "is a cleaner moment coming?".
//
//    SpotMemory   : where your inputs landed on earlier attempts and whether
//                   you survived them (learning from attempts).
// ============================================================================
#pragma once

#include <algorithm>
#include <map>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <deque>
#include <optional>
#include <vector>

namespace ibcore {

// ----------------------------------------------------------------------------
//  Delay search
// ----------------------------------------------------------------------------
enum class Verdict : uint8_t {
    Bad,     // delayed click still leads to a death nothing can prevent
    Partial, // safe as far as we can see, but the lookahead was cut short (orb / portal)
    Good,    // safe for the whole lookahead, or any death left is still avoidable
    Budget   // ran out of CPU time; result unknown
};

struct SearchParams {
    int   maxDelay = 0;     // largest delay allowed by the window (ticks)
    int   stride   = 1;     // coarse step before bisection
    bool  guard    = true;  // CBF timing guard: d + 1 must also be Good
    int   margin   = 0;     // extra ticks past the earliest safe delay, if still safe
    float aim      = 0.f;   // 0 = earliest safe; >0 = fraction into the safe range (extreme)
    int   minDelay = 1;     // first delay worth trying (probes beyond the window start past it)
    int   dense    = 0;     // delays right after minDelay checked one by one (narrow safe gaps
                            // near the click are common; a coarse stride would step over them)
};

struct SearchResult {
    int  delay  = 0;     // 0 = no usable delay
    bool full   = false; // delay is Good (not just Partial)
    bool budget = false; // budget ran out somewhere (result may be less refined)
    int  evals  = 0;     // how many delays were actually evaluated
    int  earliest = 0;   // smallest Good delay found (before guard / margin / aim); 0 = none
    int  guarded  = 0;   // after the CBF guard, before margin / aim (what a per-tick
                         // re-check would fire on: the first Good tick with a Good
                         // tick after it); 0 = none
};

// `eval(d)` returns the Verdict for delay d (d >= 1). Results are cached, so
// the bisection, guard, margin and aim steps never re-simulate a delay.
template <class Eval>
SearchResult searchDelay(SearchParams const& p, Eval&& eval) {
    SearchResult out;
    if (p.maxDelay <= 0 || p.maxDelay < std::max(1, p.minDelay)) return out;

    int const cap = p.maxDelay + 2; // the guard may look one past the window
    std::vector<int8_t> cache(static_cast<size_t>(cap) + 1, -1);
    auto get = [&](int d) -> Verdict {
        if (d < 1 || d > cap) return Verdict::Bad;
        int8_t& c = cache[static_cast<size_t>(d)];
        if (c < 0) {
            Verdict v = eval(d);
            out.evals++;
            if (v == Verdict::Budget) {
                out.budget = true;
                return v; // not cached: unknown
            }
            c = static_cast<int8_t>(v);
        }
        return static_cast<Verdict>(c);
    };
    auto good = [&](int d) { return get(d) == Verdict::Good; };

    int const stride = std::max(1, p.stride);
    int partial = 0;
    int found = 0;
    int const first = std::max(1, p.minDelay);
    int lastBad = first - 1;

    // 1. Dense scan right after the click, then a coarse scan, then bisect
    //    down to the exact smallest Good delay.
    int const denseEnd = std::min(p.maxDelay, first - 1 + std::max(0, p.dense));
    int next = first;
    for (;;) {
        int const d = std::min(next, p.maxDelay);
        next = (d < denseEnd) ? d + 1 : d + stride;
        Verdict const v = get(d);
        if (v == Verdict::Budget) return out; // nothing trustworthy yet
        if (v == Verdict::Good) {
            int lo = lastBad, hi = d;
            while (hi - lo > 1) {
                int const mid = (lo + hi) / 2;
                Verdict const vm = get(mid);
                if (vm == Verdict::Budget) break;
                if (vm == Verdict::Good) hi = mid;
                else lo = mid;
            }
            found = hi;
            break;
        }
        if (v == Verdict::Partial && partial == 0) partial = d;
        lastBad = d;
        if (d >= p.maxDelay) break;
    }

    if (found == 0) {
        out.delay = partial;
        out.full = false;
        return out;
    }

    // 2. CBF timing guard: a held click fires on a step boundary, so it can
    //    land up to one tick after the chosen delay. Prefer a delay whose
    //    next tick is Good too; fall back to the plain smallest one.
    out.earliest = found;
    int chosen = found;
    if (p.guard && !out.budget) {
        int g = found;
        bool ok = false;
        while (g <= p.maxDelay && !out.budget) {
            if (good(g + 1)) { ok = true; break; }
            int next = 0;
            for (int t = g + 2; t <= p.maxDelay; t++) {
                if (good(t)) { next = t; break; }
                if (out.budget) break;
            }
            if (next == 0) break;
            g = next;
        }
        if (ok) chosen = g;
    }
    out.guarded = chosen;

    // 3a. Aim (extreme): find the far edge of the safe range, aim into it.
    if (p.aim > 0.001f && !out.budget) {
        int farEdge = chosen;      // furthest delay known to be Good
        bool hitBad = false;   // found the end of the safe range inside the window
        while (farEdge < p.maxDelay && !out.budget) {
            int const nd = std::min(farEdge + stride, p.maxDelay);
            if (good(nd)) { farEdge = nd; continue; }
            if (out.budget) break;
            hitBad = true;
            int lo = farEdge, hi = nd; // lo Good, hi not
            while (hi - lo > 1 && !out.budget) {
                int const mid = (lo + hi) / 2;
                if (good(mid)) lo = mid;
                else hi = mid;
            }
            farEdge = lo;
            break;
        }
        int const top = (p.guard && hitBad) ? std::max(chosen, farEdge - 1) : farEdge;
        int const pick = chosen + static_cast<int>(std::lround(static_cast<float>(top - chosen) * p.aim));
        if (pick > chosen && good(pick) && (!p.guard || good(pick + 1))) chosen = pick;
    }
    // 3b. Safety margin: step a little further from the deadly edge if still safe.
    else if (p.margin > 0 && !out.budget) {
        int const d2 = std::min(chosen + p.margin, p.maxDelay);
        if (d2 > chosen && good(d2) && (!p.guard || good(d2 + 1))) chosen = d2;
    }

    out.delay = chosen;
    out.full = true;
    return out;
}

// ----------------------------------------------------------------------------
//  Escape check sampling
//
//  When a test dies `deadTicks` after the click lands, at which moments do we
//  try a single follow-up input to see if the player could still save it?
//    * a death within `commit` ticks is always the click's fault -> no samples
//    * otherwise up to 3 moments: earliest follow-up, halfway, just before death,
//      all >= `react` and < `deadTicks`, no duplicates.
// ----------------------------------------------------------------------------
inline std::vector<int> escapeSamples(int deadTicks, int commit, int react, int lateGap, int maxSamples) {
    std::vector<int> out;
    if (deadTicks <= commit || maxSamples <= 0) return out;
    int const ks[3] = {react, std::max(react, deadTicks / 2), std::max(react, deadTicks - lateGap)};
    for (int k : ks) {
        if (static_cast<int>(out.size()) >= maxSamples) break;
        if (k < react || k >= deadTicks) continue;
        if (std::find(out.begin(), out.end(), k) != out.end()) continue;
        out.push_back(k);
    }
    return out;
}

// ----------------------------------------------------------------------------
//  Input queue (game-time scheduling)
//
//  Times are in any consistent unit (the mod uses 60fps frames of game time).
//  start()   : the click that triggered the buffer is held until now + delay.
//  push()    : any input arriving while the queue is active is replayed later
//              with the same shift, so ordering and spacing (hold lengths,
//              double taps) are preserved. Consecutive replays are at least
//              `minGap` apart so physics sees every one of them.
//              The queue stays "open" after the held input fires until the
//              gesture ends (the input that undoes it, e.g. the release of a
//              buffered press), so a release made after the delayed press
//              already fired still moves with it and the hold length is kept.
//  poll(now) : called before every physics step; returns at most one input
//              that is due. One per step boundary, never two at once.
// ----------------------------------------------------------------------------
class InputQueue {
public:
    struct Event {
        bool   push;
        double at;
    };

    static constexpr size_t MAX_EVENTS = 32;

    void start(bool push, double now, double delayTime) {
        m_events.clear();
        m_shift = std::max(0.0, delayTime);
        m_events.push_back(Event{push, now + m_shift});
        m_last = now + m_shift;
        m_lastPush = push;
        m_startPush = push;
        m_open = true;
        m_dropped = 0;
    }

    // Returns false if the event was ignored (duplicate direction or overflow).
    bool push(bool pushDown, double now, double minGap) {
        if (!active()) return false; // not active: caller handles the input normally
        if (pushDown == m_lastPush) return false; // same direction twice: nothing to replay
        if (m_events.size() >= MAX_EVENTS) { m_dropped++; return false; }
        double const at = std::max(now + m_shift, m_last + minGap);
        m_events.push_back(Event{pushDown, at});
        m_last = at;
        m_lastPush = pushDown;
        return true;
    }

    // Like push(), but at an explicit time (2.1: a wave / ship follow-up can go
    // in on time instead of keeping the held input's spacing). Never before the
    // previous input + minGap. Later inputs keep the spacing of this one.
    bool pushAt(bool pushDown, double at, double now, double minGap) {
        if (!active()) return false;
        if (pushDown == m_lastPush) return false;
        if (m_events.size() >= MAX_EVENTS) { m_dropped++; return false; }
        double const t = std::max(at, m_last + minGap);
        m_events.push_back(Event{pushDown, t});
        m_last = t;
        m_lastPush = pushDown;
        m_shift = std::max(0.0, t - now);
        return true;
    }

    std::optional<bool> poll(double now, double eps = 1e-6) {
        if (m_events.empty()) return std::nullopt;
        if (now + eps < m_events.front().at) return std::nullopt;
        bool const v = m_events.front().push;
        m_events.pop_front();
        // Gesture finished: everything delivered and the last input undid the first.
        if (m_events.empty() && v != m_startPush) m_open = false;
        return v;
    }

    // Inputs must go through the queue (not be decided fresh) while this is true.
    bool active() const { return !m_events.empty() || m_open; }
    // Something is waiting to be fired.
    bool pending() const { return !m_events.empty(); }
    void clear() {
        m_events.clear();
        m_shift = 0.0;
        m_open = false;
    }

    size_t size() const { return m_events.size(); }
    double shift() const { return m_shift; }
    double nextAt() const { return m_events.empty() ? -1.0 : m_events.front().at; }
    // Direction of the next input to fire (only valid while pending()).
    bool   nextPush() const { return m_events.empty() ? false : m_events.front().push; }
    // When the first queued input of the other direction fires (e.g. the
    // release behind a held press), or < 0 if none is queued yet.
    double nextOppositeAt() const {
        if (m_events.empty()) return -1.0;
        bool const head = m_events.front().push;
        for (auto const& e : m_events) if (e.push != head) return e.at;
        return -1.0;
    }

    // Move every waiting input by `delta` (live re-planning: the next input
    // fires earlier or later than planned, and everything behind it moves with
    // it, so the spacing between your inputs - hold lengths - stays the same).
    // Nothing is ever moved before `notBefore`.
    void shiftAll(double delta, double notBefore = -1e300) {
        if (m_events.empty()) return;
        if (m_events.front().at + delta < notBefore) delta = notBefore - m_events.front().at;
        for (auto& e : m_events) e.at += delta;
        m_last += delta;
        m_shift += delta;
    }
    bool   lastPush() const { return m_lastPush; }
    // When the last input in the queue fires / fired (the held one, or the
    // last one queued behind it).
    double lastAt() const { return m_last; }
    int    dropped() const { return m_dropped; }

private:
    std::deque<Event> m_events;
    double m_shift    = 0.0;
    double m_last     = 0.0;
    bool   m_lastPush = false;
    bool   m_startPush = true;
    bool   m_open     = false;
    int    m_dropped  = 0;
};

// ----------------------------------------------------------------------------
//  Prediction tracker (live accuracy)
//
//  After a click is let through or buffered, the sim's predicted path for it
//  is handed over here together with the time the click arrived. When game
//  time reaches the checkpoint, the real player position is compared with the
//  predicted one (interpolated, so CBF sub-steps don't skew it).
//
//  path[i] = player position after sim step i, steps of `dt` starting at `t0`.
//  A check is dropped (not counted) if anything the sim didn't know about
//  happens first (another input, respawn).
// ----------------------------------------------------------------------------
struct Point2 { float x = 0.f, y = 0.f, rot = 0.f; }; // position (+ icon rotation, degrees)

struct AccuracyStats {
    int    checks  = 0;   // comparisons made
    int    hits    = 0;   // within tolerance
    int    deaths  = 0;   // predicted alive, player died before the checkpoint
    int    dropped = 0;   // invalidated by an input / respawn
    double sumErr  = 0.0; // px, over checks that reached the checkpoint
    float  lastErr = 0.f;

    double percent() const { return checks > 0 ? 100.0 * hits / checks : 0.0; }
    double avgErr()  const { int n = checks - deaths; return n > 0 ? sumErr / n : 0.0; }
};

class PredictionTracker {
public:
    struct Result {
        bool  hit;
        float err;   // px
        float dx = 0.f, dy = 0.f; // real minus predicted, px
    };

    // checkStep: sim step index to compare at (clamped to the recorded path).
    void start(double t0, double dt, std::vector<Point2> path, int checkStep, float tolerancePx) {
        m_active = false;
        if (path.size() < 2 || dt <= 0.0) return;
        int const last = static_cast<int>(path.size()) - 1;
        m_check = std::clamp(checkStep, 1, last);
        m_path = std::move(path);
        m_t0 = t0;
        m_dt = dt;
        m_tol = tolerancePx;
        m_active = true;
    }

    bool active() const { return m_active; }

    // Something the sim didn't model happened: drop this check.
    void invalidate() {
        if (!m_active) return;
        m_active = false;
        m_stats.dropped++;
    }

    // Real player died while a check was pending: the prediction said alive.
    void died() {
        if (!m_active) return;
        m_active = false;
        m_stats.checks++;
        m_stats.deaths++;
    }

    // Called with the real position before every physics step.
    std::optional<Result> poll(double now, float rx, float ry, double eps = 1e-6) {
        if (!m_active) return std::nullopt;
        // Real position at `now` = state after (now - t0) / dt sim steps.
        double const steps = (now - m_t0) / m_dt;
        if (steps + eps < static_cast<double>(m_check + 1)) return std::nullopt;
        double const f = std::max(0.0, steps - 1.0);
        int const i = std::min(static_cast<int>(std::floor(f)), static_cast<int>(m_path.size()) - 1);
        int const j = std::min(i + 1, static_cast<int>(m_path.size()) - 1);
        float const frac = static_cast<float>(std::clamp(f - i, 0.0, 1.0));
        float const px = m_path[i].x + (m_path[j].x - m_path[i].x) * frac;
        float const py = m_path[i].y + (m_path[j].y - m_path[i].y) * frac;
        float const err = std::hypot(rx - px, ry - py);
        m_active = false;
        m_stats.checks++;
        m_stats.sumErr += err;
        m_stats.lastErr = err;
        bool const hit = err <= m_tol;
        if (hit) m_stats.hits++;
        return Result{hit, err, rx - px, ry - py};
    }

    AccuracyStats const& stats() const { return m_stats; }
    void resetStats() { m_stats = AccuracyStats{}; }

private:
    std::vector<Point2> m_path;
    double m_t0 = 0.0, m_dt = 0.25;
    int    m_check = 0;
    float  m_tol = 3.f;
    bool   m_active = false;
    AccuracyStats m_stats;
};

// ----------------------------------------------------------------------------
//  Calibrator (auto windows)
//
//  Keeps the last N "how much later did this click need to be" samples for
//  one window (60fps frames). The suggested window covers a high percentile of
//  them plus a small margin, so it fits the player's real timing error.
//  Serialises to a short comma-separated string for Geode's saved values.
// ----------------------------------------------------------------------------
class Calibrator {
public:
    static constexpr size_t MAX_SAMPLES = 64;

    void add(float frames) {
        if (!(frames > 0.f) || frames > 1000.f) return;
        m_samples.push_back(frames);
        if (m_samples.size() > MAX_SAMPLES) m_samples.erase(m_samples.begin());
    }

    size_t count() const { return m_samples.size(); }
    void clear() { m_samples.clear(); }

    // pct in [0, 1]. Linear interpolation between ranks.
    float percentile(float pct) const {
        if (m_samples.empty()) return 0.f;
        std::vector<float> v = m_samples;
        std::sort(v.begin(), v.end());
        double const r = std::clamp(static_cast<double>(pct), 0.0, 1.0) * (v.size() - 1);
        size_t const lo = static_cast<size_t>(std::floor(r));
        size_t const hi = std::min(lo + 1, v.size() - 1);
        return static_cast<float>(v[lo] + (v[hi] - v[lo]) * (r - lo));
    }

    // Suggested window, or nullopt while there are too few samples.
    std::optional<float> window(size_t minSamples, float pct, float marginFrames,
                                float minFrames, float maxFrames) const {
        if (m_samples.size() < minSamples) return std::nullopt;
        float w = percentile(pct) + marginFrames;
        // round up to a quarter frame (one 240-TPS tick)
        w = std::ceil(w * 4.f - 1e-4f) / 4.f;
        return std::clamp(w, minFrames, maxFrames);
    }

    std::string serialize() const {
        std::string out;
        char buf[32];
        for (size_t i = 0; i < m_samples.size(); i++) {
            std::snprintf(buf, sizeof(buf), "%s%.3f", i ? "," : "", m_samples[i]);
            out += buf;
        }
        return out;
    }

    void deserialize(std::string const& s) {
        m_samples.clear();
        size_t pos = 0;
        while (pos < s.size()) {
            size_t const comma = s.find(',', pos);
            std::string const tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            char* end = nullptr;
            float const v = std::strtof(tok.c_str(), &end);
            if (end && end != tok.c_str()) add(v);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }

private:
    std::vector<float> m_samples;
};

// ----------------------------------------------------------------------------
//  Live re-planning (the input queue's head)
//
//  While an input is held back, before every physics step: "is firing it now
//  safe?" is asked against the real game state (by the caller's `check`), and
//  the input fires at the first safe step (after `need` safe steps in a row),
//  or at the end of its window at the latest. Inputs queued behind it move
//  with it, so their spacing (hold lengths) is kept.
// ----------------------------------------------------------------------------
enum class LiveVerdict : uint8_t {
    Safe,        // firing now survives
    SafeCut,     // safe as far as the prediction reaches (orb / portal ahead)
    Unsafe,      // firing now dies
    Unknown,     // couldn't check (CPU budget): follow the plan
    OnSchedule,  // doesn't matter (e.g. a cube release): fire at its scheduled time
    Proven       // this exact spot worked on an earlier attempt: fire now
};

// Why the live step let an input go (for the log / death reasons).
enum class FireReason : uint8_t {
    None,     // not fired yet
    Safe,     // the check said firing now survives
    SafeCut,  // safe as far as the prediction reaches, at / after the plan
    Schedule, // couldn't check (CPU) or didn't matter: fired on its schedule
    Proven,   // a spot that worked on an earlier attempt
    Deadline, // end of the window: nothing safe found, went through as made
    Plan      // the live check kept refusing past the click-time plan: fired on the plan
};

struct LiveState {
    FireReason reason = FireReason::None; // why the last input fired
    bool   first    = true;  // head = the click that triggered the buffer
    bool   needInit = false; // a queued input just became the head
    double earliest = 0.0;   // never fire before
    double deadline = -1.0;  // always fire by (< 0: set from `windowTime` on first use)
    double planned  = 0.0;   // plan from click time (Unknown / SafeCut follow it)
    double latest   = -1.0;  // fire by this time even if the live check refuses (< 0: off)
    int    good     = 0;     // consecutive Safe checks

    void startClick(double now, double stepDt, double plannedTime, double deadlineTime, double latestTime = -1.0) {
        first = true;
        latest = latestTime;
        needInit = false;
        good = 0;
        earliest = now + stepDt * 0.5; // from the next step
        planned = plannedTime;
        deadline = deadlineTime;
    }
};

// `need(first)` -> safe steps in a row required. `windowTime(push)` -> window
// for a queued input that becomes the head. Returns the input fired this step.
template <class Check, class Need, class Window>
std::optional<bool> liveStep(InputQueue& q, LiveState& st, double now, double eps,
                             Check&& check, Need&& need, Window&& windowTime) {
    if (!q.pending()) return std::nullopt;
    bool const push = q.nextPush();
    if (st.needInit) {
        st.needInit = false;
        st.first = false;
        st.good = 0;
        st.earliest = q.nextAt(); // keeps your rhythm: never before its scheduled time
        st.planned = q.nextAt();
        st.deadline = -1.0;
        st.latest = -1.0;
    }
    if (now + eps < st.earliest) return std::nullopt;
    if (st.deadline < 0.0) st.deadline = st.earliest + windowTime(push) + eps;

    bool fire = false;
    FireReason why = FireReason::None;
    if (now + eps >= st.deadline) {
        fire = true; // end of the window: the input goes through as made
        why = FireReason::Deadline;
    } else if (st.latest >= 0.0 && now + eps >= st.latest) {
        // The click-time search found this moment safe; the live check keeps
        // saying no. Waiting longer only runs you into the obstacle: go.
        fire = true;
        why = FireReason::Plan;
    } else {
        LiveVerdict const v = check(push);
        switch (v) {
            case LiveVerdict::Safe:
                st.good++;
                fire = st.good >= need(st.first);
                why = FireReason::Safe;
                break;
            case LiveVerdict::SafeCut:
                if (now + eps >= st.planned) { st.good++; fire = st.good >= need(st.first); }
                else st.good = 0;
                why = FireReason::SafeCut;
                break;
            case LiveVerdict::Unknown:
            case LiveVerdict::OnSchedule:
                fire = now + eps >= st.planned;
                why = FireReason::Schedule;
                break;
            case LiveVerdict::Proven:
                fire = true;
                why = FireReason::Proven;
                break;
            case LiveVerdict::Unsafe:
            default:
                st.good = 0;
                break;
        }
    }

    if (!fire) {
        double const late = now - q.nextAt();
        if (late > 0.0) q.shiftAll(late + eps); // held past its time: the rest moves too
        return std::nullopt;
    }
    st.reason = why;
    q.shiftAll(now - q.nextAt());
    auto due = q.poll(now, eps);
    if (q.pending()) st.needInit = true;
    return due;
}


// ----------------------------------------------------------------------------
//  Clearance and moving hazards (v1.11)
//
//  A prediction that "survives" by 0.2px is not the same as one that clears a
//  spike by 6px: the sim is off by a pixel or two in real play, so a near miss
//  in the sim is a coin flip in the game. These helpers measure how close a
//  path came to a hazard and track hazards that move (move / rotate triggers)
//  so the sim can test against where they WILL be, not where they are now.
// ----------------------------------------------------------------------------
struct Box {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f; // origin (bottom-left) + size
};

// Distance between two boxes in px (0 = touching edge to edge). Negative
// (-1) = they overlap.
inline float boxGap(Box const& a, Box const& b) {
    float const sx = std::max(b.x - (a.x + a.w), a.x - (b.x + b.w));
    float const sy = std::max(b.y - (a.y + a.h), a.y - (b.y + b.h));
    if (sx < 0.f && sy < 0.f) return -1.f;
    return std::hypot(std::max(sx, 0.f), std::max(sy, 0.f));
}

// How much room a "safe" path needs, from the measured prediction error of
// this gamemode: `scale` x average error, clamped. Too few checks -> `def`.
inline float clearanceTarget(int checks, double errSum, int minChecks, float def,
                             float lo, float hi, float scale) {
    if (checks < minChecks || checks <= 0) return def;
    float const avg = static_cast<float>(errSum / checks);
    return std::clamp(avg * scale, lo, hi);
}

// One hazard's motion, sampled from the real game once per physics step.
//
// Speed is measured over at least `baseline` of game time (not step to step:
// with Click Between Frames / uneven steps an object can move on one step and
// not the next). Acceleration is tracked too, because move triggers usually
// EASE (speed up, slow down): a constant-speed model never matched them, so
// they were never trusted (1.22.1). A slowing object is predicted to stop,
// never to reverse. A motion is trusted after it predicted the object right
// `minOk` times in a row (within 3px or 25% of the move).
struct MoverTrack {
    double x = 0.0, y = 0.0, t = -1.0;    // last sample (position, game time)
    double ax0 = 0.0, ay0 = 0.0, at = -1.0; // baseline anchor
    float  vx = 0.f, vy = 0.f;            // px per game-time unit (60fps frame)
    float  accx = 0.f, accy = 0.f;        // px per unit^2
    double vt = -1.0;                     // when the velocity was measured (-1 = none)
    int    ok = 0;                        // predictions in a row that came true

    static constexpr float MAX_ACC = 40.f;      // px / unit^2 (beyond = a jump, not easing)
    static constexpr double ACC_HORIZON = 15.0; // acceleration extrapolated at most this far

    // Displacement after `dt` along one axis with speed v and acceleration a,
    // stopping (not reversing) when a slows v down to zero.
    static double travel(double v, double a, double dt) {
        double const ta = std::min(dt, ACC_HORIZON);
        if (a != 0.0 && v * a < 0.0) {
            double const tStop = -v / a; // > 0
            if (ta >= tStop) return v * tStop + 0.5 * a * tStop * tStop;
        }
        // acceleration only over ta, then constant speed
        double const d1 = v * ta + 0.5 * a * ta * ta;
        double const v1 = v + a * ta;
        return d1 + v1 * (dt - ta);
    }

    void sample(double nx, double ny, double nt, double maxGap, float teleport, double baseline = 0.5) {
        if (t >= 0.0 && nt - t <= 0.0005) return; // same step: keep the older sample
        if (t < 0.0 || nt - t >= maxGap) {
            x = ax0 = nx; y = ay0 = ny; t = at = nt;
            vx = vy = accx = accy = 0.f; vt = -1.0; ok = 0;
            return;
        }
        x = nx; y = ny; t = nt;
        double const span = nt - at;
        if (span < baseline) return;
        float const ux = static_cast<float>((nx - ax0) / span);
        float const uy = static_cast<float>((ny - ay0) / span);
        if (std::fabs(ux) >= teleport || std::fabs(uy) >= teleport) {
            vx = vy = accx = accy = 0.f; vt = -1.0; ok = 0;
        } else {
            if (vt >= 0.0) {
                // Did the last motion predict this sample?
                double const px = ax0 + travel(vx, accx, span);
                double const py = ay0 + travel(vy, accy, span);
                double const err = std::hypot(nx - px, ny - py);
                double const moved = std::hypot(nx - ax0, ny - ay0);
                ok = (err <= std::max(3.0, 0.25 * moved)) ? ok + 1 : 0;
                // acceleration from the change of the average speed between windows
                float const nax = static_cast<float>((ux - vx) / span);
                float const nay = static_cast<float>((uy - vy) / span);
                accx = (std::fabs(nax) < MAX_ACC) ? nax : 0.f;
                accy = (std::fabs(nay) < MAX_ACC) ? nay : 0.f;
            }
            vx = ux; vy = uy; vt = nt;
        }
        ax0 = nx; ay0 = ny; at = nt;
    }

    // Measured recently, proven, and actually moving.
    bool moving(double now, double fresh, float minSpeed, int minOk = 1) const {
        return vt >= 0.0 && ok >= minOk && now - vt <= fresh &&
               (std::fabs(vx) >= minSpeed || std::fabs(vy) >= minSpeed || std::fabs(accx) > 0.05f ||
                std::fabs(accy) > 0.05f);
    }

    // Offset from its last sample to where it will be at game time `when`.
    // (The speed is the average over the last window, i.e. at its middle; the
    // small lag that leaves is part of what the proof check measures.)
    void offsetAt(double when, float& dx, float& dy) const {
        double const ahead = std::max(0.0, when - t);
        dx = static_cast<float>(travel(vx, accx, ahead));
        dy = static_cast<float>(travel(vy, accy, ahead));
    }
};

// Live check: firing now is safe but only by `cur` px (< target). Is a
// clearly cleaner moment coming within `look` steps? probe(k) -> {stop, good,
// clear} for firing k steps from now (stop = out of time / budget). Returns
// the first k whose path is good, clears `target` and beats `cur` by `gain`,
// or 0 = none (take the close one now: waiting would only risk the window).
struct CleanProbe {
    bool  stop  = false;
    bool  good  = false;
    float clear = 0.f;
};
template <class Probe>
int cleanerAhead(float cur, float target, float gain, int look, Probe&& probe) {
    if (cur >= target) return 0;
    for (int k = 1; k <= look; k++) {
        CleanProbe const r = probe(k);
        if (r.stop) return 0;
        if (r.good && r.clear >= target && r.clear >= cur + gain) return k;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  Spot memory (v1.12): learning from your attempts
//
//  Every input that reaches the game is remembered by WHERE it landed (player
//  position, vertical speed, gamemode, press / release, gravity). If you were
//  still alive a moment later the spot is "good"; if you died first it's
//  "bad". On the next attempt the live check fires on good spots and refuses
//  bad ones, even when the prediction says otherwise: what really happened
//  beats what the sim thinks. Kept for the level (all attempts).
// ----------------------------------------------------------------------------
struct Spot {
    float    x = 0.f, y = 0.f, vy = 0.f;
    uint32_t key = 0;      // gamemode / press / gravity
    int      good = 0, bad = 0;
    uint32_t anyKey = 0;   // press / gravity only (any gamemode): "a spot ahead" across portals
};

enum class SpotHit : uint8_t { None, Good, Bad };

struct SpotTol {
    float x  = 1.f;   // px (about half a tick of travel)
    float y  = 6.f;   // px: same route, not a different path through the section
    float vy = 1.5f;  // vertical speed: same moment of the jump
};

class SpotMemory {
public:
    static constexpr size_t MAX_SPOTS = 4000;

    // Record an outcome. A spot already known (within tolerance) is updated.
    void add(Spot s, bool good, SpotTol const& tol) {
        if (Spot* e = match(s.x, s.y, s.vy, s.key, tol)) {
            (good ? e->good : e->bad)++;
            return;
        }
        s.good = good ? 1 : 0;
        s.bad = good ? 0 : 1;
        if (m_spots.size() >= MAX_SPOTS) m_spots.erase(m_spots.begin());
        m_spots.push_back(s);
    }

    // What happened last times at this exact spot? Bad wins ties: a death is
    // the stronger lesson.
    SpotHit at(float x, float y, float vy, uint32_t key, SpotTol const& tol) const {
        Spot const* e = match(x, y, vy, key, tol);
        if (!e) return SpotHit::None;
        return e->bad >= e->good ? SpotHit::Bad : SpotHit::Good;
    }

    // Nearest good spot strictly ahead (in direction `dir`, within `reach`
    // px), same route. Returns its distance, or < 0 for none. `vy` is not
    // matched: it changes on the way there.
    // `anyKey` != 0: also match good spots recorded in another gamemode (a spot
    // past a portal, seen from before it). Position in the level decides.
    float goodAhead(float x, float y, uint32_t key, float dir, float reach, SpotTol const& tol,
                    uint32_t anyKey = 0) const {
        float best = -1.f;
        for (Spot const& s : m_spots) {
            bool const keyOk = s.key == key || (anyKey != 0 && s.anyKey == anyKey);
            if (!keyOk || s.bad >= s.good) continue;
            float const d = (s.x - x) * dir;
            if (d <= tol.x || d > reach) continue;
            if (std::fabs(s.y - y) > tol.y + std::fabs(d) * 0.5f) continue; // y may drift a bit on the way
            if (best < 0.f || d < best) best = d;
        }
        return best;
    }

    void clear() { m_spots.clear(); }
    size_t size() const { return m_spots.size(); }
    // Drop every spot `keep` says no to; returns how many went.
    template <class F>
    size_t keepIf(F&& keep) {
        size_t const before = m_spots.size();
        m_spots.erase(std::remove_if(m_spots.begin(), m_spots.end(), [&](Spot const& sp) { return !keep(sp); }),
                      m_spots.end());
        return before - m_spots.size();
    }

    // Saved to a file per level, so what you learned survives restarts.
    // One spot per line: "x y vy key good bad [anyKey]". Bad lines are skipped.
    std::string serialize() const {
        std::string out = "IBSPOTS 1\n";
        char buf[96];
        for (Spot const& s : m_spots) {
            std::snprintf(buf, sizeof(buf), "%.2f %.2f %.3f %u %d %d %u\n", s.x, s.y, s.vy,
                          static_cast<unsigned>(s.key), s.good, s.bad, static_cast<unsigned>(s.anyKey));
            out += buf;
        }
        return out;
    }
    void deserialize(std::string const& text) {
        m_spots.clear();
        size_t pos = 0;
        bool header = true;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string const line = text.substr(pos, end - pos);
            pos = end + 1;
            if (header) {
                header = false;
                if (line.rfind("IBSPOTS", 0) == 0) continue;
            }
            char const* c = line.c_str();
            char* q = nullptr;
            Spot s;
            s.x = std::strtof(c, &q);     if (q == c) continue; c = q;
            s.y = std::strtof(c, &q);     if (q == c) continue; c = q;
            s.vy = std::strtof(c, &q);    if (q == c) continue; c = q;
            unsigned long const k = std::strtoul(c, &q, 10); if (q == c) continue; c = q;
            long const g = std::strtol(c, &q, 10); if (q == c) continue; c = q;
            long const b = std::strtol(c, &q, 10); if (q == c) continue; c = q;
            unsigned long const ak = std::strtoul(c, &q, 10); // optional (older files: 0)
            s.anyKey = (q == c) ? 0u : static_cast<uint32_t>(ak);
            if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.vy) || g < 0 || b < 0) continue;
            s.key = static_cast<uint32_t>(k);
            s.good = static_cast<int>(std::min<long>(g, 1000000));
            s.bad = static_cast<int>(std::min<long>(b, 1000000));
            if (s.good == 0 && s.bad == 0) continue;
            if (m_spots.size() >= MAX_SPOTS) break;
            m_spots.push_back(s);
        }
    }
    int goodCount() const { int n = 0; for (auto const& s : m_spots) n += s.bad < s.good; return n; }
    int badCount() const { return static_cast<int>(m_spots.size()) - goodCount(); }

private:
    std::vector<Spot> m_spots;

    Spot const* match(float x, float y, float vy, uint32_t key, SpotTol const& tol) const {
        Spot const* best = nullptr;
        float bestD = 0.f;
        for (Spot const& s : m_spots) {
            if (s.key != key) continue;
            float const dx = std::fabs(s.x - x);
            if (dx > tol.x || std::fabs(s.y - y) > tol.y || std::fabs(s.vy - vy) > tol.vy) continue;
            if (!best || dx < bestD) { best = &s; bestD = dx; }
        }
        return best;
    }
    Spot* match(float x, float y, float vy, uint32_t key, SpotTol const& tol) {
        return const_cast<Spot*>(static_cast<SpotMemory const*>(this)->match(x, y, vy, key, tol));
    }
};

// Which input gets the credit or the blame: the latest one. It becomes
// "good" when you're still alive `window` later or make another input; "bad"
// if you die before that.
class OutcomeTracker {
public:
    struct Done { Spot spot; bool good; };

    // A new input reached the game. Returns the previous one, now good.
    std::optional<Done> input(Spot const& s, double now) {
        std::optional<Done> out;
        if (m_has) out = Done{m_spot, true};
        m_spot = s;
        m_at = now;
        m_has = true;
        return out;
    }
    std::optional<Done> death(double now, double window) {
        if (!m_has) return std::nullopt;
        m_has = false;
        return Done{m_spot, now - m_at > window}; // died long after: not its fault
    }
    std::optional<Done> poll(double now, double window) {
        if (!m_has || now - m_at <= window) return std::nullopt;
        m_has = false;
        return Done{m_spot, true};
    }
    void clear() { m_has = false; }
    bool pending() const { return m_has; }

private:
    Spot   m_spot;
    double m_at = 0.0;
    bool   m_has = false;
};

// ----------------------------------------------------------------------------
//  Orb aim tick picking (1.25.5)
// ----------------------------------------------------------------------------
// Which press tick (in [lo, hi]) to use for an orb. `probe(d)` tests a press on
// tick d and returns nullopt when out of time. Overlaps short enough are tested
// tick by tick; longer ones every 3rd tick, then every tick (+-2) around:
//  * every working tick,
//  * where the orb starts / stops being used (frame-perfect windows sit at the
//    orb's edge),
//  * the 3 orb-using ticks that survived longest.
// The pick is the middle of the longest run of consecutive working ticks.
struct AimProbe {
    bool orb = false;  // the press used the orb
    bool ok = false;   // ...and it gets you through
    int  dies = -1;    // ticks alive after the press (-1: alive)
};
struct AimPick {
    int best = -1, runLo = -1, runHi = -1;
    int tested = 0;
    bool outOfTime = false;
    std::map<int, AimProbe> res;
};
template <class Probe>
AimPick pickOrbTick(int lo, int hi, int fineSpan, int maxTests, Probe&& probe) {
    AimPick p;
    if (hi < lo) return p;
    auto test = [&](int d) {
        if (d < lo || d > hi || p.res.count(d) || p.tested >= maxTests || p.outOfTime) return;
        std::optional<AimProbe> r = probe(d);
        if (!r) { p.outOfTime = true; return; }
        p.tested++;
        p.res[d] = *r;
    };
    int const step = (hi - lo + 1) <= fineSpan ? 1 : 3;
    for (int d = lo; d <= hi; d += step) test(d);
    test(hi);
    if (step > 1) {
        std::vector<int> around;
        std::vector<std::pair<int, int>> surv;
        int prevD = -1;
        bool prevOrb = false, havePrev = false;
        for (auto const& [d, t] : p.res) {
            if (t.ok) around.push_back(d);
            if (havePrev && prevOrb != t.orb) { around.push_back(prevD); around.push_back(d); }
            if (t.orb) surv.emplace_back(t.dies < 0 ? (1 << 20) : t.dies, d);
            prevD = d;
            prevOrb = t.orb;
            havePrev = true;
        }
        std::stable_sort(surv.begin(), surv.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
        for (size_t i = 0; i < surv.size() && i < 3; i++) around.push_back(surv[i].second);
        for (int d : around) for (int j = -2; j <= 2; j++) test(d + j);
        // Still nothing that works: every remaining tick, from the end of the
        // overlap back (where the working tick of an on-time click has been),
        // until the test / time limit.
        bool anyOk = false;
        for (auto const& [d, t] : p.res) if (t.ok) { anyOk = true; break; }
        if (!anyOk) for (int d = hi; d >= lo && !p.outOfTime && p.tested < maxTests; d--) test(d);
    }
    int bestLen = 0;
    for (auto it = p.res.begin(); it != p.res.end(); ++it) {
        if (!it->second.ok) continue;
        int const s0 = it->first;
        int e0 = s0;
        auto jt = std::next(it);
        while (jt != p.res.end() && jt->first == e0 + 1 && jt->second.ok) { e0 = jt->first; ++jt; }
        if (e0 - s0 + 1 > bestLen) { bestLen = e0 - s0 + 1; p.runLo = s0; p.runHi = e0; }
    }
    if (bestLen > 0) p.best = p.runLo + (p.runHi - p.runLo) / 2;
    return p;
}

} // namespace ibcore
