// Unit tests for src/BufferCore.hpp (no game needed).
//   g++ -std=c++20 -O2 -I ../src core_tests.cpp -o core_tests && ./core_tests
//
// Everything here is synthetic: verdict patterns and step timings are made up
// to exercise the logic. It proves the logic does what it claims; it does not
// measure in-game latency.
#include "BufferCore.hpp"

#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace ibcore;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (cond) g_pass++;                                                \
        else {                                                             \
            g_fail++;                                                      \
            std::printf("  FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond);  \
            std::printf(__VA_ARGS__);                                      \
            std::printf("\n");                                             \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
//  Delay search
// ---------------------------------------------------------------------------
struct Pattern {
    std::function<Verdict(int)> f;
    std::map<int, int> calls = {};
    Verdict operator()(int d) { calls[d]++; return f(d); }
};

static SearchResult run(SearchParams p, Pattern& pat) {
    return searchDelay(p, [&](int d) { return pat(d); });
}

static void testSearch() {
    std::printf("[search]\n");
    {   // smallest safe delay, exact with stride 1
        Pattern pat{[](int d) { return d >= 5 ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 5 && r.full, "delay=%d", r.delay);
    }
    {   // exact with a coarse stride (bisection)
        for (int edge = 1; edge <= 40; edge++) {
            Pattern pat{[edge](int d) { return d >= edge ? Verdict::Good : Verdict::Bad; }};
            auto r = run({40, 4, false, 0, 0.f}, pat);
            CHECK(r.delay == edge, "edge=%d got=%d", edge, r.delay);
        }
    }
    {   // guard: 5 is safe but 6 is not -> next safe pair is 8/9
        Pattern pat{[](int d) { return (d == 5 || d >= 8) ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 8, "delay=%d", r.delay);
    }
    {   // guard fallback: only an isolated safe tick -> still use it
        Pattern pat{[](int d) { return d == 5 ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 5 && r.full, "delay=%d", r.delay);
    }
    {   // guard may look one tick past the window
        Pattern pat{[](int d) { return d >= 16 ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 16, "delay=%d", r.delay);
        CHECK(pat.calls.count(17) == 1, "17 not checked");
        CHECK(pat.calls.count(18) == 0, "looked too far");
    }
    {   // no Good anywhere, a Partial (cut by orb/portal) is used as a fallback
        Pattern pat{[](int d) { return d == 3 ? Verdict::Partial : Verdict::Bad; }};
        auto r = run({8, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 3 && !r.full, "delay=%d full=%d", r.delay, r.full);
    }
    {   // nothing helps
        Pattern pat{[](int) { return Verdict::Bad; }};
        auto r = run({8, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 0, "delay=%d", r.delay);
    }
    {   // budget before anything is found -> no delay, flagged
        Pattern pat{[](int) { return Verdict::Budget; }};
        auto r = run({8, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 0 && r.budget, "delay=%d budget=%d", r.delay, r.budget);
    }
    {   // budget during refinement keeps the found delay
        Pattern pat{[](int d) { return d >= 4 ? (d == 5 ? Verdict::Budget : Verdict::Good) : Verdict::Bad; }};
        auto r = run({16, 1, true, 0, 0.f}, pat);
        CHECK(r.delay == 4 && r.budget, "delay=%d budget=%d", r.delay, r.budget);
    }
    {   // margin steps one further when safe (and its pair is safe)
        Pattern pat{[](int d) { return d >= 5 ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 1, 0.f}, pat);
        CHECK(r.delay == 6, "delay=%d", r.delay);
    }
    {   // margin skipped when it would land on the edge
        Pattern pat{[](int d) { return (d >= 5 && d <= 6) ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 1, 0.f}, pat);
        CHECK(r.delay == 5, "delay=%d", r.delay);
    }
    {   // aim 0.5: safe 5..15 -> aims into the middle of 5..14 (guard keeps a tick)
        Pattern pat{[](int d) { return (d >= 5 && d <= 15) ? Verdict::Good : Verdict::Bad; }};
        auto r = run({30, 2, true, 0, 0.5f}, pat);
        CHECK(r.delay >= 9 && r.delay <= 10, "delay=%d", r.delay);
    }
    {   // no delay is ever evaluated twice
        Pattern pat{[](int d) { return (d == 7 || d >= 11) ? Verdict::Good : Verdict::Bad; }};
        run({40, 3, true, 2, 0.5f}, pat);
        bool once = true;
        for (auto& [d, n] : pat.calls) if (n != 1) once = false;
        CHECK(once, "a delay was simulated more than once");
    }
}

// ---------------------------------------------------------------------------
//  Step timing models
//    TPS 240 (tick = 0.25 frame-units). Rendered frames at `fps`. Without CBF
//    each frame runs whole ticks. With CBF, an input that lands mid-frame
//    splits the current tick into two shorter steps at the input time.
// ---------------------------------------------------------------------------
constexpr double TICK = 0.25; // 60fps-frame units

// Physics step boundaries (times at which a step STARTS), from 0 to `end`.
static std::vector<double> stepStarts(double end, std::vector<double> const& splitTimes) {
    std::vector<double> b;
    for (double t = 0.0; t < end; t += TICK) b.push_back(t);
    for (double s : splitTimes) b.push_back(s);
    std::sort(b.begin(), b.end());
    b.erase(std::unique(b.begin(), b.end(), [](double x, double y) { return std::fabs(x - y) < 1e-9; }), b.end());
    return b;
}

struct Fired { bool push; double t; };

// Real inputs: (push, time). With CBF the input arrives exactly at its time
// (and creates a step boundary there); without CBF it arrives at the start
// of the next rendered frame.
struct Input { bool push; double t; };

static double frameAlign(double t, double fps) {
    double const frame = 60.0 / fps; // frame length in 60fps-frame units
    return std::ceil(t / frame - 1e-9) * frame;
}

// --- New scheduler driven by InputQueue (mirrors main.cpp) ---
// `decide(push, now)` returns the delay in ticks for an input that would be
// buffered (0 = pass immediately). Only consulted when the queue is idle.
static std::vector<Fired> runNew(std::vector<Input> in, bool cbf, double fps,
                                 std::function<int(bool, double)> decide) {
    for (auto& i : in) if (!cbf) i.t = frameAlign(i.t, fps);
    std::vector<double> splits;
    if (cbf) for (auto& i : in) splits.push_back(i.t);
    double const end = in.back().t + 40.0;
    auto starts = stepStarts(end, splits);

    InputQueue q;
    std::vector<Fired> out;
    size_t next = 0;
    for (double now : starts) {
        // inputs arriving at this boundary (handleButton runs before the step)
        while (next < in.size() && in[next].t <= now + 1e-9) {
            Input const& e = in[next++];
            if (q.active()) { q.push(e.push, now, TICK * 0.999); continue; }
            int d = decide(e.push, now);
            if (d > 0) q.start(e.push, now, d * TICK);
            else out.push_back({e.push, now});
        }
        // onPlayerUpdate: before the step
        if (auto f = q.poll(now, 1e-4)) out.push_back({*f, now});
    }
    return out;
}

// --- Previous (v1.3.0) scheduler: counts update calls, one queued-opposite flag ---
static std::vector<Fired> runOld(std::vector<Input> in, bool cbf, double fps,
                                 std::function<int(bool, double)> decide) {
    for (auto& i : in) if (!cbf) i.t = frameAlign(i.t, fps);
    std::vector<double> splits;
    if (cbf) for (auto& i : in) splits.push_back(i.t);
    double const end = in.back().t + 40.0;
    auto starts = stepStarts(end, splits);

    int wait = -1, follow = -1, elapsed = 0, queuedAt = 0;
    bool bufPush = true, queuedOpp = false;
    std::vector<Fired> out;
    size_t next = 0;
    for (double now : starts) {
        while (next < in.size() && in[next].t <= now + 1e-9) {
            Input const& e = in[next++];
            if (wait >= 0) {
                bool opp = (e.push != bufPush);
                if (opp && !queuedOpp) queuedAt = elapsed;
                queuedOpp = opp;   // a same-direction input clears it (the bug)
                continue;
            }
            if (follow >= 0) { follow = -1; out.push_back({!bufPush, now}); }
            int d = decide(e.push, now);
            if (d > 0) { bufPush = e.push; wait = d; queuedOpp = false; elapsed = 0; queuedAt = 0; follow = -1; }
            else out.push_back({e.push, now});
        }
        if (wait == 0) {
            out.push_back({bufPush, now});
            if (queuedOpp) { follow = std::max(1, queuedAt); queuedOpp = false; }
            wait = -1;
        } else if (wait > 0) { wait--; elapsed++; }
        else if (follow > 1) follow--;
        else if (follow == 1) { out.push_back({!bufPush, now}); follow = -1; }
    }
    return out;
}

static int countPress(std::vector<Fired> const& f) {
    int n = 0;
    for (auto& e : f) n += e.push ? 1 : 0;
    return n;
}

static bool alternates(std::vector<Fired> const& f) {
    for (size_t i = 1; i < f.size(); i++) if (f[i].push == f[i - 1].push) return false;
    return true;
}

static void testQueue() {
    std::printf("[queue]\n");

    // 1. Double tap while the first press is held back: nothing may be lost.
    //    press @1.0 (buffered 6 ticks), release @1.5, press @2.0, release @2.5
    std::vector<Input> dbl = {{true, 1.0}, {false, 1.5}, {true, 2.0}, {false, 2.5}};
    auto first6 = [](bool push, double) { return push ? 6 : 0; };
    for (bool cbf : {false, true}) {
        for (double fps : {60.0, 144.0, 170.0, 240.0, 360.0}) {
            auto n = runNew(dbl, cbf, fps, first6);
            auto o = runOld(dbl, cbf, fps, first6);
            CHECK(countPress(n) == 2 && n.size() == 4 && alternates(n),
                  "new lost/duplicated taps cbf=%d fps=%.0f presses=%d events=%zu", cbf, fps, countPress(n), n.size());
            if (fps == 170.0 && cbf)
                std::printf("  double tap @170fps+CBF: old fired %d press(es) / %zu events, new %d / %zu\n",
                            countPress(o), o.size(), countPress(n), n.size());
        }
    }

    // 2. Held press fires no earlier than planned, and within one step after,
    //    at every fps, with and without CBF sub-steps.
    for (bool cbf : {false, true}) {
        for (double fps : {60.0, 144.0, 170.0, 240.0, 360.0}) {
            for (double t0 : {1.0, 1.13, 1.37, 1.61}) {
                std::vector<Input> one = {{true, t0}, {false, t0 + 3.0}};
                auto n = runNew(one, cbf, fps, [](bool p, double) { return p ? 4 : 0; });
                double arrive = cbf ? t0 : frameAlign(t0, fps);
                double planned = arrive + 4 * TICK;
                CHECK(!n.empty() && n[0].push && n[0].t + 1e-6 >= planned && n[0].t <= planned + TICK + 1e-6,
                      "fire at %.4f planned %.4f cbf=%d fps=%.0f", n.empty() ? -1 : n[0].t, planned, cbf, fps);
            }
        }
    }

    // 3. Old scheduler under CBF: extra sub-steps made held presses fire early.
    {
        int early = 0, total = 0;
        for (double t0 = 1.0; t0 < 3.0; t0 += 0.07) {
            // a second input shortly after creates an extra CBF sub-step during the wait
            std::vector<Input> seq = {{true, t0}, {false, t0 + 0.31}, {true, t0 + 3.0}, {false, t0 + 3.4}};
            auto o = runOld(seq, true, 170.0, [](bool p, double) { return p ? 4 : 0; });
            auto n = runNew(seq, true, 170.0, [](bool p, double) { return p ? 4 : 0; });
            double planned = t0 + 4 * TICK;
            total++;
            if (!o.empty() && o[0].t + 1e-6 < planned) early++;
            CHECK(!n.empty() && n[0].t + 1e-6 >= planned, "new fired early at t0=%.2f", t0);
        }
        std::printf("  170fps+CBF held press fired early: old %d/%d, new 0/%d\n", early, total, total);
    }

    // 4. Hold length survives the delay (within one step).
    for (double fps : {60.0, 170.0, 240.0}) {
        for (bool cbf : {false, true}) {
            std::vector<Input> hold = {{true, 1.0}, {false, 1.0 + 7 * TICK + 0.05}};
            auto n = runNew(hold, cbf, fps, [](bool p, double) { return p ? 5 : 0; });
            CHECK(n.size() == 2, "events=%zu", n.size());
            if (n.size() == 2) {
                double realHold = (cbf ? hold[1].t : frameAlign(hold[1].t, fps)) - (cbf ? hold[0].t : frameAlign(hold[0].t, fps));
                double gotHold = n[1].t - n[0].t;
                CHECK(std::fabs(gotHold - realHold) <= TICK + 1e-6, "hold %.3f vs %.3f fps=%.0f cbf=%d", gotHold, realHold, fps, cbf);
            }
        }
    }

    // 5. Rapid spam (UFO/swing style) while held: every tap replayed, in order,
    //    at least one tick apart.
    {
        std::vector<Input> spam;
        double t = 1.0;
        for (int i = 0; i < 12; i++) { spam.push_back({true, t}); spam.push_back({false, t + 0.3}); t += 0.6; }
        auto n = runNew(spam, true, 170.0, [](bool p, double) { return p ? 8 : 0; });
        CHECK(countPress(n) == 12 && alternates(n), "presses=%d alt=%d", countPress(n), alternates(n));
        bool spaced = true;
        for (size_t i = 1; i < n.size(); i++) if (n[i].t - n[i - 1].t < TICK * 0.99 - 1e-6) spaced = false;
        CHECK(spaced, "two replays closer than a tick");
        auto o = runOld(spam, true, 170.0, [](bool p, double) { return p ? 8 : 0; });
        std::printf("  12-tap spam held 8 ticks @170fps+CBF: old fired %d presses, new %d\n", countPress(o), countPress(n));
    }

    // 6. Queue idles again once drained; next input is decided normally.
    {
        InputQueue q;
        q.start(true, 0.0, 1.0);
        CHECK(q.active(), "not active");
        CHECK(!q.poll(0.5).has_value(), "fired early");
        auto a = q.poll(1.0);
        CHECK(a && *a == true, "did not fire");
        CHECK(q.active() && !q.pending(), "should stay open until the press is undone");
        CHECK(q.push(false, 1.1, 0.25), "release after fire not accepted");
        auto b = q.poll(1.1 + 1.0);
        CHECK(b && *b == false, "release not replayed with the same shift");
        CHECK(!q.active(), "still active after gesture ended");
        CHECK(!q.push(true, 3.0, 0.25), "accepted input while idle");
    }
}

// ---------------------------------------------------------------------------
//  Added delay: previous Extreme (aim 0.5) vs new Extreme (earliest + margin)
//  over random safe ranges. Synthetic model, not an in-game measurement.
// ---------------------------------------------------------------------------
static void testAddedDelay() {
    std::printf("[added delay, synthetic]\n");
    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> lo(1, 20), len(1, 30);
    double oldSum = 0, newSum = 0, minSum = 0;
    int n = 0;
    for (int i = 0; i < 20000; i++) {
        int a = lo(rng), b = a + len(rng);
        auto f = [a, b](int d) { return (d >= a && d <= b) ? Verdict::Good : Verdict::Bad; };
        auto ro = searchDelay(SearchParams{50, 4, true, 0, 0.5f}, f);
        auto rn = searchDelay(SearchParams{50, 4, true, 1, 0.0f}, f);
        if (ro.delay == 0 || rn.delay == 0) continue;
        oldSum += ro.delay; newSum += rn.delay; minSum += a; n++;
    }
    std::printf("  avg delay (ticks @240): earliest possible %.2f | old extreme %.2f | new extreme %.2f\n",
                minSum / n, oldSum / n, newSum / n);
    CHECK(newSum < oldSum, "new not lower");
    CHECK(newSum / n - minSum / n <= 1.01, "new adds more than the margin");
}

static void testEscape() {
    std::printf("[escape samples]\n");
    // 240 TPS: commit 24, react 12, lateGap 3
    CHECK(escapeSamples(10, 24, 12, 3, 3).empty(), "death inside commit window must be the click's fault");
    CHECK(escapeSamples(24, 24, 12, 3, 3).empty(), "death exactly at commit counts as the click's fault");
    auto a = escapeSamples(40, 24, 12, 3, 3);
    CHECK(a.size() == 3 && a[0] == 12 && a[1] == 20 && a[2] == 37, "got %zu samples", a.size());
    for (int dead = 25; dead < 200; dead++) {
        auto v = escapeSamples(dead, 24, 12, 3, 3);
        bool ok = !v.empty() && v.size() <= 3;
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i] < 12 || v[i] >= dead) ok = false;
            for (size_t j = 0; j < i; j++) if (v[i] == v[j]) ok = false;
        }
        CHECK(ok, "bad samples for dead=%d", dead);
    }
    CHECK(escapeSamples(100, 24, 12, 3, 2).size() == 2, "sample cap ignored");
    // 480 TPS (everything scaled x2)
    CHECK(escapeSamples(40, 48, 24, 6, 3).empty(), "commit not scaled");
}

static void testSearchExtras() {
    std::printf("[search extras]\n");
    {   // earliest is reported before guard/margin move the choice
        Pattern pat{[](int d) { return (d == 5 || d >= 8) ? Verdict::Good : Verdict::Bad; }};
        auto r = run({16, 1, true, 1, 0.f}, pat);
        CHECK(r.earliest == 5 && r.delay >= 8, "earliest=%d delay=%d", r.earliest, r.delay);
    }
    {   // minDelay: probe beyond a window never re-tests delays below it
        Pattern pat{[](int d) { return d >= 23 ? Verdict::Good : Verdict::Bad; }};
        SearchParams sp{40, 3, false, 0, 0.f, 9};
        auto r = searchDelay(sp, [&](int d) { return pat(d); });
        CHECK(r.earliest == 23, "earliest=%d", r.earliest);
        bool below = false;
        for (auto& [d, n] : pat.calls) if (d < 9) below = true;
        CHECK(!below, "tested a delay below minDelay");
    }
    {   // minDelay past maxDelay: nothing to do
        Pattern pat{[](int) { return Verdict::Good; }};
        SearchParams sp{8, 1, false, 0, 0.f, 9};
        auto r = searchDelay(sp, [&](int d) { return pat(d); });
        CHECK(r.delay == 0 && pat.calls.empty(), "delay=%d", r.delay);
    }
}

static void testNarrowWindows() {
    std::printf("[narrow safe windows]\n");
    // Extreme-sized window (60 ticks, stride 4): every 1..3-tick-wide safe gap
    // near the click must be found, at its earliest tick.
    int missed = 0, total = 0;
    for (int start = 1; start <= 30; start++) {
        for (int width = 1; width <= 3; width++) {
            Pattern pat{[=](int d) { return (d >= start && d < start + width) ? Verdict::Good : Verdict::Bad; }};
            SearchParams sp{60, 4, false, 0, 0.f};
            sp.dense = 12;
            auto r = searchDelay(sp, [&](int d) { return pat(d); });
            if (start <= 12) {
                total++;
                if (r.earliest != start) missed++;
            }
        }
    }
    CHECK(missed == 0, "missed %d of the narrow gaps inside the dense range", missed);
    // and without the dense range, the old behavior misses most of them
    int oldMissed = 0;
    for (int start = 1; start <= 12; start++) {
        Pattern pat{[=](int d) { return d == start ? Verdict::Good : Verdict::Bad; }};
        SearchParams sp{60, 4, false, 0, 0.f};
        sp.dense = 0;
        auto r = searchDelay(sp, [&](int d) { return pat(d); });
        if (r.earliest != start) oldMissed++;
    }
    std::printf("  1-tick safe gap within 12 ticks, extreme window: old missed %d/12, new missed %d/%d\n",
                oldMissed, missed, total);
}

static void testLiveRetime() {
    std::printf("[live re-planning]\n");
    // press held (planned +10 ticks), release queued 7 ticks after the press
    InputQueue q;
    q.start(true, 0.0, 10 * TICK);
    q.push(false, 7 * TICK, TICK * 0.999);
    CHECK(q.nextPush() == true, "head should be the press");
    // live check: press is safe 4 ticks earlier than planned -> fire at 6
    q.shiftAll(-4 * TICK, 1 * TICK);
    auto a = q.poll(6 * TICK, 1e-6);
    CHECK(a && *a == true, "press not fired at its new time");
    CHECK(std::fabs(q.nextAt() - (6 + 7) * TICK) < 1e-9, "release did not move with the press: %.3f", q.nextAt());
    // live check: release unsafe at its time -> postpone 2 ticks, spacing behind it kept
    q.push(true, 9 * TICK, TICK * 0.999);   // another press queued
    double const gap = 0; (void)gap;
    q.shiftAll(2 * TICK);
    auto b = q.poll((6 + 7 + 2) * TICK, 1e-6);
    CHECK(b && *b == false, "release not fired after postponing");
    CHECK(q.nextPush() == true && q.pending(), "queued press lost");
    // never before notBefore
    InputQueue r;
    r.start(true, 5.0, 2.0);
    r.shiftAll(-10.0, 5.25);
    CHECK(std::fabs(r.nextAt() - 5.25) < 1e-9, "moved before notBefore: %.3f", r.nextAt());
}

// Drive liveStep over physics steps. safeAt(push, now) says whether firing now is safe.
struct LiveRun { std::vector<Fired> fired; };
static LiveRun runLive(std::vector<Input> extra, double arrive, int planned, int window, int need,
                       std::function<LiveVerdict(bool, double)> safeAt, int followWindow = 10,
                       std::vector<double> splits = {}) {
    InputQueue q;
    LiveState st;
    q.start(true, arrive, planned * TICK);
    st.startClick(arrive, TICK, arrive + planned * TICK, arrive + (window + 1) * TICK + 1e-4);
    auto starts = stepStarts(arrive + 80.0 * TICK, splits);
    LiveRun out;
    size_t next = 0;
    for (double now : starts) {
        if (now < arrive + 1e-9) continue;
        while (next < extra.size() && extra[next].t <= now + 1e-9) {
            q.push(extra[next].push, now, TICK * 0.999);
            next++;
        }
        auto f = liveStep(q, st, now, 1e-4,
                          [&](bool push) { return safeAt(push, now); },
                          [&](bool first) { return first ? need : 1; },
                          [&](bool) { return followWindow * TICK; });
        if (f) out.fired.push_back({*f, now});
    }
    return out;
}

static void testLive() {
    std::printf("[live re-planning step]\n");
    double const A = 10.0; // arrival time
    auto safeFrom = [A](double fromTick) {
        return [A, fromTick](bool, double now) {
            return now + 1e-6 >= A + fromTick * TICK ? LiveVerdict::Safe : LiveVerdict::Unsafe;
        };
    };
    {   // first safe at +7: need 1 -> fires at +7; need 2 -> +8 (margin)
        auto r1 = runLive({}, A, 10, 40, 1, safeFrom(7));
        auto r2 = runLive({}, A, 10, 40, 2, safeFrom(7));
        CHECK(!r1.fired.empty() && std::fabs(r1.fired[0].t - (A + 7 * TICK)) < 1e-6, "need1 fired at %.3f", r1.fired.empty() ? -1 : r1.fired[0].t);
        CHECK(!r2.fired.empty() && std::fabs(r2.fired[0].t - (A + 8 * TICK)) < 1e-6, "need2 fired at %.3f", r2.fired.empty() ? -1 : r2.fired[0].t);
    }
    {   // never safe -> fires at the end of the window, not before
        auto r = runLive({}, A, 10, 20, 2, [](bool, double) { return LiveVerdict::Unsafe; });
        CHECK(!r.fired.empty() && r.fired[0].t + 1e-6 >= A + 21 * TICK && r.fired[0].t <= A + 22 * TICK + 1e-6,
              "deadline fire at %.3f", r.fired.empty() ? -1 : r.fired[0].t);
    }
    {   // never fires on the click's own step
        auto r = runLive({}, A, 10, 40, 1, [](bool, double) { return LiveVerdict::Safe; });
        CHECK(!r.fired.empty() && r.fired[0].t > A + 1e-6, "fired on the click step");
    }
    {   // streak resets: safe at 5, unsafe at 6, safe from 7 -> need 2 fires at 8
        auto r = runLive({}, A, 10, 40, 2, [A](bool, double now) {
            double t = (now - A) / TICK;
            if (std::fabs(t - 6) < 1e-6) return LiveVerdict::Unsafe;
            return t + 1e-6 >= 5 ? LiveVerdict::Safe : LiveVerdict::Unsafe;
        });
        CHECK(!r.fired.empty() && std::fabs(r.fired[0].t - (A + 8 * TICK)) < 1e-6, "streak: fired at %.3f", r.fired.empty() ? -1 : r.fired[0].t);
    }
    {   // can't check (budget) -> follows the plan
        auto r = runLive({}, A, 9, 40, 1, [](bool, double) { return LiveVerdict::Unknown; });
        CHECK(!r.fired.empty() && std::fabs(r.fired[0].t - (A + 9 * TICK)) < 1e-6, "unknown: fired at %.3f", r.fired.empty() ? -1 : r.fired[0].t);
    }
    {   // release queued 5 ticks after the press: press safe from +8 (fires +8),
        // release unsafe until press+7 -> held 2 extra ticks; a later press keeps its gap to it
        std::vector<Input> extra = {{false, A + 5 * TICK}, {true, A + 9 * TICK}};
        auto r = runLive(extra, A, 12, 40, 1, [A](bool push, double now) {
            double t = (now - A) / TICK;
            if (push) return t + 1e-6 >= 8 ? LiveVerdict::Safe : LiveVerdict::Unsafe;
            return t + 1e-6 >= 15 ? LiveVerdict::Safe : LiveVerdict::Unsafe; // release safe from +15
        });
        CHECK(r.fired.size() == 3, "events fired: %zu", r.fired.size());
        if (r.fired.size() == 3) {
            CHECK(r.fired[0].push && std::fabs(r.fired[0].t - (A + 8 * TICK)) < 1e-6, "press at %.3f", r.fired[0].t);
            CHECK(!r.fired[1].push && std::fabs(r.fired[1].t - (A + 15 * TICK)) < 1e-6, "release at %.3f", r.fired[1].t);
            // second press arrived 4 ticks after the release -> still 4 ticks after it
            CHECK(r.fired[2].push && std::fabs(r.fired[2].t - r.fired[1].t - 4 * TICK) < 1e-6,
                  "rhythm after a held release: gap %.3f", r.fired[2].t - r.fired[1].t);
        }
    }
    {   // release that doesn't matter (cube) fires exactly on schedule, keeping hold length
        std::vector<Input> extra = {{false, A + 3 * TICK}};
        auto r = runLive(extra, A, 6, 40, 1, [A](bool push, double now) {
            if (!push) return LiveVerdict::OnSchedule;
            return (now - A) / TICK + 1e-6 >= 6 ? LiveVerdict::Safe : LiveVerdict::Unsafe;
        });
        CHECK(r.fired.size() == 2 && std::fabs(r.fired[1].t - r.fired[0].t - 3 * TICK) < 1e-6,
              "hold length changed: %.3f", r.fired.size() == 2 ? r.fired[1].t - r.fired[0].t : -1);
    }
    {   // CBF sub-steps in between: still fires at the first safe boundary, never before it
        std::vector<double> splits = {A + 2.3 * TICK, A + 6.6 * TICK};
        auto r = runLive({}, A, 10, 40, 1, safeFrom(6.6), 10, splits);
        CHECK(!r.fired.empty() && std::fabs(r.fired[0].t - (A + 6.6 * TICK)) < 1e-6, "cbf: fired at %.3f", r.fired.empty() ? -1 : r.fired[0].t);
    }
}

static void testTracker() {
    std::printf("[accuracy tracker]\n");
    // straight line: x = 10*(i+1), y = 2*(i+1)  (position after step i)
    auto line = [](int n) {
        std::vector<Point2> p;
        for (int i = 0; i < n; i++) p.push_back({10.f * (i + 1), 2.f * (i + 1)});
        return p;
    };
    {
        PredictionTracker t;
        t.start(100.0, 0.25, line(60), 24, 3.f);
        CHECK(!t.poll(100.0 + 24 * 0.25, 0, 0).has_value(), "checked too early");
        // after 25 steps the real player should be at path[24] = (250, 50)
        auto r = t.poll(100.0 + 25 * 0.25, 250.f, 50.f);
        CHECK(r && r->hit && r->err < 1e-3f, "exact match not a hit");
        CHECK(!t.active(), "still active");
    }
    {   // CBF sub-step: checkpoint reached mid-step -> interpolated
        PredictionTracker t;
        t.start(0.0, 0.25, line(60), 24, 3.f);
        double now = 25.5 * 0.25; // 25.5 steps done
        auto r = t.poll(now, 255.f, 51.f);
        CHECK(r && r->err < 1e-3f, "interpolation off: %.3f", r ? r->err : -1.f);
    }
    {   // miss beyond tolerance
        PredictionTracker t;
        t.start(0.0, 0.25, line(60), 10, 3.f);
        auto r = t.poll(11 * 0.25, 110.f, 30.f); // expected y=22
        CHECK(r && !r->hit && std::fabs(r->err - 8.f) < 1e-3f, "err=%.3f", r ? r->err : -1.f);
        CHECK(t.stats().checks == 1 && t.stats().hits == 0, "stats");
    }
    {   // invalidation and death bookkeeping
        PredictionTracker t;
        t.start(0.0, 0.25, line(60), 10, 3.f);
        t.invalidate();
        CHECK(!t.poll(100.0, 0, 0).has_value() && t.stats().dropped == 1, "invalidate");
        t.start(0.0, 0.25, line(60), 10, 3.f);
        t.died();
        CHECK(t.stats().checks == 1 && t.stats().deaths == 1 && t.stats().hits == 0, "death");
    }
    {   // short path: checkpoint clamps to the last recorded step
        PredictionTracker t;
        t.start(0.0, 0.25, line(8), 24, 3.f);
        auto r = t.poll(8 * 0.25, 80.f, 16.f);
        CHECK(r && r->hit, "clamped checkpoint");
    }
}

static void testCalibrator() {
    std::printf("[calibrator]\n");
    Calibrator c;
    CHECK(!c.window(10, 0.9f, 0.25f, 0.25f, 4.f).has_value(), "window with no samples");
    for (int i = 1; i <= 20; i++) c.add(0.1f * i); // 0.1 .. 2.0 frames
    auto w = c.window(10, 0.9f, 0.25f, 0.25f, 4.f);
    // p90 of 0.1..2.0 = 1.81, +0.25 = 2.06 -> rounded up to 2.25
    CHECK(w && std::fabs(*w - 2.25f) < 1e-4f, "window=%.3f", w ? *w : -1.f);
    CHECK(c.window(10, 0.9f, 0.25f, 0.25f, 1.5f).value_or(0) == 1.5f, "cap ignored");
    Calibrator d;
    d.deserialize(c.serialize());
    CHECK(d.count() == 20 && std::fabs(d.percentile(0.5f) - c.percentile(0.5f)) < 1e-3f, "round trip");
    d.deserialize("garbage,,1.5,x,0.75");
    CHECK(d.count() == 2, "bad tokens not skipped: %zu", d.count());
    for (int i = 0; i < 100; i++) c.add(1.f);
    CHECK(c.count() == Calibrator::MAX_SAMPLES, "ring not capped");
    c.add(-1.f); c.add(0.f);
    CHECK(c.count() == Calibrator::MAX_SAMPLES, "accepted invalid sample");
}

int main() {
    testSearch();
    testSearchExtras();
    testNarrowWindows();
    testLiveRetime();
    testLive();
    testTracker();
    testCalibrator();
    testEscape();
    testQueue();
    testAddedDelay();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
