// Example for siv::SimplexNoise.hpp (+ BlueNoise2D inside the same header)
//
// This demo generates a small "gallery" of BMP images so users can SEE what each noise does.
// It outputs ALL noise variants provided by your header:
//   - base Simplex 2D
//   - FBM (octaves): raw + normalized
//   - Ridged multifractal
//   - Domain-warped base
//   - Domain-warped FBM (raw + normalized idea via clamped remap)
//   - Domain-warped Ridged
//   - BlueNoise2D placement visualization (Poisson-disk-like points)
//
// Notes:
// - The library itself is freestanding; this demo is NOT (uses iostream/vector/algorithm/cmath).
// - Images are grayscale BMPs (24-bit).
// - You can tune frequency / octaves / warp parameters below.

#include <cassert>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm> // std::clamp

#include "SimplexNoise.hpp"

#pragma pack(push, 1)
struct BMPHeader {
    std::uint16_t bfType;
    std::uint32_t bfSize;
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;
    std::uint32_t biSize;
    std::int32_t  biWidth;
    std::int32_t  biHeight;
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;
    std::uint32_t biCompression;
    std::uint32_t biSizeImage;
    std::int32_t  biXPelsPerMeter;
    std::int32_t  biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};
static_assert(sizeof(BMPHeader) == 54, "");
#pragma pack(pop)

struct RGB {
    double r = 0.0, g = 0.0, b = 0.0;
    constexpr RGB() = default;
    explicit constexpr RGB(double v) noexcept : r(v), g(v), b(v) {}
    constexpr RGB(double rr, double gg, double bb) noexcept : r(rr), g(gg), b(bb) {}
};

class Image {
public:
    Image() = default;
    Image(std::size_t w, std::size_t h)
        : m_data(w* h), m_width((std::int32_t)w), m_height((std::int32_t)h) {}

    void set(std::int32_t x, std::int32_t y, const RGB& c) {
        if (!inBounds(y, x)) return;
        m_data[(std::size_t)y * (std::size_t)m_width + (std::size_t)x] = c;
    }

    std::int32_t width() const noexcept { return m_width; }
    std::int32_t height() const noexcept { return m_height; }

    bool saveBMP(const std::string& path) const {
        const std::int32_t rowSize = m_width * 3 + (m_width % 4);
        const std::uint32_t bmpsize = (std::uint32_t)(rowSize * m_height);
        const BMPHeader header{
            0x4d42,
            (std::uint32_t)(bmpsize + sizeof(BMPHeader)),
            0, 0, sizeof(BMPHeader), 40,
            m_width, m_height, 1, 24,
            0, bmpsize, 0, 0, 0, 0
        };

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return false;

        ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

        std::vector<std::uint8_t> line((std::size_t)rowSize);
        for (std::int32_t y = m_height - 1; y >= 0; --y) {
            std::size_t pos = 0;
            for (std::int32_t x = 0; x < m_width; ++x) {
                const RGB& col = m_data[(std::size_t)y * (std::size_t)m_width + (std::size_t)x];
                line[pos++] = toU8(col.b);
                line[pos++] = toU8(col.g);
                line[pos++] = toU8(col.r);
            }
            // padding already in line vector, left as-is (zeros)
            ofs.write(reinterpret_cast<const char*>(line.data()), (std::streamsize)line.size());
        }
        return true;
    }

private:
    std::vector<RGB> m_data;
    std::int32_t m_width = 0, m_height = 0;

    bool inBounds(std::int32_t y, std::int32_t x) const noexcept {
        return (0 <= y) && (y < m_height) && (0 <= x) && (x < m_width);
    }

    static std::uint8_t toU8(double x) noexcept {
        if (x <= 0.0) return 0;
        if (1.0 <= x) return 255;
        return (std::uint8_t)(x * 255.0 + 0.5);
    }
};

// -------------------- Helpers for visualization --------------------
static inline float clamp01(float v) noexcept {
    return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
}

// Visualize a noise that is *supposed* to be in [-1,1], but we clamp for safety.
static inline float to01_from_m11(float v) noexcept {
    // remap [-1,1] -> [0,1] then clamp (domain-warped FBM can overshoot)
    return clamp01(v * 0.5f + 0.5f);
}

// Visualize already-in-[0,1] signals.
static inline float to01_from_01(float v) noexcept {
    return clamp01(v);
}

// A tiny "slope-ish" helper if you want (optional).
static inline float absf(float v) noexcept { return (v < 0.f) ? -v : v; }

