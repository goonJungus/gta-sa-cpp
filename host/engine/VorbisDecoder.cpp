#include "VorbisDecoder.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>

namespace {

int ILog(unsigned v) { int n = 0; while (v) { n++; v >>= 1; } return n; }

float Float32Unpack(uint32_t x) {
    float mant = (float)(x & 0x1FFFFF);
    const int exp = (int)((x & 0x7FE00000) >> 21);
    if (x & 0x80000000u) mant = -mant;
    return std::ldexp(mant, exp - 788);
}

int Lookup1Values(int entries, int dims) {
    int r = (int)std::floor(std::pow((double)entries, 1.0 / dims));
    while (std::pow((double)(r + 1), dims) <= entries) r++;
    while (r > 1 && std::pow((double)r, dims) > entries) r--;
    return r;
}

float InverseDb(int i) {
    // floor1_inverse_dB_table: geometric from 1.0649863e-07 to 1.0.
    static float table[256];
    static bool init = false;
    if (!init) {
        const double step = std::exp(std::log(1.0 / 1.0649863e-07) / 255.0);
        for (int k = 0; k < 256; k++) table[k] = (float)(1.0649863e-07 * std::pow(step, k));
        init = true;
    }
    return table[std::clamp(i, 0, 255)];
}

}  // namespace

uint32_t VorbisDecoder::Bits::Read(int count) {
    uint32_t v = 0;
    for (int i = 0; i < count; i++) {
        if (pos >= n) { eof = true; return v; }
        v |= (uint32_t)((d[pos] >> bit) & 1) << i;
        if (++bit == 8) { bit = 0; pos++; }
    }
    return v;
}

bool VorbisDecoder::Open(const uint8_t* data, size_t size) {
    m_d = data; m_size = size; m_off = 0;
    m_ready = m_eof = false;
    m_pcm.clear();
    m_partial.clear();
    m_pageSegs = m_pageSeg = 0;
    if (!ParseHeaders()) return false;
    m_dataStart = m_off;
    m_ready = true;
    Rewind();
    return true;
}

void VorbisDecoder::Rewind() {
    m_off = m_dataStart;
    m_pageSegs = m_pageSeg = 0;
    m_partial.clear();
    m_pcm.clear();
    m_eof = false;
    m_first = true;
    m_prevN = 0;
    m_acc.assign((size_t)m_channels, std::vector<float>());
}

bool VorbisDecoder::ReadPacket(std::vector<uint8_t>& out) {
    out.clear();
    for (;;) {
        if (m_pageSeg >= m_pageSegs) {
            if (m_off + 27 > m_size || std::memcmp(m_d + m_off, "OggS", 4) != 0) return false;
            m_pageSegs = m_d[m_off + 26];
            if (m_off + 27 + (size_t)m_pageSegs > m_size) return false;
            std::memcpy(m_segTable, m_d + m_off + 27, (size_t)m_pageSegs);
            m_pageDataOff = m_off + 27 + (size_t)m_pageSegs;
            size_t total = 0;
            for (int i = 0; i < m_pageSegs; i++) total += m_segTable[i];
            m_off = m_pageDataOff + total;
            m_pageSeg = 0;
            if (m_off > m_size) return false;
            continue;
        }
        const int lace = m_segTable[m_pageSeg++];
        out.insert(out.end(), m_d + m_pageDataOff, m_d + m_pageDataOff + lace);
        m_pageDataOff += (size_t)lace;
        if (lace < 255) return true;
    }
}

int VorbisDecoder::Huff(Bits& b, const Codebook& c) {
    if (c.tree.empty()) return -1;
    int node = 0;
    for (;;) {
        const int next = c.tree[(size_t)node * 2 + b.Read(1)];
        if (b.eof) return -1;
        if (next < 0) return -next - 1;
        if (next == 0) return -1;
        node = next;
    }
}

