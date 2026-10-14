// Profiler.h - lightweight per-frame hitch profiler (header-only).
//
// Times the two big per-frame phases (world streaming, world rendering) with
// steady_clock (QPC-backed on Windows, sub-microsecond), keeps a rolling window
// of frame times for avg / p99 / worst, and remembers what the worst frame was
// spent on (stream vs render vs other, and how many model loads / collision
// builds Stream() ran that frame). Smoothness is about the spikes, not the
// average FPS, so the worst frame and its breakdown are the headline numbers.

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>

class FrameProfiler {
public:
    enum Phase { P_STREAM, P_RENDER, P_PRESENT, P_COUNT };
    using clock = std::chrono::steady_clock;

    // One frame's breakdown.
    struct Sample {
        double total = 0, phase[P_COUNT] = {0, 0, 0};
        double loadMs = 0, colMs = 0;
        size_t loads = 0, cols = 0;
        double Other() const {
            double o = total - phase[P_STREAM] - phase[P_RENDER] - phase[P_PRESENT];
            return o > 0 ? o : 0;
        }
    };

    void FrameBegin() {
        m_frameStart = clock::now();
        m_cur = Sample{};
        m_ignore = false;
    }
    // Exclude this frame from hitch / worst tracking (screenshot saves etc.).
    void IgnoreFrame() { m_ignore = true; }
    void Begin(Phase p) { m_pStart[p] = clock::now(); }
    void End(Phase p) { m_cur.phase[p] += Ms(m_pStart[p]); }

    void SetStreamStats(size_t loads, double loadMs, size_t cols, double colMs) {
        m_cur.loads = loads; m_cur.loadMs = loadMs;
        m_cur.cols = cols;   m_cur.colMs = colMs;
    }

    // Closes the frame. Returns true when it was a hitch: slower than
    // hitchMs AND more than 2x the rolling average (so a uniformly slow
    // machine doesn't flag every frame). The first frames are warm-up.
    bool FrameEnd(double hitchMs) {
        m_cur.total = Ms(m_frameStart);
        m_last = m_cur;
        double avg = Avg();
        m_hist[m_pos] = m_cur.total;
        m_pos = (m_pos + 1) % kHist;
        if (m_count < kHist) m_count++;
        m_frames++;
        if (m_frames <= kWarmup || m_ignore) return false;
        if (m_cur.total > m_worstEver.total) m_worstEver = m_cur;
        bool hitch = m_cur.total >= hitchMs && m_cur.total > 2.0 * avg;
        if (hitch) m_hitches++;
        return hitch;
    }

    // Rolling window stats (last kHist frames).
    double Avg() const {
        if (!m_count) return 0;
        double s = 0;
        for (int i = 0; i < m_count; i++) s += m_hist[i];
        return s / m_count;
    }
    void Window(double& avg, double& p99, double& worst) const {
        avg = p99 = worst = 0;
        if (!m_count) return;
        double v[kHist];
        std::copy(m_hist, m_hist + m_count, v);
        std::sort(v, v + m_count);
        avg = Avg();
        worst = v[m_count - 1];
        p99 = v[(int)((m_count - 1) * 0.99)];
    }

    const Sample& Last() const { return m_last; }
    const Sample& WorstEver() const { return m_worstEver; }
    size_t Hitches() const { return m_hitches; }
    size_t Frames() const { return m_frames; }
    void ResetWorst() { m_worstEver = Sample{}; m_hitches = 0; }

private:
    static constexpr int kHist = 240;       // ~2-4 s of frames
    static constexpr size_t kWarmup = 30;   // skip startup frames
    static double Ms(clock::time_point t0) {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    }
    clock::time_point m_frameStart, m_pStart[P_COUNT];
    Sample m_cur, m_last, m_worstEver;
    double m_hist[kHist] = {};
    int m_pos = 0, m_count = 0;
    size_t m_frames = 0, m_hitches = 0;
    bool m_ignore = false;
};
