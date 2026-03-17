//	siv::Simplex
//	Simplex noise library with additional features for C++14 or above
//
//	Copyright (C) 2013-2021 Ryo Suzuki <reputeless@gmail.com>
//	Copyright (C) 2026 setbe
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files(the
// "Software"), to deal 	in the Software without restriction, including
// without limitation the rights 	to use, copy, modify, merge, publish,
// distribute, sublicense, and / or sell 	copies of the Software, and to
// permit persons to whom the Software is 	furnished to do so, subject to
// the
// following conditions :
//
//	The above copyright notice and this permission notice shall be included
// in
// 	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
// THE 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN 	THE SOFTWARE.
//
//----------------------------------------------------------------------------------------
//
//  See original library here: https://github.com/Reputeless/PerlinNoise

#pragma once
//==================================================================================================
// Documentation (General Usage)
//==================================================================================================
//
// Overview
// --------
// This header provides a freestanding (no <cmath>, no STL required) implementation of:
//   1) Simplex Noise in 2D and 3D (continuous noise, ~[-1, 1]).
//   2) FBM (fractal Brownian motion) octaves (layered noise).
//   3) Ridged multifractal (mountain-like ridges).
//   4) Domain warping helpers (coordinate-space displacement for richer macro shapes).
//   5) BlueNoise2D (deterministic, tile-free, blue-noise-like Poisson-disk distribution)
//      for object placement (trees, rocks, flowers, etc.).
//   6) VoronoiNoise2D / VoronoiNoise3D (deterministic cellular feature queries:
//      nearest owner, F1/F2, boundary factor, and small fixed-size top-k blending weights).
//
// Determinism
// -----------
// All functions are deterministic given the same seeds and inputs. This is important for
// chunk-based procedural generation and reproducible worlds.
//
// Coordinate Space
// ----------------
// You decide the units. Typically:
//   - Use "world block coordinates" (integers) for placement decisions.
//   - Use scaled "world coordinates" (floats) for noise sampling, e.g. x * frequency.
//
// Ranges
// ------
// - noise2D/noise3D: approximately in [-1, 1].
// - octave2D/octave3D: sum of octaves, NOT normalized; range depends on oct/persistence.
// - normalizedOctave*: octave divided by max amplitude; more stable range near [-1, 1].
// - ridged*: roughly [0, 1] (depends on parameters; not strictly clamped).
// - remap01(v): maps [-1, 1] -> [0, 1].
// - Voronoi queries return squared distances (F1/F2/... in distance^2 units) for speed.
//
// Seeding
// -------
// - SimplexNoise::reseed(seed) sets the permutation table.
// - serialize()/deserialize() allow saving/restoring the permutation state.
//
// Typical Parameters
// ------------------
// - For FBM: oct=4..8, persistence=0.45..0.6, lacunarity=2.0.
// - For ridged: oct=5..8, lac=2.0, gain=2.0, offset=1.0.
// - For domain warp: warpAmp=0.3..1.5, warpFreq=0.5..2.0.
//
// Usage Examples
// --------------
// Simplex noise (base):
//   siv::SimplexNoise n(12345);
//   float h = n.noise2D(x * 0.01f, z * 0.01f);   // ~[-1,1]
//
// FBM (normalized):
//   float f = n.normalizedOctave2D(x*0.005f, z*0.005f, 6, 0.5f, 2.0f); // ~[-1,1]
//
// Ridged mountains:
//   float m = n.ridged2D(x*0.002f, z*0.002f, 6); // ~[0,1]
//
// Domain-warped ridged:
//   float mw = n.domainWarpRidged2D(x*0.002f, z*0.002f, 6, /*warpAmp*/1.0f, /*warpFreq*/0.8f);
//
// Blue-noise-like placement:
//   siv::BlueNoise2D bn(worldSeed, chunkSeed);
//   bn.forEachPointInReck(chunkMinX, chunkMinZ, 16, 16, /*minDist*/5.5f, [&](float px, float pz){
//       // candidate placement at (px,pz)
//   });
//
// Notes
// -----
// - Prefer normalizedOctave* when you will threshold or combine multiple noises.
// - Domain warping is most effective on low-frequency signals (macro shapes).
// - For performance, keep frequencies and octaves reasonable; cache biome masks per chunk.



//==================================================================================================
// Documentation (Minecraft-like World Generation Guide)
//==================================================================================================
//
// Goals
// -----
// For a Minecraft-like voxel world you typically need:
//   - Heightmap / terrain surface variation (2D).
//   - Biome fields (2D masks: temperature/humidity/continentalness-like fields).
//   - Mountains / ridges (2D).
//   - Caves / density field (3D).
//   - Object placement: trees, rocks, flowers, structures (blue-noise-like / Poisson-disk).
//
// Recommended High-Level Pipeline
// -------------------------------
// 1) Macro terrain (continents / large shapes):
//      macro = domainWarpOctave2D(x*f0, z*f0, oct=4..6, pers~0.5, lac=2,
//                                 warpAmp~0.8..1.5, warpFreq~0.5..1.0)
//    Use low frequency f0 (e.g. 1/1024..1/256).
//
// 2) Base height (plains / rolling hills):
//      base = normalizedOctave2D(x*f1, z*f1, oct=5..7, pers~0.5, lac=2)
//    f1 is medium (e.g. 1/256..1/64).
//
// 3) Mountains (ridges):
//      ridges = domainWarpRidged2D(x*fm, z*fm, oct=5..8,
//                                  warpAmp~0.8..1.2, warpFreq~0.6..1.0,
//                                  lac=2, gain=2, offset=1)
//    fm is lower than base (e.g. 1/512..1/128).
//
// 4) Combine into final height:
//      height = seaLevel + baseScale * base + mountainScale * mountainMask * ridges + detail
//    where mountainMask can come from macro/biome/continentalness-like fields.
//    detail = small-scale normalizedOctave2D(x*f2, z*f2, oct=3..5) with higher frequency.
//
// 5) Biomes (2D):
//      temp = normalizedOctave2D(x*ft, z*ft, 4..6)
//      humid= normalizedOctave2D(x*fh, z*fh, 4..6)
//      cont = macro (or another low-freq field)
//    Classify biome using thresholds and/or smoothstep curves.
//
// 6) Caves (3D density field):
//      d = domainWarpOctave3D(x*fc, y*fc, z*fc, 4..6,
//                             pers~0.55, lac=2, warpAmp~0.6..1.0, warpFreq~1.0)
//      cave = (d > threshold) or (abs(d) < band) depending on your style.
//    Use biome/height/erosion masks to disable caves near surface if desired.
//
// 7) Trees / Objects (BlueNoise2D):
//      - Use BlueNoise2D to generate candidate points with a minimum distance r.
//      - Apply biome mask + slope + height checks.
//      - Use deterministic RNG (hash) for variations.
//
// Chunk-Safe Seeding Strategy
// ---------------------------
// For deterministic worlds, use two levels of seeding:
//   - SimplexNoise: seeded by world seed (or per-layer seed derived from it).
//   - BlueNoise2D: world seed + per-chunk derived seed.
// Example:
//   uint32_t chunkSeed = hash2D(worldSeed, chunkX, chunkZ) ^ 0xA5A5A5A5;
//   BlueNoise2D bn(worldSeed, chunkSeed);
//
// Tree Placement Recipe (Practical)
// ---------------------------------
// 1) Choose minimum spacing r by biome:
//      plains: r=6..9, forest: r=4..7, dense: r=3.5..6
// 2) Iterate candidates inside the chunk:
//      bn.forEachPointInRect(chunkMinX, chunkMinZ, 16, 16, r, callback)
// 3) For each candidate (px,pz):
//      - compute surface Y (heightmap)
//      - reject if underwater or too steep (slope)
//      - compute biome/density mask to modulate probability
//      - optionally do a final hash-based rejection to vary density:
//            if (bn.rand01((int)px, (int)pz) > density01) reject;
//      - otherwise spawn tree/feature.
//
// Avoiding Border Artifacts
// -------------------------
// The provided BlueNoise2D method is inherently border-safe because acceptance checks
// sample neighboring cells across chunk boundaries deterministically.
//
// Performance Notes
// -----------------
// - Keep tree candidate loops small: r sets the number of candidates; larger r -> fewer candidates.
// - Cache heightmap/biome masks per chunk; do not re-sample multiple times per block.
// - Prefer low octaves for macro fields; add detail only where needed.
//
// Tuning Tips
// -----------
// - If terrain looks "too noisy": reduce high-frequency octaves or detail amplitude.
// - If patterns repeat: increase warpAmp slightly and/or use lower warpFreq for macro.
// - If mountains are too spiky: reduce gain or increase baseScale relative to ridges.
// - If caves are too common: raise threshold or reduce fc / octaves.
//
//==================================================================================================
#include <stdint.h>

namespace siv {
    // freestanding floorf
    static int fastfloor(float x) noexcept {
        const int i = (int)x;
        return (x < (float)i) ? (i - 1) : i;
    }

    static float saturate01(float v) noexcept {
        return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
    }

    struct SimplexNoise {
        SimplexNoise() noexcept { reset(); }
        explicit SimplexNoise(uint32_t seed) noexcept { reseed(seed); }

        // -------------------- Public API --------------------
        void reseed(uint32_t seed) noexcept {
            // init 0..255
            for (int i = 0; i < 256; ++i) m_perm[i] = (uint8_t)i;

            // xorshift32
            uint32_t x = seed ? seed : 2463534242u;
            for (int i = 0; i < 10; ++i) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; }

            // Fisher-Yates
            for (int i = 255; i > 0; --i) {
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                uint32_t j = x % (uint32_t)(i + 1);
                uint8_t t = m_perm[i];
                m_perm[i] = m_perm[j];
                m_perm[j] = t;
            }

            // duplicate to 512 to reduce masking
            for (int i = 0; i < 256; ++i) m_perm512[i] = m_perm512[i + 256] = m_perm[i];
        }

        const uint8_t* serialize() const noexcept { return m_perm; }
        void deserialize(const uint8_t state[256]) noexcept {
            for (int i = 0; i < 256; ++i) m_perm[i] = state[i];
            for (int i = 0; i < 256; ++i) m_perm512[i] = m_perm512[i + 256] = m_perm[i];
        }

        // returns ~[-1,1]
        float noise2D(float x, float y) const noexcept {
            // Skew to simplex grid
            const float s = (x + y) * F2;
            const int i = ::siv::fastfloor(x + s);
            const int j = ::siv::fastfloor(y + s);

            const float t = (float)(i + j) * G2;
            const float X0 = (float)i - t;
            const float Y0 = (float)j - t;
            const float x0 = x - X0;
            const float y0 = y - Y0;

            // simplex corner ordering (triangle)
            int i1, j1;
            if (x0 > y0) { i1 = 1; j1 = 0; }
            else { i1 = 0; j1 = 1; }

            // Offsets for remaining corners
            const float x1 = x0 - (float)i1 + G2;
            const float y1 = y0 - (float)j1 + G2;
            const float x2 = x0 - 1.0f + 2.0f * G2;
            const float y2 = y0 - 1.0f + 2.0f * G2;

            // Hash gradients
            const int ii = i & 255;
            const int jj = j & 255;
            const uint8_t gi0 = m_perm512[ii + m_perm512[jj]] % 8;
            const uint8_t gi1 = m_perm512[ii + i1 + m_perm512[jj + j1]] % 8;
            const uint8_t gi2 = m_perm512[ii + 1 + m_perm512[jj + 1]] % 8;

            float n0 = 0.f, n1 = 0.f, n2 = 0.f;

            float t0 = 0.5f - x0 * x0 - y0 * y0;
            if (t0 > 0.f) {
                t0 *= t0;
                n0 = t0 * t0 * dot2(grad2[gi0], x0, y0);
            }

            float t1 = 0.5f - x1 * x1 - y1 * y1;
            if (t1 > 0.f) {
                t1 *= t1;
                n1 = t1 * t1 * dot2(grad2[gi1], x1, y1);
            }

            float t2 = 0.5f - x2 * x2 - y2 * y2;
            if (t2 > 0.f) {
                t2 *= t2;
                n2 = t2 * t2 * dot2(grad2[gi2], x2, y2);
            }

            // Scale to approx [-1,1]
            return 70.0f * (n0 + n1 + n2);
        }

        // returns ~[-1,1]
        float noise3D(float x, float y, float z) const noexcept {
            const float s = (x + y + z) * F3;
            const int i = ::siv::fastfloor(x + s);
            const int j = ::siv::fastfloor(y + s);
            const int k = ::siv::fastfloor(z + s);

            const float t = (float)(i + j + k) * G3;
            const float X0 = (float)i - t;
            const float Y0 = (float)j - t;
            const float Z0 = (float)k - t;

            const float x0 = x - X0;
            const float y0 = y - Y0;
            const float z0 = z - Z0;

            // Rank ordering to pick tetrahedron
            int i1, j1, k1;
            int i2, j2, k2;

            if (x0 >= y0) {
                if (y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0;  i2 = 1; j2 = 1; k2 = 0; } // X Y Z
                else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0;  i2 = 1; j2 = 0; k2 = 1; } // X Z Y
                else               { i1 = 0; j1 = 0; k1 = 1;  i2 = 1; j2 = 0; k2 = 1; } // Z X Y
            }
            else {
                if (y0 < z0)      { i1 = 0; j1 = 0; k1 = 1;  i2 = 0; j2 = 1; k2 = 1; } // Z Y X
                else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0;  i2 = 0; j2 = 1; k2 = 1; } // Y Z X
                else              { i1 = 0; j1 = 1; k1 = 0;  i2 = 1; j2 = 1; k2 = 0; } // Y X Z
            }

            const float x1 = x0 - (float)i1 + G3;
            const float y1 = y0 - (float)j1 + G3;
            const float z1 = z0 - (float)k1 + G3;

            const float x2 = x0 - (float)i2 + 2.0f * G3;
            const float y2 = y0 - (float)j2 + 2.0f * G3;
            const float z2 = z0 - (float)k2 + 2.0f * G3;

            const float x3 = x0 - 1.0f + 3.0f * G3;
            const float y3 = y0 - 1.0f + 3.0f * G3;
            const float z3 = z0 - 1.0f + 3.0f * G3;

            const int ii = i & 255;
            const int jj = j & 255;
            const int kk = k & 255;

            const uint8_t gi0 = m_perm512[ii + m_perm512[jj + m_perm512[kk]]] % 12;
            const uint8_t gi1 = m_perm512[ii + i1 + m_perm512[jj + j1 + m_perm512[kk + k1]]] % 12;
            const uint8_t gi2 = m_perm512[ii + i2 + m_perm512[jj + j2 + m_perm512[kk + k2]]] % 12;
            const uint8_t gi3 = m_perm512[ii + 1 + m_perm512[jj + 1 + m_perm512[kk + 1]]] % 12;

            float n0 = 0.f, n1 = 0.f, n2 = 0.f, n3 = 0.f;

            float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
            if (t0 > 0.f) { t0 *= t0; n0 = t0 * t0 * dot3(grad3[gi0], x0, y0, z0); }

            float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
            if (t1 > 0.f) { t1 *= t1; n1 = t1 * t1 * dot3(grad3[gi1], x1, y1, z1); }

            float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
            if (t2 > 0.f) { t2 *= t2; n2 = t2 * t2 * dot3(grad3[gi2], x2, y2, z2); }

            float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
            if (t3 > 0.f) { t3 *= t3; n3 = t3 * t3 * dot3(grad3[gi3], x3, y3, z3); }

            // Scale to approx [-1,1]
            return 32.0f * (n0 + n1 + n2 + n3);
        }

        static float remap01(float v) noexcept { return v * 0.5f + 0.5f; }

        // -------------------- Octaves (FBM) --------------------
        // pers = persistence (typically 0.5), lac = lacunarity (typically 2.0)

        float octave2D(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            float sum = 0.f;
            float amp = 1.f;
            for (int i = 0; i < oct; ++i) {
                sum += noise2D(x, y) * amp;
                x *= lac; y *= lac;
                amp *= pers;
            }
            return sum; // not normalized
        }

        float octave3D(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            float sum = 0.f;
            float amp = 1.f;
            for (int i = 0; i < oct; ++i) {
                sum += noise3D(x, y, z) * amp;
                x *= lac; y *= lac; z *= lac;
                amp *= pers;
            }
            return sum; // not normalized
        }

        // max amplitude
        static float maxAmp(int oct, float pers) noexcept {
            float s = 0.f, a = 1.f;
            for (int i = 0; i < oct; ++i) { s += a; a *= pers; }
            return s;
        }

        float normalizedOctave2D(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            const float denom = maxAmp(oct, pers);
            return (denom > 0.f) ? (octave2D(x, y, oct, pers, lac) / denom) : 0.f;
        }

        float normalizedOctave3D(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            const float denom = maxAmp(oct, pers);
            return (denom > 0.f) ? (octave3D(x, y, z, oct, pers, lac) / denom) : 0.f;
        }

        // 0..1 convenience
        float octave2D_01(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            return remap01(octave2D(x, y, oct, pers, lac));
        }
        float octave3D_01(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            return remap01(octave3D(x, y, z, oct, pers, lac));
        }
        float normalizedOctave2D_01(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            return remap01(normalizedOctave2D(x, y, oct, pers, lac));
        }
        float normalizedOctave3D_01(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept {
            return remap01(normalizedOctave3D(x, y, z, oct, pers, lac));
        }

        // -------------------- Ridged multifractal --------------------
        // typical: oct=5..8, lac=2.0, gain=2.0, offset=1.0
        // roughly returns [0..1]

        float ridged2D(float x, float y, int oct,
            float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept
        {
            float sum = 0.f;
            float amp = 0.5f;
            float weight = 1.f;

            for (int i = 0; i < oct; ++i) {
                float n = noise2D(x, y);          // ~[-1,1]
                n = offset - absf(n);             // [0..offset] (roughly)
                n *= n;                           // highlights ridges
                n *= weight;                      // feedback

                sum += n * amp;

                // next octave
                weight = n * gain;
                if (weight > 1.f) weight = 1.f;   // clamp
                x *= lac; y *= lac;
                amp *= 0.5f;
            }
            return sum;
        }

        float ridged3D(float x, float y, float z, int oct,
            float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept
        {
            float sum = 0.f;
            float amp = 0.5f;
            float weight = 1.f;

            for (int i = 0; i < oct; ++i) {
                float n = noise3D(x, y, z);
                n = offset - absf(n);
                n *= n;
                n *= weight;

                sum += n * amp;

                weight = n * gain;
                if (weight > 1.f) weight = 1.f;
                x *= lac; y *= lac; z *= lac;
                amp *= 0.5f;
            }
            return sum;
        }

        // -------------------- Domain warp helpers --------------------
        // warpAmp - amplitude of displacement in 'coordinate space' (not in the sense of noise)
        // warpFreq - warp frequency (typically 0.5..2.0 relative to the base)

        float domainWarp2D(float x, float y,
            float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept
        {
            const float wx = noise2D(x * warpFreq + 19.19f, y * warpFreq + 73.73f);
            const float wy = noise2D(x * warpFreq + 101.01f, y * warpFreq + 7.07f);
            return noise2D(x + wx * warpAmp, y + wy * warpAmp);
        }

        // warped FBM (useful for terrain)
        float domainWarpOctave2D(float x, float y, int oct,
            float pers = 0.5f, float lac = 2.0f,
            float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept
        {
            const float wx = noise2D(x * warpFreq + 19.19f, y * warpFreq + 73.73f);
            const float wy = noise2D(x * warpFreq + 101.01f, y * warpFreq + 7.07f);
            return octave2D(x + wx * warpAmp, y + wy * warpAmp, oct, pers, lac);
        }

        float domainWarpRidged2D(float x, float y, int oct,
            float warpAmp = 0.75f, float warpFreq = 1.0f,
            float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept
        {
            const float wx = noise2D(x * warpFreq + 19.19f, y * warpFreq + 73.73f);
            const float wy = noise2D(x * warpFreq + 101.01f, y * warpFreq + 7.07f);
            return ridged2D(x + wx * warpAmp, y + wy * warpAmp, oct, lac, gain, offset);
        }

        float domainWarp3D(float x, float y, float z,
            float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept
        {
            const float wx = noise3D(x * warpFreq + 19.19f, y * warpFreq + 73.73f, z * warpFreq + 11.11f);
            const float wy = noise3D(x * warpFreq + 101.01f, y * warpFreq + 7.07f, z * warpFreq + 29.29f);
            const float wz = noise3D(x * warpFreq + 37.37f, y * warpFreq + 53.53f, z * warpFreq + 97.97f);
            return noise3D(x + wx * warpAmp, y + wy * warpAmp, z + wz * warpAmp);
        }

        float domainWarpOctave3D(float x, float y, float z, int oct,
            float pers = 0.5f, float lac = 2.0f,
            float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept
        {
            const float wx = noise3D(x * warpFreq + 19.19f, y * warpFreq + 73.73f, z * warpFreq + 11.11f);
            const float wy = noise3D(x * warpFreq + 101.01f, y * warpFreq + 7.07f, z * warpFreq + 29.29f);
            const float wz = noise3D(x * warpFreq + 37.37f, y * warpFreq + 53.53f, z * warpFreq + 97.97f);
            return octave3D(x + wx * warpAmp, y + wy * warpAmp, z + wz * warpAmp, oct, pers, lac);
        }

        float domainWarpRidged3D(float x, float y, float z, int oct,
            float warpAmp = 0.75f, float warpFreq = 1.0f,
            float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept
        {
            const float wx = noise3D(x * warpFreq + 19.19f, y * warpFreq + 73.73f, z * warpFreq + 11.11f);
            const float wy = noise3D(x * warpFreq + 101.01f, y * warpFreq + 7.07f, z * warpFreq + 29.29f);
            const float wz = noise3D(x * warpFreq + 37.37f, y * warpFreq + 53.53f, z * warpFreq + 97.97f);
            return ridged3D(x + wx * warpAmp, y + wy * warpAmp, z + wz * warpAmp, oct, lac, gain, offset);
        }

    private:
        static float absf(float v) noexcept { return (v < 0.f) ? -v : v; }

        struct G2v { float x, y; };
        struct G3v { float x, y, z; };

        static float dot2(const G2v& g, float x, float y) noexcept { return g.x * x + g.y * y; }
        static float dot3(const G3v& g, float x, float y, float z) noexcept { return g.x * x + g.y * y + g.z * z; }

        void reset() noexcept {
            for (int i = 0; i < 256; ++i) m_perm[i] = default_perm[i];
            for (int i = 0; i < 256; ++i) m_perm512[i] = m_perm512[i + 256] = m_perm[i];
        }

    private:
        uint8_t m_perm[256];
        uint8_t m_perm512[512];

        static constexpr float F2 = 0.3660254037844386f;  // (sqrt(3)-1)/2
        static constexpr float G2 = 0.21132486540518713f; // (3-sqrt(3))/6
        static constexpr float F3 = 0.3333333333333333f;  // 1/3
        static constexpr float G3 = 0.16666666666666666f; // 1/6

        static constexpr G2v grad2[8] = {
            { 1, 1}, {-1, 1}, { 1,-1}, {-1,-1},
            { 1, 0}, {-1, 0}, { 0, 1}, { 0,-1}
        };

        static constexpr G3v grad3[12] = {
            { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
            { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
            { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
        };

        static constexpr uint8_t default_perm[256] = {
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
            140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
            247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
            57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
            74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
            60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
            65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
            200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
            52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
            207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
            119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
            129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
            218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
            81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
            184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
            222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };
    };


    // ==============================================================================================
    // Voronoi / Cellular Queries (deterministic, no heap, fixed small top-k)
    // ==============================================================================================
    //
    // Notes:
    // - One feature point per coarse lattice cell (deterministic jitter via integer hash).
    // - Distances are returned as squared distances for speed and freestanding-friendliness.
    // - `boundary01` is 1 near region boundaries and 0 deep in the region interior.
    // - `nearby()` returns up to top-k nearest region ids + normalized influence weights.
    //
    // Typical usage:
    //   siv::VoronoiNoise2D v(12345u);
    //   auto q = v.nearby(x, z, 3, /*cellSize*/128.0f, /*boundaryWidth*/24.0f);
    //   // q.ids[0] = owner, q.weights[] = blending weights, q.boundary01 = border proximity

    struct VoronoiNearest2DResult {
        uint64_t region_id = 0u;
        int cell_x = 0;
        int cell_y = 0;
        float f1_sq = 0.0f;
    };

    struct VoronoiNearestTwo2DResult {
        uint64_t primary_id = 0u;
        uint64_t secondary_id = 0u;
        int primary_cell_x = 0;
        int primary_cell_y = 0;
        int secondary_cell_x = 0;
        int secondary_cell_y = 0;
        float f1_sq = 0.0f;
        float f2_sq = 0.0f;
        float gap_sq = 0.0f;    // f2_sq - f1_sq
        float boundary01 = 0.0f;
    };

    struct VoronoiNearby2DResult {
        static constexpr int MaxK = 4;
        int count = 0;
        uint64_t ids[MaxK] = { 0u, 0u, 0u, 0u };
        float distances_sq[MaxK] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float weights[MaxK] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float boundary01 = 0.0f;
    };

    struct VoronoiNoise2D {
        VoronoiNoise2D() noexcept : m_seed(0u) {}
        explicit VoronoiNoise2D(uint32_t seed) noexcept : m_seed(seed) {}

        void reseed(uint32_t seed) noexcept { m_seed = seed; }
        uint32_t seed() const noexcept { return m_seed; }

        // Cheapest ownership query:
        // returns nearest region id + F1 (squared distance).
        VoronoiNearest2DResult nearest(
            float x, float y,
            float cellSize = 1.0f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);

            const float inf = 3.402823466e+38f;
            float d1 = inf;
            Feature2D best;
            best.id = 0u; best.cell_x = baseX; best.cell_y = baseY; best.x = 0.0f; best.y = 0.0f;

            for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                    const int cx = baseX + ox;
                    const int cy = baseY + oy;
                    const Feature2D f = featureInCell(cx, cy, cellSize);
                    const float dx = x - f.x;
                    const float dy = y - f.y;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < d1) {
                        d1 = dsq;
                        best = f;
                    }
                }
            }

            VoronoiNearest2DResult out;
            out.region_id = best.id;
            out.cell_x = best.cell_x;
            out.cell_y = best.cell_y;
            out.f1_sq = d1;
            return out;
        }

        // Boundary-aware query:
        // returns nearest + second-nearest and a boundary factor.
        VoronoiNearestTwo2DResult nearestTwo(
            float x, float y,
            float cellSize = 1.0f,
            float boundaryWidth = 0.25f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);

            const float inf = 3.402823466e+38f;
            float d1 = inf;
            float d2 = inf;
            Feature2D a;
            Feature2D b;
            a.id = 0u; a.cell_x = baseX; a.cell_y = baseY; a.x = 0.0f; a.y = 0.0f;
            b = a;

            for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                    const int cx = baseX + ox;
                    const int cy = baseY + oy;
                    const Feature2D f = featureInCell(cx, cy, cellSize);
                    const float dx = x - f.x;
                    const float dy = y - f.y;
                    const float dsq = dx * dx + dy * dy;

                    if (dsq < d1) {
                        d2 = d1; b = a;
                        d1 = dsq; a = f;
                    }
                    else if (dsq < d2) {
                        d2 = dsq; b = f;
                    }
                }
            }

            if (d2 >= inf * 0.5f) {
                d2 = d1;
                b = a;
            }

            const float gapSq = (d2 > d1) ? (d2 - d1) : 0.0f;
            const float widthSq = (boundaryWidth > 0.0f) ? (boundaryWidth * boundaryWidth) : 0.0f;
            const float boundary01 = (widthSq > 0.0f)
                ? (1.0f - ::siv::saturate01(gapSq / widthSq))
                : ((gapSq <= 0.0f) ? 1.0f : 0.0f);

            VoronoiNearestTwo2DResult out;
            out.primary_id = a.id;
            out.secondary_id = b.id;
            out.primary_cell_x = a.cell_x;
            out.primary_cell_y = a.cell_y;
            out.secondary_cell_x = b.cell_x;
            out.secondary_cell_y = b.cell_y;
            out.f1_sq = d1;
            out.f2_sq = d2;
            out.gap_sq = gapSq;
            out.boundary01 = boundary01;
            return out;
        }

        // Full nearby query (top-k small, k in [1..4]):
        // returns ids, squared distances, normalized weights, boundary factor.
        VoronoiNearby2DResult nearby(
            float x, float y,
            int k = 3,
            float cellSize = 1.0f,
            float boundaryWidth = 0.25f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (k < 1) k = 1;
            if (k > VoronoiNearby2DResult::MaxK) k = VoronoiNearby2DResult::MaxK;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            VoronoiNearby2DResult out;
            const float inf = 3.402823466e+38f;
            for (int i = 0; i < VoronoiNearby2DResult::MaxK; ++i) {
                out.ids[i] = 0u;
                out.distances_sq[i] = inf;
                out.weights[i] = 0.0f;
            }

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);

            for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                    const int cx = baseX + ox;
                    const int cy = baseY + oy;
                    const Feature2D f = featureInCell(cx, cy, cellSize);
                    const float dx = x - f.x;
                    const float dy = y - f.y;
                    const float dsq = dx * dx + dy * dy;
                    insertTopK(out.ids, out.distances_sq, k, f.id, dsq);
                }
            }

            int count = 0;
            for (int i = 0; i < k; ++i) {
                if (out.distances_sq[i] < inf * 0.5f) ++count;
            }
            out.count = count;

            if (count >= 2) {
                const float gapSq = (out.distances_sq[1] > out.distances_sq[0])
                    ? (out.distances_sq[1] - out.distances_sq[0])
                    : 0.0f;
                const float widthSq = (boundaryWidth > 0.0f) ? (boundaryWidth * boundaryWidth) : 0.0f;
                out.boundary01 = (widthSq > 0.0f)
                    ? (1.0f - ::siv::saturate01(gapSq / widthSq))
                    : ((gapSq <= 0.0f) ? 1.0f : 0.0f);
            }
            else {
                out.boundary01 = 0.0f;
            }

            if (count > 0) {
                const float eps = 1.0e-6f;
                float sum = 0.0f;
                for (int i = 0; i < count; ++i) {
                    const float w = 1.0f / (out.distances_sq[i] + eps);
                    out.weights[i] = w;
                    sum += w;
                }

                if (sum > 0.0f) {
                    const float inv = 1.0f / sum;
                    for (int i = 0; i < count; ++i) {
                        out.weights[i] *= inv;
                    }
                }
                else {
                    out.weights[0] = 1.0f;
                }
            }

            return out;
        }

        // Convenience F-values (all squared distance space).
        float F1(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            return nearest(x, y, cellSize, neighborhoodRadius).f1_sq;
        }

        float F2(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            return nearestTwo(x, y, cellSize, 1.0f, neighborhoodRadius).f2_sq;
        }

        float F2MinusF1(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            const VoronoiNearestTwo2DResult q = nearestTwo(x, y, cellSize, 1.0f, neighborhoodRadius);
            return (q.f2_sq > q.f1_sq) ? (q.f2_sq - q.f1_sq) : 0.0f;
        }

    private:
        struct Feature2D {
            float x, y;
            int cell_x, cell_y;
            uint64_t id;
        };

        static uint32_t mix32(uint32_t v) noexcept {
            v ^= v >> 16;
            v *= 0x7FEB352Du;
            v ^= v >> 15;
            v *= 0x846CA68Bu;
            v ^= v >> 16;
            return v;
        }

        static uint64_t mix64(uint64_t v) noexcept {
            v ^= v >> 30;
            v *= 0xBF58476D1CE4E5B9ull;
            v ^= v >> 27;
            v *= 0x94D049BB133111EBull;
            v ^= v >> 31;
            return v;
        }

        uint32_t hash2(int x, int y, uint32_t stream) const noexcept {
            uint32_t h = 0x9E3779B9u ^ stream;
            h ^= (uint32_t)x * 0x85EBCA6Bu; h = mix32(h);
            h ^= (uint32_t)y * 0xC2B2AE35u; h = mix32(h);
            h ^= m_seed;                    h = mix32(h);
            return h;
        }

        static float u01(uint32_t h) noexcept {
            return (float)(h & 0x00FFFFFFu) * (1.0f / 16777216.0f);
        }

        uint64_t featureId(int cellX, int cellY, uint32_t localIndex) const noexcept {
            uint64_t v = 0x9E3779B97F4A7C15ull;
            v ^= (uint64_t)(uint32_t)cellX * 0xBF58476D1CE4E5B9ull;
            v ^= (uint64_t)(uint32_t)cellY * 0x94D049BB133111EBull;
            v ^= (uint64_t)m_seed * 0xD6E8FEB86659FD93ull;
            v ^= (uint64_t)localIndex * 0xA0761D6478BD642Full;
            return mix64(v);
        }

        Feature2D featureInCell(int cellX, int cellY, float cellSize) const noexcept {
            const uint32_t hx = hash2(cellX, cellY, 0x68BC21EBu);
            const uint32_t hy = hash2(cellX, cellY, 0x02E5BE93u);

            Feature2D f;
            f.x = ((float)cellX + u01(hx)) * cellSize;
            f.y = ((float)cellY + u01(hy)) * cellSize;
            f.cell_x = cellX;
            f.cell_y = cellY;
            f.id = featureId(cellX, cellY, 0u); // one feature point per coarse cell
            return f;
        }

        static void insertTopK(
            uint64_t ids[VoronoiNearby2DResult::MaxK],
            float distancesSq[VoronoiNearby2DResult::MaxK],
            int k,
            uint64_t id,
            float dsq) noexcept
        {
            for (int i = 0; i < k; ++i) {
                if (ids[i] == id) {
                    if (dsq < distancesSq[i]) distancesSq[i] = dsq;
                    return;
                }
            }

            int pos = -1;
            for (int i = 0; i < k; ++i) {
                if (dsq < distancesSq[i]) {
                    pos = i;
                    break;
                }
            }
            if (pos < 0) return;

            for (int i = k - 1; i > pos; --i) {
                ids[i] = ids[i - 1];
                distancesSq[i] = distancesSq[i - 1];
            }
            ids[pos] = id;
            distancesSq[pos] = dsq;
        }

    private:
        uint32_t m_seed;
    };

    struct VoronoiNearest3DResult {
        uint64_t region_id = 0u;
        int cell_x = 0;
        int cell_y = 0;
        int cell_z = 0;
        float f1_sq = 0.0f;
    };

    struct VoronoiNearestTwo3DResult {
        uint64_t primary_id = 0u;
        uint64_t secondary_id = 0u;
        int primary_cell_x = 0;
        int primary_cell_y = 0;
        int primary_cell_z = 0;
        int secondary_cell_x = 0;
        int secondary_cell_y = 0;
        int secondary_cell_z = 0;
        float f1_sq = 0.0f;
        float f2_sq = 0.0f;
        float gap_sq = 0.0f;    // f2_sq - f1_sq
        float boundary01 = 0.0f;
    };

    struct VoronoiNearby3DResult {
        static constexpr int MaxK = 4;
        int count = 0;
        uint64_t ids[MaxK] = { 0u, 0u, 0u, 0u };
        float distances_sq[MaxK] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float weights[MaxK] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float boundary01 = 0.0f;
    };

    struct VoronoiNoise3D {
        VoronoiNoise3D() noexcept : m_seed(0u) {}
        explicit VoronoiNoise3D(uint32_t seed) noexcept : m_seed(seed) {}

        void reseed(uint32_t seed) noexcept { m_seed = seed; }
        uint32_t seed() const noexcept { return m_seed; }

        VoronoiNearest3DResult nearest(
            float x, float y, float z,
            float cellSize = 1.0f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);
            const int baseZ = ::siv::fastfloor(z / cellSize);

            const float inf = 3.402823466e+38f;
            float d1 = inf;
            Feature3D best;
            best.id = 0u; best.cell_x = baseX; best.cell_y = baseY; best.cell_z = baseZ;
            best.x = 0.0f; best.y = 0.0f; best.z = 0.0f;

            for (int oz = -neighborhoodRadius; oz <= neighborhoodRadius; ++oz) {
                for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                    for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                        const int cx = baseX + ox;
                        const int cy = baseY + oy;
                        const int cz = baseZ + oz;
                        const Feature3D f = featureInCell(cx, cy, cz, cellSize);
                        const float dx = x - f.x;
                        const float dy = y - f.y;
                        const float dz = z - f.z;
                        const float dsq = dx * dx + dy * dy + dz * dz;
                        if (dsq < d1) {
                            d1 = dsq;
                            best = f;
                        }
                    }
                }
            }

            VoronoiNearest3DResult out;
            out.region_id = best.id;
            out.cell_x = best.cell_x;
            out.cell_y = best.cell_y;
            out.cell_z = best.cell_z;
            out.f1_sq = d1;
            return out;
        }

        VoronoiNearestTwo3DResult nearestTwo(
            float x, float y, float z,
            float cellSize = 1.0f,
            float boundaryWidth = 0.25f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);
            const int baseZ = ::siv::fastfloor(z / cellSize);

            const float inf = 3.402823466e+38f;
            float d1 = inf;
            float d2 = inf;
            Feature3D a;
            Feature3D b;
            a.id = 0u; a.cell_x = baseX; a.cell_y = baseY; a.cell_z = baseZ;
            a.x = 0.0f; a.y = 0.0f; a.z = 0.0f;
            b = a;

            for (int oz = -neighborhoodRadius; oz <= neighborhoodRadius; ++oz) {
                for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                    for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                        const int cx = baseX + ox;
                        const int cy = baseY + oy;
                        const int cz = baseZ + oz;
                        const Feature3D f = featureInCell(cx, cy, cz, cellSize);
                        const float dx = x - f.x;
                        const float dy = y - f.y;
                        const float dz = z - f.z;
                        const float dsq = dx * dx + dy * dy + dz * dz;

                        if (dsq < d1) {
                            d2 = d1; b = a;
                            d1 = dsq; a = f;
                        }
                        else if (dsq < d2) {
                            d2 = dsq; b = f;
                        }
                    }
                }
            }

            if (d2 >= inf * 0.5f) {
                d2 = d1;
                b = a;
            }

            const float gapSq = (d2 > d1) ? (d2 - d1) : 0.0f;
            const float widthSq = (boundaryWidth > 0.0f) ? (boundaryWidth * boundaryWidth) : 0.0f;
            const float boundary01 = (widthSq > 0.0f)
                ? (1.0f - ::siv::saturate01(gapSq / widthSq))
                : ((gapSq <= 0.0f) ? 1.0f : 0.0f);

            VoronoiNearestTwo3DResult out;
            out.primary_id = a.id;
            out.secondary_id = b.id;
            out.primary_cell_x = a.cell_x;
            out.primary_cell_y = a.cell_y;
            out.primary_cell_z = a.cell_z;
            out.secondary_cell_x = b.cell_x;
            out.secondary_cell_y = b.cell_y;
            out.secondary_cell_z = b.cell_z;
            out.f1_sq = d1;
            out.f2_sq = d2;
            out.gap_sq = gapSq;
            out.boundary01 = boundary01;
            return out;
        }

        VoronoiNearby3DResult nearby(
            float x, float y, float z,
            int k = 3,
            float cellSize = 1.0f,
            float boundaryWidth = 0.25f,
            int neighborhoodRadius = 1) const noexcept
        {
            if (cellSize <= 0.0f) cellSize = 1.0f;
            if (k < 1) k = 1;
            if (k > VoronoiNearby3DResult::MaxK) k = VoronoiNearby3DResult::MaxK;
            if (neighborhoodRadius < 0) neighborhoodRadius = 0;

            VoronoiNearby3DResult out;
            const float inf = 3.402823466e+38f;
            for (int i = 0; i < VoronoiNearby3DResult::MaxK; ++i) {
                out.ids[i] = 0u;
                out.distances_sq[i] = inf;
                out.weights[i] = 0.0f;
            }

            const int baseX = ::siv::fastfloor(x / cellSize);
            const int baseY = ::siv::fastfloor(y / cellSize);
            const int baseZ = ::siv::fastfloor(z / cellSize);

            for (int oz = -neighborhoodRadius; oz <= neighborhoodRadius; ++oz) {
                for (int oy = -neighborhoodRadius; oy <= neighborhoodRadius; ++oy) {
                    for (int ox = -neighborhoodRadius; ox <= neighborhoodRadius; ++ox) {
                        const int cx = baseX + ox;
                        const int cy = baseY + oy;
                        const int cz = baseZ + oz;
                        const Feature3D f = featureInCell(cx, cy, cz, cellSize);
                        const float dx = x - f.x;
                        const float dy = y - f.y;
                        const float dz = z - f.z;
                        const float dsq = dx * dx + dy * dy + dz * dz;
                        insertTopK(out.ids, out.distances_sq, k, f.id, dsq);
                    }
                }
            }

            int count = 0;
            for (int i = 0; i < k; ++i) {
                if (out.distances_sq[i] < inf * 0.5f) ++count;
            }
            out.count = count;

            if (count >= 2) {
                const float gapSq = (out.distances_sq[1] > out.distances_sq[0])
                    ? (out.distances_sq[1] - out.distances_sq[0])
                    : 0.0f;
                const float widthSq = (boundaryWidth > 0.0f) ? (boundaryWidth * boundaryWidth) : 0.0f;
                out.boundary01 = (widthSq > 0.0f)
                    ? (1.0f - ::siv::saturate01(gapSq / widthSq))
                    : ((gapSq <= 0.0f) ? 1.0f : 0.0f);
            }
            else {
                out.boundary01 = 0.0f;
            }

            if (count > 0) {
                const float eps = 1.0e-6f;
                float sum = 0.0f;
                for (int i = 0; i < count; ++i) {
                    const float w = 1.0f / (out.distances_sq[i] + eps);
                    out.weights[i] = w;
                    sum += w;
                }

                if (sum > 0.0f) {
                    const float inv = 1.0f / sum;
                    for (int i = 0; i < count; ++i) {
                        out.weights[i] *= inv;
                    }
                }
                else {
                    out.weights[0] = 1.0f;
                }
            }

            return out;
        }

        float F1(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            return nearest(x, y, z, cellSize, neighborhoodRadius).f1_sq;
        }

        float F2(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            return nearestTwo(x, y, z, cellSize, 1.0f, neighborhoodRadius).f2_sq;
        }

        float F2MinusF1(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept {
            const VoronoiNearestTwo3DResult q = nearestTwo(x, y, z, cellSize, 1.0f, neighborhoodRadius);
            return (q.f2_sq > q.f1_sq) ? (q.f2_sq - q.f1_sq) : 0.0f;
        }

    private:
        struct Feature3D {
            float x, y, z;
            int cell_x, cell_y, cell_z;
            uint64_t id;
        };

        static uint32_t mix32(uint32_t v) noexcept {
            v ^= v >> 16;
            v *= 0x7FEB352Du;
            v ^= v >> 15;
            v *= 0x846CA68Bu;
            v ^= v >> 16;
            return v;
        }

        static uint64_t mix64(uint64_t v) noexcept {
            v ^= v >> 30;
            v *= 0xBF58476D1CE4E5B9ull;
            v ^= v >> 27;
            v *= 0x94D049BB133111EBull;
            v ^= v >> 31;
            return v;
        }

        uint32_t hash3(int x, int y, int z, uint32_t stream) const noexcept {
            uint32_t h = 0x9E3779B9u ^ stream;
            h ^= (uint32_t)x * 0x85EBCA6Bu; h = mix32(h);
            h ^= (uint32_t)y * 0xC2B2AE35u; h = mix32(h);
            h ^= (uint32_t)z * 0x27D4EB2Fu; h = mix32(h);
            h ^= m_seed;                    h = mix32(h);
            return h;
        }

        static float u01(uint32_t h) noexcept {
            return (float)(h & 0x00FFFFFFu) * (1.0f / 16777216.0f);
        }

        uint64_t featureId(int cellX, int cellY, int cellZ, uint32_t localIndex) const noexcept {
            uint64_t v = 0x9E3779B97F4A7C15ull;
            v ^= (uint64_t)(uint32_t)cellX * 0xBF58476D1CE4E5B9ull;
            v ^= (uint64_t)(uint32_t)cellY * 0x94D049BB133111EBull;
            v ^= (uint64_t)(uint32_t)cellZ * 0xD6E8FEB86659FD93ull;
            v ^= (uint64_t)m_seed * 0xA0761D6478BD642Full;
            v ^= (uint64_t)localIndex * 0xE7037ED1A0B428DBull;
            return mix64(v);
        }

        Feature3D featureInCell(int cellX, int cellY, int cellZ, float cellSize) const noexcept {
            const uint32_t hx = hash3(cellX, cellY, cellZ, 0x68BC21EBu);
            const uint32_t hy = hash3(cellX, cellY, cellZ, 0x02E5BE93u);
            const uint32_t hz = hash3(cellX, cellY, cellZ, 0x967A889Bu);

            Feature3D f;
            f.x = ((float)cellX + u01(hx)) * cellSize;
            f.y = ((float)cellY + u01(hy)) * cellSize;
            f.z = ((float)cellZ + u01(hz)) * cellSize;
            f.cell_x = cellX;
            f.cell_y = cellY;
            f.cell_z = cellZ;
            f.id = featureId(cellX, cellY, cellZ, 0u); // one feature point per coarse cell
            return f;
        }

        static void insertTopK(
            uint64_t ids[VoronoiNearby3DResult::MaxK],
            float distancesSq[VoronoiNearby3DResult::MaxK],
            int k,
            uint64_t id,
            float dsq) noexcept
        {
            for (int i = 0; i < k; ++i) {
                if (ids[i] == id) {
                    if (dsq < distancesSq[i]) distancesSq[i] = dsq;
                    return;
                }
            }

            int pos = -1;
            for (int i = 0; i < k; ++i) {
                if (dsq < distancesSq[i]) {
                    pos = i;
                    break;
                }
            }
            if (pos < 0) return;

            for (int i = k - 1; i > pos; --i) {
                ids[i] = ids[i - 1];
                distancesSq[i] = distancesSq[i - 1];
            }
            ids[pos] = id;
            distancesSq[pos] = dsq;
        }

    private:
        uint32_t m_seed;
    };

    struct BlueNoise2D {
        uint32_t seed;            // world seed
        uint32_t additional_seed; // chunk seed (for Minecraft you can change (chunkX,chunkZ)-derived)

        explicit BlueNoise2D(uint32_t world_seed = 0u, uint32_t chunk_seed = 0u) noexcept
            : seed(world_seed), additional_seed(chunk_seed) {}

        // ---------------- core hash(x,z) ----------------
        uint32_t hash(int x, int z) const noexcept {
            // 2D -> 32-bit avalanche, freestanding
            uint32_t h = 0x9E3779B9u;
            h ^= (uint32_t)x * 0x85EBCA6Bu; h = mix32(h);
            h ^= (uint32_t)z * 0xC2B2AE35u; h = mix32(h);
            h ^= seed;                      h = mix32(h);
            h ^= additional_seed;           h = mix32(h);
            return h;
        }

        // [0,1)
        float rand01(int x, int z) const noexcept {
            const uint32_t h = hash(x, z);
            return (float)(h & 0x00FFFFFFu) * (1.0f / 16777216.0f);
        }

        // ---------------- "Blue-noise" points (Poisson-disk-ish) ----------------
        // Idea:
        // 1) Divide the space into a grid with a step size of cell = r / sqrt(2)
        // 2) in each cell - 1 deterministic jitter point
        // 3) The point is "accepted" if there are no neighbours at a distance < r (5x5 cell check).

        struct Point { float x, z; }; // world-space

        // Deterministically generates 1 dot inside grid-cell (cellX, cellZ)
        Point pointInCell(int cellX, int cellZ, float cellSize) const noexcept {
            // two independent components due to different impurities
            const uint32_t h1 = hash(cellX, cellZ);
            const uint32_t h2 = mix32(h1 ^ 0xB5297A4Du);

            const float jx = (float)(h1 & 0x00FFFFFFu) * (1.0f / 16777216.0f); // [0,1)
            const float jz = (float)(h2 & 0x00FFFFFFu) * (1.0f / 16777216.0f);

            Point p;
            p.x = ((float)cellX + jx) * cellSize;
            p.z = ((float)cellZ + jz) * cellSize;
            return p;
        }

        uint32_t cellKey(int cellX, int cellZ) const noexcept {
            return mix32(hash(cellX, cellZ) ^ 0xD1B54A35u);
        }

        // Checks whether the point in cell(cellX,cellZ) passes through the Poisson disk with minimum distance r
        bool acceptCell(int cellX, int cellZ, float r) const noexcept {
            if (r <= 0.f) return false;

            const float cellSize = r * INV_SQRT2; // r / sqrt(2)
            const float r2 = r * r;

            const Point p = pointInCell(cellX, cellZ, cellSize);
            const uint32_t myKey = cellKey(cellX, cellZ);

            // 5x5 neighbouring cells (radius 2) are sufficient, because cellSize ~ r/1.414
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dz == 0) continue;

                    const int nx = cellX + dx;
                    const int nz = cellZ + dz;

                    const Point q = pointInCell(nx, nz, cellSize);
                    const float vx = p.x - q.x;
                    const float vz = p.z - q.z;

                    if (vx * vx + vz * vz < r2) {
                        const uint32_t nKey = cellKey(nx, nz);
                        if (nKey < myKey) return false; // neighbour wins conflict
                    }
                }
            }
            return true;
        }

        // Minecraft-like API (for convenience):
        // sorting through "tree candidates" in a chunk:
        // MinX/Z - world coordinates of the chunk start (in blocks), sizeX/Z - e.g. 16
        // r - minimum distance between trees (in blocks)
        // callback(cellPointX, cellPointZ) is called for each accepted point that falls within the chunk.
        template <class F>
        void forEachPointInRect(int minX, int minZ, int sizeX, int sizeZ, float r, F&& cb) const noexcept {
            if (sizeX <= 0 || sizeZ <= 0 || r <= 0.f) return;
            const float cellSize = r * INV_SQRT2;

            const int cellMinX = ::siv::fastfloor((float)minX / cellSize);
            const int cellMinZ = ::siv::fastfloor((float)minZ / cellSize);
            const int cellMaxX = ::siv::fastfloor((float)(minX + sizeX - 1) / cellSize);
            const int cellMaxZ = ::siv::fastfloor((float)(minZ + sizeZ - 1) / cellSize);

            const float maxX = (float)(minX + sizeX);
            const float maxZ = (float)(minZ + sizeZ);

            for (int cz = cellMinZ; cz <= cellMaxZ; ++cz) {
                for (int cx = cellMinX; cx <= cellMaxX; ++cx) {
                    if (!acceptCell(cx, cz, r)) continue;
                    const Point p = pointInCell(cx, cz, cellSize);
                    if (p.x >= (float)minX && p.x < maxX && p.z >= (float)minZ && p.z < maxZ) {
                        cb(p.x, p.z);
                    }
                }
            }
        }

    private:
        static constexpr float INV_SQRT2 = 0.7071067811865475f;

        static uint32_t mix32(uint32_t v) noexcept {
            v ^= v >> 16;
            v *= 0x7FEB352Du;
            v ^= v >> 15;
            v *= 0x846CA68Bu;
            v ^= v >> 16;
            return v;
        }
    }; // struct BlueNoise2D
} // namespace siv