bool VorbisDecoder::ParseHeaders() {
    std::vector<uint8_t> pkt;
    // identification
    if (!ReadPacket(pkt) || pkt.size() < 30 || pkt[0] != 1 || std::memcmp(&pkt[1], "vorbis", 6) != 0) return false;
    m_channels = pkt[11];
    std::memcpy(&m_rate, &pkt[12], 4);
    m_bs[0] = 1 << (pkt[28] & 15);
    m_bs[1] = 1 << (pkt[28] >> 4);
    if (m_channels < 1 || m_channels > 8 || m_bs[0] > m_bs[1] || m_bs[0] < 64) return false;
    // comment
    if (!ReadPacket(pkt) || pkt.empty() || pkt[0] != 3) return false;
    // setup
    if (!ReadPacket(pkt) || pkt.size() < 8 || pkt[0] != 5) return false;
    Bits b;
    b.d = pkt.data() + 7; b.n = pkt.size() - 7;
    return ParseSetup(b);
}

bool VorbisDecoder::ParseSetup(Bits& b) {
    const int nBooks = (int)b.Read(8) + 1;
    m_books.assign((size_t)nBooks, Codebook());
    for (Codebook& c : m_books) {
        if (b.Read(24) != 0x564342) return false;
        c.dims = (int)b.Read(16);
        c.entries = (int)b.Read(24);
        if (c.dims < 1 || c.entries < 0 || c.entries > (1 << 22)) return false;
        c.lengths.assign((size_t)c.entries, 0);
        if (b.Read(1)) {   // ordered
            int cur = (int)b.Read(5) + 1, i = 0;
            while (i < c.entries) {
                const int n = (int)b.Read(ILog((unsigned)(c.entries - i)));
                if (i + n > c.entries || b.eof) return false;
                for (int k = 0; k < n; k++) c.lengths[(size_t)(i + k)] = (uint8_t)cur;
                i += n; cur++;
            }
        } else {
            const bool sparse = b.Read(1) != 0;
            for (int i = 0; i < c.entries; i++) {
                if (sparse && !b.Read(1)) c.lengths[(size_t)i] = 0;
                else c.lengths[(size_t)i] = (uint8_t)(b.Read(5) + 1);
            }
        }
        // Canonical code assignment, then a binary tree to read it bit by bit.
        c.tree.assign(2, 0);
        uint32_t avail[33] = {};
        bool haveFirst = false;
        for (int i = 0; i < c.entries; i++) {
            const int len = c.lengths[(size_t)i];
            if (!len) continue;
            uint32_t code;
            if (!haveFirst) {
                code = 0; haveFirst = true;
                for (int k = 1; k <= len; k++) avail[k] = 1u << (32 - k);
            } else {
                int z = len;
                while (z > 0 && !avail[z]) z--;
                if (z == 0) return false;
                code = avail[z]; avail[z] = 0;
                for (int y = len; y > z; y--) avail[y] = code + (1u << (32 - y));
            }
            int node = 0;
            for (int k = 0; k < len; k++) {
                const int bit = (int)((code >> (31 - k)) & 1);
                if (k == len - 1) {
                    c.tree[(size_t)node * 2 + (size_t)bit] = -(i + 1);
                } else {
                    int next = c.tree[(size_t)node * 2 + (size_t)bit];
                    if (next <= 0) {
                        next = (int)(c.tree.size() / 2);
                        c.tree.push_back(0); c.tree.push_back(0);
                        c.tree[(size_t)node * 2 + (size_t)bit] = next;
                    }
                    node = next;
                }
            }
        }
        c.lookupType = (int)b.Read(4);
        if (c.lookupType == 1 || c.lookupType == 2) {
            const float minV = Float32Unpack(b.Read(32)), delta = Float32Unpack(b.Read(32));
            const int valueBits = (int)b.Read(4) + 1;
            const bool seq = b.Read(1) != 0;
            const int count = c.lookupType == 1 ? Lookup1Values(c.entries, c.dims) : c.entries * c.dims;
            std::vector<float> mult((size_t)count);
            for (float& m : mult) m = (float)b.Read(valueBits);
            c.vectors.assign((size_t)c.entries * (size_t)c.dims, 0.0f);
            for (int e = 0; e < c.entries; e++) {
                float last = 0.0f;
                int idiv = 1;
                for (int k = 0; k < c.dims; k++) {
                    const int idx = c.lookupType == 1 ? (e / idiv) % count : e * c.dims + k;
                    const float v = mult[(size_t)idx] * delta + minV + last;
                    c.vectors[(size_t)e * (size_t)c.dims + (size_t)k] = v;
                    if (seq) last = v;
                    if (c.lookupType == 1) idiv *= count;
                }
            }
        } else if (c.lookupType != 0) return false;
        if (b.eof) return false;
    }
    const int nTimes = (int)b.Read(6) + 1;
    for (int i = 0; i < nTimes; i++) b.Read(16);
    const int nFloors = (int)b.Read(6) + 1;
    m_floors.assign((size_t)nFloors, Floor1());
    for (Floor1& f : m_floors) {
        if (b.Read(16) != 1) return false;   // only floor type 1
        const int parts = (int)b.Read(5);
        f.classList.resize((size_t)parts);
        int maxClass = -1;
        for (uint8_t& c : f.classList) { c = (uint8_t)b.Read(4); maxClass = std::max(maxClass, (int)c); }
        f.classDims.resize((size_t)maxClass + 1); f.classSub.resize((size_t)maxClass + 1);
        f.classMaster.assign((size_t)maxClass + 1, 0); f.classBooks.assign((size_t)maxClass + 1, {});
        for (int c = 0; c <= maxClass; c++) {
            f.classDims[(size_t)c] = (uint8_t)(b.Read(3) + 1);
            f.classSub[(size_t)c] = (uint8_t)b.Read(2);
            if (f.classSub[(size_t)c]) f.classMaster[(size_t)c] = (uint8_t)b.Read(8);
            for (int j = 0; j < (1 << f.classSub[(size_t)c]); j++) f.classBooks[(size_t)c].push_back((int)b.Read(8) - 1);
        }
        f.multiplier = (int)b.Read(2) + 1;
        f.rangeBits = (int)b.Read(4);
        f.x = {0, 1 << f.rangeBits};
        for (uint8_t c : f.classList)
            for (int j = 0; j < f.classDims[c]; j++) f.x.push_back((int)b.Read(f.rangeBits));
        const int n = (int)f.x.size();
        f.order.resize((size_t)n);
        for (int i = 0; i < n; i++) f.order[(size_t)i] = i;
        std::sort(f.order.begin(), f.order.end(), [&](int a, int c) { return f.x[(size_t)a] < f.x[(size_t)c]; });
        f.lowN.assign((size_t)n, 0); f.highN.assign((size_t)n, 1);
        for (int i = 2; i < n; i++) {
            int lo = 0, hi = 1;
            for (int k = 0; k < i; k++) {
                if (f.x[(size_t)k] < f.x[(size_t)i] && f.x[(size_t)k] > f.x[(size_t)lo]) lo = k;
                if (f.x[(size_t)k] > f.x[(size_t)i] && f.x[(size_t)k] < f.x[(size_t)hi]) hi = k;
            }
            f.lowN[(size_t)i] = lo; f.highN[(size_t)i] = hi;
        }
    }
    const int nRes = (int)b.Read(6) + 1;
    m_res.assign((size_t)nRes, Residue());
    for (Residue& r : m_res) {
        r.type = (int)b.Read(16);
        if (r.type > 2) return false;
        r.begin = (int)b.Read(24); r.end = (int)b.Read(24);
        r.partSize = (int)b.Read(24) + 1;
        r.classes = (int)b.Read(6) + 1;
        r.classBook = (int)b.Read(8);
        r.books.assign((size_t)r.classes, {});
        std::vector<int> cascade((size_t)r.classes);   // all cascades come first, then the books
        for (int& c : cascade) {
            const int low = (int)b.Read(3);
            const int high = b.Read(1) ? (int)b.Read(5) : 0;
            c = high * 8 + low;
        }
        for (size_t k = 0; k < r.books.size(); k++)
            for (int j = 0; j < 8; j++) r.books[k][(size_t)j] = (cascade[k] & (1 << j)) ? (int)b.Read(8) : -1;
        if (r.classBook >= nBooks) return false;
    }
    const int nMaps = (int)b.Read(6) + 1;
    m_maps.assign((size_t)nMaps, Mapping());
    for (Mapping& m : m_maps) {
        if (b.Read(16) != 0) return false;
        m.submaps = b.Read(1) ? (int)b.Read(4) + 1 : 1;
        if (b.Read(1)) {
            const int steps = (int)b.Read(8) + 1;
            for (int s = 0; s < steps; s++) {
                m.magnitude.push_back((int)b.Read(ILog((unsigned)m_channels - 1)));
                m.angle.push_back((int)b.Read(ILog((unsigned)m_channels - 1)));
            }
        }
        if (b.Read(2) != 0) return false;
        m.mux.assign((size_t)m_channels, 0);
        if (m.submaps > 1) for (int& v : m.mux) v = (int)b.Read(4);
        for (int s = 0; s < m.submaps; s++) {
            b.Read(8);
            m.floor.push_back((int)b.Read(8));
            m.residue.push_back((int)b.Read(8));
        }
    }
    const int nModes = (int)b.Read(6) + 1;
    m_modes.assign((size_t)nModes, Mode());
    for (Mode& m : m_modes) {
        m.blockflag = (int)b.Read(1);
        b.Read(16); b.Read(16);
        m.mapping = (int)b.Read(8);
        if (m.mapping >= nMaps) return false;
    }
    return !b.eof;
}