// -------------------- Gallery generator --------------------
enum class NoiseKind {
    Base2D,
    FBM_Raw,
    FBM_Normalized,
    Ridged,
    Warp_Base,
    Warp_FBM_Raw,
    Warp_Ridged,
    BlueNoisePoints
};

static const char* kindName(NoiseKind k) {
    switch (k) {
    case NoiseKind::Base2D:         return "base2D";
    case NoiseKind::FBM_Raw:        return "fbm_raw";
    case NoiseKind::FBM_Normalized: return "fbm_norm";
    case NoiseKind::Ridged:         return "ridged";
    case NoiseKind::Warp_Base:      return "warp_base";
    case NoiseKind::Warp_FBM_Raw:   return "warp_fbm_raw";
    case NoiseKind::Warp_Ridged:    return "warp_ridged";
    case NoiseKind::BlueNoisePoints:return "bluenoise_points";
    default:                        return "unknown";
    }
}

static float sampleKind(
    NoiseKind kind,
    const siv::SimplexNoise& n,
    float sx, float sz,           // already scaled coordinates (x*freq, z*freq)
    int octaves,
    float pers, float lac,
    float warpAmp, float warpFreq
) {
    switch (kind) {
    case NoiseKind::Base2D: {
        const float v = n.noise2D(sx, sz);              // ~[-1,1]
        return to01_from_m11(v);
    }
    case NoiseKind::FBM_Raw: {
        const float v = n.octave2D(sx, sz, octaves, pers, lac); // not normalized; may exceed [-1,1]
        return to01_from_m11(v); // clamp-remap for visualization
    }
    case NoiseKind::FBM_Normalized: {
        const float v = n.normalizedOctave2D(sx, sz, octaves, pers, lac); // ~[-1,1]
        return to01_from_m11(v);
    }
    case NoiseKind::Ridged: {
        const float v = n.ridged2D(sx, sz, octaves, lac, 2.0f, 1.0f); // ~[0,1]
        return to01_from_01(v);
    }
    case NoiseKind::Warp_Base: {
        const float v = n.domainWarp2D(sx, sz, warpAmp, warpFreq); // ~[-1,1]
        return to01_from_m11(v);
    }
    case NoiseKind::Warp_FBM_Raw: {
        const float v = n.domainWarpOctave2D(sx, sz, octaves, pers, lac, warpAmp, warpFreq);
        return to01_from_m11(v); // raw FBM under warp; clamp-remap
    }
    case NoiseKind::Warp_Ridged: {
        const float v = n.domainWarpRidged2D(sx, sz, octaves, warpAmp, warpFreq, lac, 2.0f, 1.0f);
        return to01_from_01(v);
    }
    default:
        return 0.0f;
    }
}

static bool renderNoiseBMP(
    const std::string& filename,
    NoiseKind kind,
    const siv::SimplexNoise& n,
    int W, int H,
    float frequency,             // "how many features across the image"
    int octaves,
    float persistence,
    float lacunarity,
    float warpAmp,
    float warpFreq
) {
    Image img((std::size_t)W, (std::size_t)H);

    // Convert frequency to scale factors
    const float fx = frequency / (float)W;
    const float fz = frequency / (float)H;

    for (int z = 0; z < H; ++z) {
        for (int x = 0; x < W; ++x) {
            const float sx = (float)x * fx;
            const float sz = (float)z * fz;
            const float v01 = sampleKind(kind, n, sx, sz, octaves, persistence, lacunarity, warpAmp, warpFreq);
            img.set(x, z, RGB((double)v01));
        }
    }

    const bool ok = img.saveBMP(filename);
    std::cout << (ok ? "saved  " : "FAILED ") << filename << "\n";
    return ok;
}

static bool renderBlueNoisePointsBMP(
    const std::string& filename,
    uint32_t worldSeed,
    uint32_t chunkSeed,
    int W, int H,
    float minDist,   // Poisson-like minimum spacing in "block units"
    int dotRadius    // dot radius in pixels
) {
    Image img((std::size_t)W, (std::size_t)H);

    // Fill background a bit dark so dots are visible
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img.set(x, y, RGB(0.08));

    siv::BlueNoise2D bn(worldSeed, chunkSeed);

    // Here we treat the whole image as one "chunk" in world-coordinates:
    // chunkMinX/Z = 0, chunkSize = W/H
    int count = 0;
    bn.forEachPointInRect(0, 0, W, H, minDist, [&](float px, float pz) {
        ++count;
        const int cx = (int)px;
        const int cz = (int)pz;

        // draw a small filled circle
        for (int dz = -dotRadius; dz <= dotRadius; ++dz) {
            for (int dx = -dotRadius; dx <= dotRadius; ++dx) {
                if (dx * dx + dz * dz > dotRadius * dotRadius) continue;
                const int x = cx + dx;
                const int y = cz + dz;
                if (0 <= x && x < W && 0 <= y && y < H) {
                    img.set(x, y, RGB(1.0)); // white dot
                }
            }
        }
    });
    std::cout << "points=" << count << "\n";

    const bool ok = img.saveBMP(filename);
    std::cout << (ok ? "saved  " : "FAILED ") << filename << "\n";
    return ok;
}

