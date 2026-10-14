#pragma once
// Fe2d - screen-space 2D drawing for the frontend (the CSprite2d role):
// solid rects and textured quads in backbuffer pixels, alpha-blended,
// no depth. Begin() captures device state and End() restores it, so the
// world renderer's states are untouched.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <cstdint>

class Fe2d {
public:
    void Begin(IDirect3DDevice9* dev);
    void End();

    // ARGB colours (D3DCOLOR). Rects are x0,y0 (top-left) .. x1,y1.
    void DrawRect(float x0, float y0, float x1, float y1, uint32_t argb);
    void DrawTex(IDirect3DTexture9* tex, float x0, float y0, float x1, float y1,
                 uint32_t argb, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);

    // Textured quads sharing one texture (font glyphs): queue, then Flush.
    void QueueTex(float x0, float y0, float x1, float y1, uint32_t argb,
                  float u0, float v0, float u1, float v1);
    void Flush(IDirect3DTexture9* tex);

private:
    struct V { float x, y, z, rhw; DWORD c; float u, v; };
    static const DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    static const int kMaxQuads = 2048;
    IDirect3DDevice9* m_dev = nullptr;
    IDirect3DStateBlock9* m_saved = nullptr;
    V m_buf[kMaxQuads * 6];
    int m_count = 0;   // vertices queued
};
