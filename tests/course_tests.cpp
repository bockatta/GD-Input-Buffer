// Nova Run course checks: every section, in every mode, is beaten by its own
// path (nothing the icon touches kills it), sections join up, and they have
// obstacles in them. Build: c++ -std=c++20 -O2 -I../src course_tests.cpp
// Optional: ./a.out dump.json  -> the first sections, for a preview.
#include "NovaCourse.hpp"
#include <cstdio>
#include <string>

using namespace ncourse;

int main(int argc, char** argv) {
    int fails = 0, sections = 0, empty = 0, perMode[MODE_N] = {};
    for (uint32_t seed = 1; seed <= 60; seed++) {
        Course c(seed * 2654435761u);
        c.start(static_cast<int>(seed % MODE_N));
        float x = 0.f;
        float lastEnd = 0.f;
        bool first = true;
        for (int k = 0; k < 40; k++) {
            x += SEG_LEN;
            c.ensureAhead(x);
            for (auto const& s : c.segs) {
                if (!first && s.x1 <= lastEnd) continue;
                first = false;
                lastEnd = s.x1;
                sections++;
                perMode[s.mode]++;
                int const h = firstHit(s);
                if (h >= 0) {
                    fails++;
                    std::printf("HIT seed %u mode %d at x %.2f y %.2f\n", seed, s.mode, s.x0 + h * DX, s.y[h]);
                }
                if (s.spikes.empty() && s.blocks.empty() && s.wallLo.empty()) { empty++; std::printf("EMPTY mode %d\n", s.mode); }
            }
        }
        // sections join: each starts where the last ended, at the same height
        for (size_t i = 1; i < c.segs.size(); i++) {
            auto const& a = c.segs[i - 1];
            auto const& b = c.segs[i];
            if (std::fabs(a.x1 - b.x0) > 1e-3f) { fails++; std::printf("GAP seed %u\n", seed); }
            if (std::fabs(a.y.back() - b.y.front()) > 0.35f) { fails++; std::printf("JUMP seed %u %.2f -> %.2f\n", seed, a.y.back(), b.y.front()); }
            if (a.nextMode != b.mode) { fails++; std::printf("PORTAL seed %u\n", seed); }
            if (a.mode == b.mode) { fails++; std::printf("SAME MODE twice seed %u\n", seed); }
        }
    }
    std::printf("%d sections (cube %d ship %d ball %d ufo %d wave %d swing %d robot %d spider %d), %d without obstacles, %d fails\n",
                sections, perMode[CUBE], perMode[SHIP], perMode[BALL], perMode[UFO], perMode[WAVE], perMode[SWING], perMode[ROBOT],
                perMode[SPIDER], empty, fails);

    if (argc > 1) {
        FILE* f = std::fopen(argv[1], "w");
        Course c(12345u);
        c.start(CUBE);
        std::vector<Segment> all;
        float seen = -1.f;
        for (int k = 0; k < 9; k++) {
            c.ensureAhead(SEG_LEN * static_cast<float>(k));
            for (auto const& s : c.segs) if (s.x0 > seen) { all.push_back(s); seen = s.x0; }
        }
        std::fprintf(f, "[");
        bool firstSeg = true;
        for (auto const& s : all) {
            std::fprintf(f, "%s{\"mode\":%d,\"x0\":%g,\"x1\":%g,\"next\":%d,\"portalY\":%g,\"y\":[", firstSeg ? "" : ",", s.mode, s.x0, s.x1, s.nextMode, s.portalY);
            firstSeg = false;
            for (int i = 0; i < s.count(); i++) std::fprintf(f, "%s%.3f", i ? "," : "", s.y[i]);
            std::fprintf(f, "],\"rot\":[");
            for (int i = 0; i < s.count(); i++) std::fprintf(f, "%s%.1f", i ? "," : "", s.rot[i]);
            std::fprintf(f, "],\"lo\":[");
            for (size_t i = 0; i < s.wallLo.size(); i++) std::fprintf(f, "%s%.3f", i ? "," : "", s.wallLo[i]);
            std::fprintf(f, "],\"hi\":[");
            for (size_t i = 0; i < s.wallHi.size(); i++) std::fprintf(f, "%s%.3f", i ? "," : "", s.wallHi[i]);
            std::fprintf(f, "],\"spikes\":[");
            for (size_t i = 0; i < s.spikes.size(); i++) std::fprintf(f, "%s[%g,%g,%d]", i ? "," : "", s.spikes[i].x, s.spikes[i].base, s.spikes[i].down ? 1 : 0);
            std::fprintf(f, "],\"blocks\":[");
            for (size_t i = 0; i < s.blocks.size(); i++) std::fprintf(f, "%s[%g,%g,%g,%g]", i ? "," : "", s.blocks[i].x, s.blocks[i].y, s.blocks[i].w, s.blocks[i].h);
            std::fprintf(f, "]}");
        }
        std::fprintf(f, "]");
        std::fclose(f);
    }
    return fails ? 1 : 0;
}