bool VorbisDecoder::DecodeFloor(Bits& b, const Floor1& f, int n2, std::vector<float>& out) {
    if (!b.Read(1)) return false;   // unused
    static const int kRange[4] = {256, 128, 86, 64};
    const int range = kRange[f.multiplier - 1];
    const int n = (int)f.x.size();
    std::vector<int> y((size_t)n, 0);
    y[0] = (int)b.Read(ILog((unsigned)range - 1));
    y[1] = (int)b.Read(ILog((unsigned)range - 1));
    int offset = 2;
    for (uint8_t cls : f.classList) {
        const int cdim = f.classDims[cls], cbits = f.classSub[cls];
        int cval = 0;
        if (cbits) cval = Huff(b, m_books[f.classMaster[cls]]);
        for (int j = 0; j < cdim; j++) {
            const int book = f.classBooks[cls][(size_t)(cval & ((1 << cbits) - 1))];
            cval >>= cbits;
            y[(size_t)(offset + j)] = book >= 0 ? std::max(0, Huff(b, m_books[(size_t)book])) : 0;
        }
        offset += cdim;
    }
    if (b.eof) return false;
    std::vector<int> fy((size_t)n, 0);
    std::vector<char> flag((size_t)n, 0);
    fy[0] = y[0]; fy[1] = y[1]; flag[0] = flag[1] = 1;
    auto renderPoint = [](int x0, int y0, int x1, int y1, int x) {
        const int dy = y1 - y0, adx = x1 - x0, ady = std::abs(dy);
        const int off = ady * (x - x0) / adx;
        return dy < 0 ? y0 - off : y0 + off;
    };
    for (int i = 2; i < n; i++) {
        const int lo = f.lowN[(size_t)i], hi = f.highN[(size_t)i];
        const int pred = renderPoint(f.x[(size_t)lo], fy[(size_t)lo], f.x[(size_t)hi], fy[(size_t)hi], f.x[(size_t)i]);
        const int val = y[(size_t)i];
        const int highroom = range - pred, lowroom = pred, room = std::min(highroom, lowroom) * 2;
        if (val) {
            flag[(size_t)lo] = flag[(size_t)hi] = flag[(size_t)i] = 1;
            if (val >= room) fy[(size_t)i] = highroom > lowroom ? val - lowroom + pred : pred - val + highroom - 1;
            else fy[(size_t)i] = (val & 1) ? pred - ((val + 1) >> 1) : pred + (val >> 1);
        } else {
            flag[(size_t)i] = 0;
            fy[(size_t)i] = pred;
        }
    }
    out.assign((size_t)n2, 0.0f);
    int hx = 0, lx = 0;
    int ly = fy[(size_t)f.order[0]] * f.multiplier, hy = ly;
    auto line = [&](int x0, int y0, int x1, int y1) {
        const int dy = y1 - y0, adx = x1 - x0;
        if (adx <= 0) return;
        int ady = std::abs(dy);
        const int base = dy / adx;
        const int sy = dy < 0 ? base - 1 : base + 1;
        ady -= std::abs(base) * adx;
        int x = x0, yy = y0, err = 0;
        if (x < n2) out[(size_t)x] = InverseDb(yy);
        for (x = x0 + 1; x < x1 && x < n2; x++) {
            err += ady;
            if (err >= adx) { err -= adx; yy += sy; } else yy += base;
            out[(size_t)x] = InverseDb(yy);
        }
    };
    for (int i = 1; i < n; i++) {
        const int idx = f.order[(size_t)i];
        if (!flag[(size_t)idx]) continue;
        hy = fy[(size_t)idx] * f.multiplier;
        hx = f.x[(size_t)idx];
        line(lx, ly, hx, hy);
        lx = hx; ly = hy;
    }
    for (int x = hx; x < n2; x++) out[(size_t)x] = InverseDb(hy);
    return true;
}