// -------------------- Tests similar to original Perlin demo --------------------
static void TestSimplexDeterminism() {
    const uint32_t seedA = 12345u;
    siv::SimplexNoise a(seedA);
    siv::SimplexNoise b;

    // state roundtrip
    b.deserialize(a.serialize());
    assert(a.noise3D(0.1f, 0.2f, 0.3f) == b.noise3D(0.1f, 0.2f, 0.3f));

    // reseed determinism
    a.reseed(67890u);
    b.reseed(67890u);
    assert(a.noise2D(0.11f, 0.22f) == b.noise2D(0.11f, 0.22f));
    assert(a.octave3D(0.1f, 0.2f, 0.3f, 4) == b.octave3D(0.1f, 0.2f, 0.3f, 4));

    std::cout << "Determinism tests: OK\n";
}

int main() {
    TestSimplexDeterminism();

    // -------------------- Gallery settings --------------------
    const int W = 512;
    const int H = 512;

    // "frequency": roughly how many noise "cells/features" across the image.
    // Higher -> more detailed (smaller features).
    const float frequencies[] = { 2.0f, 8.0f, 32.0f };

    // Octaves & FBM parameters
    const int   octaves = 6;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    // Domain warp parameters
    const float warpAmp = 1.0f; // try 0.5..1.5
    const float warpFreq = 0.8f; // try 0.5..1.2

    // Seeds to show "same algorithm, different worlds"
    const uint32_t seeds[] = { 0u, 1u, 12345u, 67890u };

    // Noise kinds (ALL provided variants)
    const NoiseKind kinds[] = {
        NoiseKind::Base2D,
        NoiseKind::FBM_Raw,
        NoiseKind::FBM_Normalized,
        NoiseKind::Ridged,
        NoiseKind::Warp_Base,
        NoiseKind::Warp_FBM_Raw,
        NoiseKind::Warp_Ridged,
    };

    std::cout << "Generating Simplex noise gallery...\n";
    std::cout << "Image: " << W << "x" << H << "\n";
    std::cout << "Octaves=" << octaves
        << " pers=" << persistence
        << " lac=" << lacunarity
        << " warpAmp=" << warpAmp
        << " warpFreq=" << warpFreq << "\n\n";

    // -------------------- Render all noises, all seeds, several frequencies --------------------
    for (uint32_t seed : seeds) {
        siv::SimplexNoise n(seed);

        for (float freq : frequencies) {
            for (NoiseKind k : kinds) {
                std::ostringstream name;
                name << "simplex_" << kindName(k)
                    << "_seed" << seed
                    << "_freq" << freq
                    << "_oct" << octaves
                    << ".bmp";

                renderNoiseBMP(
                    name.str(),
                    k, n,
                    W, H,
                    freq, octaves,
                    persistence, lacunarity,
                    warpAmp, warpFreq
                );
            }
        }
        std::cout << "---- done seed " << seed << " ----\n\n";
    }

    // -------------------- BlueNoise2D visualization (object placement) --------------------
    // This is a *placement* pattern, not height noise.
    // Think: tree candidate positions with minimum spacing.
    std::cout << "Generating BlueNoise2D placement maps...\n";

    const uint32_t worldSeed = 1337u;
    const float minDistances[] = { 4.0f, 6.0f, 8.0f }; // tighter -> denser trees
    for (float r : minDistances) {
        // You can derive chunkSeed from chunk coords in your engine; here we just show "different chunks".
        const uint32_t chunkSeeds[] = { 0u, 1u, 123u, 999u };

        for (uint32_t chunkSeed : chunkSeeds) {
            std::ostringstream name;
            name << "bluenoise_points_world" << worldSeed
                << "_chunk" << chunkSeed
                << "_r" << r
                << ".bmp";

            renderBlueNoisePointsBMP(name.str(), worldSeed, chunkSeed, W, H, r, /*dotRadius*/2);
        }
    }

    std::cout << "\nAll done.\n";
    std::cout << "Tip: Compare base2D vs warp_base, fbm_norm vs ridged vs warp_ridged.\n";
    return 0;
}