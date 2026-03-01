# siv::SimplexNoise

![Noise Gallery](noise-gallery.png)

Header-only **Simplex noise** toolkit for **C++14** (freestanding implementation, no `<cmath>` required in the library itself).  
Includes the pieces you typically need for Minecraft-like procedural generation: **FBM**, **ridged multifractal**, **domain warping**, and **blue-noise-like** placement for trees/objects.

## What this fork is

This repository started as a fork of **siv::PerlinNoise** and was rewritten into a **Simplex-based** library with a different scope and API.

### Main changes compared to the original PerlinNoise repo

- **Algorithm**: Perlin → **Simplex** (2D uses triangles, 3D uses tetrahedra)
- **Dimensionality**: removed **1D** noise (focus on Minecraft-like usage)
- **Freestanding C++14**: the library code avoids `<cmath>` and STL dependencies
- **Added features** (beyond classic Perlin):
  - **FBM** octaves (raw + normalized)
  - **Ridged multifractal** (mountain-style ridges)
  - **Domain warping** helpers (macro variation)
  - **BlueNoise2D** for deterministic, chunk-safe **object placement** (trees/rocks/etc.)
- **API**: simplified, no templates; primary type is `siv::SimplexNoise` (float-based)

## Features

- 2D / 3D **Simplex noise** (`noise2D`, `noise3D`) — ~`[-1, 1]`
- **Octave FBM** (raw + normalized)
- **Ridged multifractal** (2D/3D)
- **Domain warping** (base / FBM / ridged, 2D/3D)
- Deterministic **serialization** (`serialize`, `deserialize`)
- **BlueNoise2D** (tile-free, deterministic Poisson-disk-like placement)

## License

Distributed under the **MIT license** (same as the original upstream).

## Build (example)

This repo contains a single demo program: `example.cpp` in the root.

### CMake (C++14 only)

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run:

```bash
./build/example
```

### Makefile (clang, C++14)

```bash
make
./example
```

> The **library** is freestanding; the **example** is not (it uses iostream/vector/files to generate BMPs).

## Usage

```cpp
#include "SimplexNoise.hpp"

int main() {
    siv::SimplexNoise n(12345u);

    // base simplex: ~[-1, 1]
    float v0 = n.noise2D(10.0f * 0.01f, 20.0f * 0.01f);

    // FBM (normalized): ~[-1, 1]
    float v1 = n.normalizedOctave2D(10.0f * 0.005f, 20.0f * 0.005f, 6, 0.5f, 2.0f);

    // ridged (mountains): roughly [0, 1]
    float v2 = n.ridged2D(10.0f * 0.002f, 20.0f * 0.002f, 6);

    // domain-warped base: ~[-1, 1]
    float v3 = n.domainWarp2D(10.0f * 0.01f, 20.0f * 0.01f, 1.0f, 0.8f);

    (void)v0; (void)v1; (void)v2; (void)v3;
}
```

## API

### `struct siv::SimplexNoise`

**Constructors**
- `SimplexNoise() noexcept`
- `explicit SimplexNoise(std::uint32_t seed) noexcept`

**Seeding**
- `void reseed(std::uint32_t seed) noexcept`

**Serialization**
- `const std::uint8_t* serialize() const noexcept`  
  Returns a pointer to internal permutation state (256 bytes).
- `void deserialize(const std::uint8_t state[256]) noexcept`

**Base noise** (approximately in `[-1, 1]`)
- `float noise2D(float x, float y) const noexcept`
- `float noise3D(float x, float y, float z) const noexcept`

**Utilities**
- `static float remap01(float v) noexcept`  
  Remaps `[-1,1] -> [0,1]` (no clamp).

**FBM / Octaves**  
`pers` = persistence, `lac` = lacunarity.
- Raw (can exceed `[-1,1]`)
  - `float octave2D(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
  - `float octave3D(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
- Normalized (more stable range, typically near `[-1,1]`)
  - `float normalizedOctave2D(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
  - `float normalizedOctave3D(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
- Convenience remap to `[0,1]` (not clamped)
  - `float octave2D_01(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
  - `float octave3D_01(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
  - `float normalizedOctave2D_01(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`
  - `float normalizedOctave3D_01(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f) const noexcept`

**Ridged multifractal** (mountain-like ridges; roughly `[0,1]`)
- `float ridged2D(float x, float y, int oct, float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept`
- `float ridged3D(float x, float y, float z, int oct, float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept`

**Domain warping**
- Base warp
  - `float domainWarp2D(float x, float y, float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept`
  - `float domainWarp3D(float x, float y, float z, float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept`
- Warp + FBM (raw FBM sum under warp)
  - `float domainWarpOctave2D(float x, float y, int oct, float pers = 0.5f, float lac = 2.0f, float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept`
  - `float domainWarpOctave3D(float x, float y, float z, int oct, float pers = 0.5f, float lac = 2.0f, float warpAmp = 0.75f, float warpFreq = 1.0f) const noexcept`
- Warp + ridged
  - `float domainWarpRidged2D(float x, float y, int oct, float warpAmp = 0.75f, float warpFreq = 1.0f, float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept`
  - `float domainWarpRidged3D(float x, float y, float z, int oct, float warpAmp = 0.75f, float warpFreq = 1.0f, float lac = 2.0f, float gain = 2.0f, float offset = 1.0f) const noexcept`

### `struct siv::BlueNoise2D`

Deterministic, chunk-safe, tile-free blue-noise-like distribution for **object placement**, not heightmaps.

**Fields**
- `std::uint32_t seed` — world seed
- `std::uint32_t additional_seed` — chunk seed (or any derived local seed)

**Constructors**
- `explicit BlueNoise2D(std::uint32_t world_seed = 0u, std::uint32_t chunk_seed = 0u) noexcept`

**Hash / RNG**
- `std::uint32_t hash(int x, int z) const noexcept`
- `float rand01(int x, int z) const noexcept`  (`[0,1)`)

**Placement**
- `template<class F> void forEachPointInRect(int minX, int minZ, int sizeX, int sizeZ, float r, F&& cb) const noexcept`

Where:
- `(minX, minZ)` is the region start in world “block” coordinates
- `(sizeX, sizeZ)` is the region size (e.g. `16x16` for a chunk)
- `r` is the minimum spacing in blocks (e.g. `4..9` for trees)
- callback receives `(px, pz)` candidate positions in world space

## Notes on ranges (important)

- `noise2D/noise3D` are designed to be near `[-1, 1]`
- `octave*` / `domainWarpOctave*` are **raw sums** and can exceed `[-1,1]`
- for stable “thresholding” workflows (caves, biome masks), prefer:
  - `normalizedOctave*` or clamp/remap in your own pipeline

## What `example.cpp` does

`example.cpp` is a standalone demo that generates a BMP “gallery” so users can **visually understand** each noise mode.

It outputs:
- base simplex (`noise2D`)
- FBM: raw + normalized (`octave2D`, `normalizedOctave2D`)
- ridged multifractal (`ridged2D`)
- domain-warped base / FBM / ridged
- BlueNoise2D placement maps (think “tree positions”)

Key points:
- generates multiple images with different **seeds**
- generates multiple frequencies (e.g. `2 / 8 / 32`) to show scale differences
- BlueNoise images visualize points with spacing `r` (denser for smaller `r`)

If you want to tweak output, edit these parts inside `main()`:
- `frequencies[]`
- `octaves`, `persistence`, `lacunarity`
- `warpAmp`, `warpFreq`
- BlueNoise `minDistances[]` and `dotRadius`
