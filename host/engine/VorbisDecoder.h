#pragma once
// VorbisDecoder: a small Ogg Vorbis I decoder written from the format specification.
// Used for the radio and other streamed audio (retail streams are Ogg Vorbis, XOR scrambled on disk).
// Supports floor type 1, residue types 0-2, mapping type 0 and channel coupling, which is
// everything the retail streams use.

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class VorbisDecoder {
public:
    // The whole Ogg file in memory; it must stay alive while decoding.
    bool Open(const uint8_t* data, size_t size);
    int Channels() const { return m_channels; }
    int SampleRate() const { return m_rate; }
    // Interleaved 16 bit frames. Returns the number of frames written, 0 at the end.
    size_t Read(int16_t* out, size_t maxFrames);
    bool Eof() const { return m_eof && m_pcm.empty(); }
    void Rewind();

private:
    struct Codebook {
        int dims = 0, entries = 0;
        std::vector<uint8_t> lengths;
        // Huffman tree: node = {child0, child1}; leaves hold -(entry+1).
        std::vector<int32_t> tree;
        int lookupType = 0;
        std::vector<float> vectors;   // entries * dims when a lookup exists
    };
    struct Floor1 {
        std::vector<uint8_t> classList, classDims, classSub, classMaster;
        std::vector<std::vector<int>> classBooks;
        int multiplier = 1, rangeBits = 0;
        std::vector<int> x, order;    // x values and indices sorted by x
        std::vector<int> lowN, highN;
    };
    struct Residue {
        int type = 0, begin = 0, end = 0, partSize = 1, classes = 1, classBook = 0;
        std::vector<std::array<int, 8>> books;
    };
    struct Mapping {
        int submaps = 1;
        std::vector<int> mux, floor, residue, magnitude, angle;
    };
    struct Mode { int blockflag = 0, mapping = 0; };

    struct Bits {
        const uint8_t* d = nullptr; size_t n = 0, pos = 0; int bit = 0; bool eof = false;
        uint32_t Read(int count);
    };

    bool ReadPacket(std::vector<uint8_t>& out);
    bool ParseHeaders();
    bool ParseSetup(Bits& b);
    bool DecodeAudio(const std::vector<uint8_t>& pkt);
    int Huff(Bits& b, const Codebook& c);
    bool DecodeFloor(Bits& b, const Floor1& f, int n2, std::vector<float>& out);
    void Imdct(const float* in, float* out, int n);

    const uint8_t* m_d = nullptr;
    size_t m_size = 0, m_off = 0;
    int m_channels = 0, m_rate = 0;
    int m_bs[2] = {0, 0};
    std::vector<Codebook> m_books;
    std::vector<Floor1> m_floors;
    std::vector<Residue> m_res;
    std::vector<Mapping> m_maps;
    std::vector<Mode> m_modes;
    bool m_ready = false, m_eof = false;
    int m_prevFlag = 0;
    bool m_first = true;
    int m_prevN = 0;
    size_t m_dataStart = 0;
    std::vector<std::vector<float>> m_acc;   // per channel overlap accumulation
    std::vector<float> m_window;
    std::deque<int16_t> m_pcm;
    // leftover bytes of a packet split across pages
    std::vector<uint8_t> m_partial;
    int m_pageSegs = 0, m_pageSeg = 0;
    uint8_t m_segTable[255] = {};
    size_t m_pageDataOff = 0;
};