// y[i] = sum_k X[k] cos(2 pi / N (i + 1/2 + N/4)(k + 1/2)), N = n, X has n/2 values.
void VorbisDecoder::Imdct(const float* in, float* out, int n) {
    const int M = n / 2;
    // DCT-IV of size M by a complex FFT of size M/2.
    const int h = M / 2;
    std::vector<std::complex<double>> z((size_t)h);
    const double pi = 3.14159265358979323846;
    for (int k = 0; k < h; k++) {
        const double a = in[2 * k], c = in[M - 1 - 2 * k];
        z[(size_t)k] = std::complex<double>(a, c) * std::polar(1.0, -pi * (k + 0.25) / M);
    }
    // in place radix-2 FFT
    for (int i = 1, j = 0; i < h; i++) {
        int bit = h >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(z[(size_t)i], z[(size_t)j]);
    }
    for (int len = 2; len <= h; len <<= 1) {
        const std::complex<double> wl = std::polar(1.0, -2.0 * pi / len);
        for (int i = 0; i < h; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int k = 0; k < len / 2; k++) {
                const std::complex<double> u = z[(size_t)(i + k)], v = z[(size_t)(i + k + len / 2)] * w;
                z[(size_t)(i + k)] = u + v; z[(size_t)(i + k + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
    std::vector<double> d((size_t)M);
    for (int k = 0; k < h; k++) {
        const std::complex<double> v = z[(size_t)k] * std::polar(1.0, -pi * (k + 0.25) / M);
        d[(size_t)(2 * k)] = v.real();
        d[(size_t)(M - 1 - 2 * k)] = -v.imag();
    }
    for (int i = 0; i < n; i++) {
        const int j = i + M / 2;
        float v;
        if (j < M) v = (float)d[(size_t)j];
        else if (j < 2 * M) v = -(float)d[(size_t)(2 * M - 1 - j)];
        else v = -(float)d[(size_t)(j - 2 * M)];
        out[i] = v;
    }
}

bool VorbisDecoder::DecodeAudio(const std::vector<uint8_t>& pkt) {
    if (pkt.empty() || (pkt[0] & 1)) return true;   // not an audio packet
    Bits b;
    b.d = pkt.data(); b.n = pkt.size();
    b.Read(1);
    const int modeBits = ILog((unsigned)m_modes.size() - 1);
    const int modeIdx = (int)b.Read(modeBits);
    if (modeIdx >= (int)m_modes.size()) return false;
    const Mode& mode = m_modes[(size_t)modeIdx];
    const Mapping& map = m_maps[(size_t)mode.mapping];
    const int n = m_bs[mode.blockflag], n2 = n / 2;
    int prevFlag = 0, nextFlag = 0;
    if (mode.blockflag) { prevFlag = (int)b.Read(1); nextFlag = (int)b.Read(1); }
    const int ch = m_channels;
    std::vector<std::vector<float>> floors((size_t)ch), spec((size_t)ch, std::vector<float>((size_t)n2, 0.0f));
    std::vector<char> nonzero((size_t)ch, 0), floorOk((size_t)ch, 0);
    for (int c = 0; c < ch; c++) {
        const int sm = map.mux[(size_t)c];
        nonzero[(size_t)c] = floorOk[(size_t)c] = DecodeFloor(b, m_floors[(size_t)map.floor[(size_t)sm]], n2, floors[(size_t)c]) ? 1 : 0;
    }
    if (b.eof) return false;
    for (size_t s = 0; s < map.magnitude.size(); s++)
        if (nonzero[(size_t)map.magnitude[s]] || nonzero[(size_t)map.angle[s]])
            nonzero[(size_t)map.magnitude[s]] = nonzero[(size_t)map.angle[s]] = 1;
    // Residue, one submap at a time.
    for (int sm = 0; sm < map.submaps; sm++) {
        const Residue& r = m_res[(size_t)map.residue[(size_t)sm]];
        std::vector<int> chans;
        for (int c = 0; c < ch; c++) if (map.mux[(size_t)c] == sm) chans.push_back(c);
        if (chans.empty()) continue;
        const int vecs = r.type == 2 ? 1 : (int)chans.size();
        const int actual = r.type == 2 ? n2 * (int)chans.size() : n2;
        std::vector<std::vector<float>> v((size_t)vecs, std::vector<float>((size_t)actual, 0.0f));
        std::vector<char> dnd((size_t)vecs, 0);
        if (r.type == 2) {
            bool any = false;
            for (int c : chans) any = any || nonzero[(size_t)c];
            dnd[0] = !any;
        } else {
            for (size_t k = 0; k < chans.size(); k++) dnd[k] = !nonzero[(size_t)chans[k]];
        }
        const int lb = std::min(r.begin, actual), le = std::min(r.end, actual);
        const int toRead = le - lb;
        bool allDone = true;
        for (char d : dnd) allDone = allDone && d;
        if (toRead > 0 && !allDone) {
            const Codebook& cb = m_books[(size_t)r.classBook];
            const int cw = cb.dims;
            const int parts = toRead / r.partSize;
            std::vector<std::vector<int>> cls((size_t)vecs, std::vector<int>((size_t)parts + (size_t)cw, 0));
            for (int pass = 0; pass < 8; pass++) {
                int pc = 0;
                while (pc < parts) {
                    if (pass == 0)
                        for (int j = 0; j < vecs; j++) {
                            if (dnd[(size_t)j]) continue;
                            int temp = Huff(b, cb);
                            if (temp < 0) return false;
                            for (int i = cw - 1; i >= 0; i--) { cls[(size_t)j][(size_t)(i + pc)] = temp % r.classes; temp /= r.classes; }
                        }
                    for (int i = 0; i < cw && pc < parts; i++, pc++)
                        for (int j = 0; j < vecs; j++) {
                            if (dnd[(size_t)j]) continue;
                            const int book = r.books[(size_t)cls[(size_t)j][(size_t)pc]][(size_t)pass];
                            if (book < 0) continue;
                            const Codebook& vb = m_books[(size_t)book];
                            if (vb.vectors.empty()) return false;
                            const int off = lb + pc * r.partSize;
                            if (r.type == 0) {
                                const int step = r.partSize / vb.dims;
                                for (int i2 = 0; i2 < step; i2++) {
                                    const int e = Huff(b, vb);
                                    if (e < 0) return false;
                                    for (int k = 0; k < vb.dims; k++) v[(size_t)j][(size_t)(off + i2 + k * step)] += vb.vectors[(size_t)e * (size_t)vb.dims + (size_t)k];
                                }
                            } else {
                                for (int i2 = 0; i2 < r.partSize; i2 += vb.dims) {
                                    const int e = Huff(b, vb);
                                    if (e < 0) return false;
                                    for (int k = 0; k < vb.dims && off + i2 + k < actual; k++)
                                        v[(size_t)j][(size_t)(off + i2 + k)] += vb.vectors[(size_t)e * (size_t)vb.dims + (size_t)k];
                                }
                            }
                        }
                }
                if (b.eof) break;   // a truncated last packet decodes what it has
            }
        }
        if (r.type == 2) {
            for (size_t k = 0; k < chans.size(); k++)
                for (int i = 0; i < n2; i++) spec[(size_t)chans[k]][(size_t)i] = v[0][(size_t)i * chans.size() + k];
        } else {
            for (size_t k = 0; k < chans.size(); k++) spec[(size_t)chans[k]] = v[k];
        }
    }
    // Inverse channel coupling, last step first.
    for (size_t s = map.magnitude.size(); s-- > 0;) {
        std::vector<float>& m = spec[(size_t)map.magnitude[s]];
        std::vector<float>& a = spec[(size_t)map.angle[s]];
        for (int i = 0; i < n2; i++) {
            float M = m[(size_t)i], A = a[(size_t)i], nm, na;
            if (M > 0) { if (A > 0) { nm = M; na = M - A; } else { na = M; nm = M + A; } }
            else { if (A > 0) { nm = M; na = M + A; } else { na = M; nm = M - A; } }
            m[(size_t)i] = nm; a[(size_t)i] = na;
        }
    }
    // Window shape for this block.
    int leftStart = 0, leftEnd = n2, rightStart = n2, rightEnd = n;
    if (mode.blockflag && !prevFlag) { leftStart = n / 4 - m_bs[0] / 4; leftEnd = n / 4 + m_bs[0] / 4; }
    if (mode.blockflag && !nextFlag) { rightStart = n * 3 / 4 - m_bs[0] / 4; rightEnd = n * 3 / 4 + m_bs[0] / 4; }
    m_window.assign((size_t)n, 0.0f);
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < n; i++) {
        double w;
        if (i < leftStart) w = 0.0;
        else if (i < leftEnd) { const double s = std::sin((i - leftStart + 0.5) / (leftEnd - leftStart) * pi / 2); w = std::sin(pi / 2 * s * s); }
        else if (i < rightStart) w = 1.0;
        else if (i < rightEnd) { const double s = std::sin((rightEnd - i - 0.5) / (rightEnd - rightStart) * pi / 2); w = std::sin(pi / 2 * s * s); }
        else w = 0.0;
        m_window[(size_t)i] = (float)w;
    }
    // Centre of this block relative to the previous one: prevN/4 + n/4 samples later.
    const int advance = m_first ? 0 : m_prevN / 4 + n / 4;
    std::vector<float> y((size_t)n);
    // Samples already in the accumulators are relative to the previous block's centre.
    for (int c = 0; c < ch; c++) {
        std::vector<float>& acc = m_acc[(size_t)c];
        std::vector<float> in((size_t)n2, 0.0f);
        if (floorOk[(size_t)c]) for (int i = 0; i < n2; i++) in[(size_t)i] = spec[(size_t)c][(size_t)i] * floors[(size_t)c][(size_t)i];
        Imdct(in.data(), y.data(), n);
        // acc index 0 = previous block's centre; this block's window starts at advance - n/2.
        const int start = advance - n2;
        if (m_first) {
            acc.assign((size_t)n, 0.0f);
            for (int i = 0; i < n; i++) acc[(size_t)i] = y[(size_t)i] * m_window[(size_t)i];
        } else {
            const int need = start + n;
            if ((int)acc.size() < need) acc.resize((size_t)need, 0.0f);
            for (int i = 0; i < n; i++) if (start + i >= 0) acc[(size_t)(start + i)] += y[(size_t)i] * m_window[(size_t)i];
        }
    }
    // Emit everything before this block's centre, then drop it from the accumulators.
    const int centre = m_first ? n2 : advance;
    if (!m_first) {
        const int emitFrom = 0;
        const int emitTo = centre;   // previous centre is index 0
        for (int i = emitFrom; i < emitTo; i++)
            for (int c = 0; c < ch; c++) {
                const float f = i < (int)m_acc[(size_t)c].size() ? m_acc[(size_t)c][(size_t)i] : 0.0f;
                m_pcm.push_back((int16_t)std::clamp((int)std::lrint(f * 32767.0f), -32768, 32767));
            }
    }
    for (int c = 0; c < ch; c++) {
        std::vector<float>& acc = m_acc[(size_t)c];
        const int drop = m_first ? 0 : centre;
        if (!m_first) acc.erase(acc.begin(), acc.begin() + std::min<size_t>((size_t)drop, acc.size()));
        // Re-base so index 0 is this block's centre.
        if (m_first) acc.erase(acc.begin(), acc.begin() + n2);
    }
    m_first = false;
    m_prevN = n;
    return true;
}

size_t VorbisDecoder::Read(int16_t* out, size_t maxFrames) {
    if (!m_ready) return 0;
    const size_t ch = (size_t)m_channels;
    while (m_pcm.size() < maxFrames * ch && !m_eof) {
        std::vector<uint8_t> pkt;
        if (!ReadPacket(pkt)) { m_eof = true; break; }
        if (!DecodeAudio(pkt)) { m_eof = true; break; }
    }
    const size_t frames = std::min(maxFrames, m_pcm.size() / ch);
    for (size_t i = 0; i < frames * ch; i++) { out[i] = m_pcm.front(); m_pcm.pop_front(); }
    return frames;
}
