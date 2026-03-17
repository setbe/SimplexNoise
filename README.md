# siv::SimplexNoise

![Noise Gallery](noise-gallery.png)

Header-only **Simplex noise** toolkit for **C++14** (freestanding implementation, no `<cmath>` required in the library itself).  
Includes the pieces you typically need for Minecraft-like procedural generation: **FBM**, **ridged multifractal**, **domain warping**, **Voronoi/cellular region queries**, and **blue-noise-like** placement for trees/objects.

## What this fork is

This repository started as a fork of **siv::PerlinNoise** and was rewritten into a **Simplex-based** library with a different scope and API.

### Main changes compared to the original PerlinNoise repo

- **Algorithm**: Perlin -> **Simplex** (2D uses triangles, 3D uses tetrahedra)
- **Dimensionality**: removed **1D** noise (focus on Minecraft-like usage)
- **Freestanding C++14**: the library code avoids `<cmath>` and STL dependencies
- **Added features** (beyond classic Perlin):
  - **FBM** octaves (raw + normalized)
  - **Ridged multifractal** (mountain-style ridges)
  - **Domain warping** helpers (macro variation)
  - **VoronoiNoise2D / VoronoiNoise3D** (nearest owner, F1/F2, boundary, top-k blending)
  - **BlueNoise2D** for deterministic, chunk-safe **object placement** (trees/rocks/etc.)
- **API**: simplified, no templates; primary type is `siv::SimplexNoise` (float-based)

## Features

- 2D / 3D **Simplex noise** (`noise2D`, `noise3D`) - ~`[-1, 1]`
- **Octave FBM** (raw + normalized)
- **Ridged multifractal** (2D/3D)
- **Domain warping** (base / FBM / ridged, 2D/3D)
- **Voronoi/cellular region queries** (2D/3D): nearest owner, nearest-two boundary, top-k weights
- Deterministic **serialization** (`serialize`, `deserialize`)
- **BlueNoise2D** (tile-free, deterministic Poisson-disk-like placement)

## Voronoi: what, how, why

### What it gives you

`VoronoiNoise2D/3D` is not just another scalar noise signal.  
It is a deterministic **spatial query system** that gives you:

- a discrete owner region (`nearest`)
- boundary awareness (`nearestTwo`, using `F2 - F1` in squared-distance space)
- smooth multi-region blending inputs (`nearby` top-k ids + normalized weights)

This is useful when you need both:

- hard logic (one stable region id)
- smooth transitions (blended influence near borders)

### How it works

The world is split into coarse lattice cells.  
Each cell has one deterministic feature point generated from:

- integer cell coordinates
- world seed

For query position `p`, the library checks a small local neighborhood (`neighborhoodRadius`, default `1`) and computes nearest candidates.

Returned distances are **squared distances** for speed (no `sqrt` in hot path).

### Why this is better than regular grid ownership

With regular grid ownership, borders are axis-aligned and visually predictable.  
With Voronoi ownership:

- regions have irregular natural shapes
- borders are meaningful simulation zones
- `boundary01` directly tells how close you are to a border

That lets you keep deterministic region identity while still driving smooth field blending near boundaries.

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
    siv::VoronoiNoise2D v(12345u);

    // base simplex: ~[-1, 1]
    float v0 = n.noise2D(10.0f * 0.01f, 20.0f * 0.01f);

    // FBM (normalized): ~[-1, 1]
    float v1 = n.normalizedOctave2D(10.0f * 0.005f, 20.0f * 0.005f, 6, 0.5f, 2.0f);

    // ridged (mountains): roughly [0, 1]
    float v2 = n.ridged2D(10.0f * 0.002f, 20.0f * 0.002f, 6);

    // domain-warped base: ~[-1, 1]
    float v3 = n.domainWarp2D(10.0f * 0.01f, 20.0f * 0.01f, 1.0f, 0.8f);

    // Voronoi region query (distance units are squared)
    auto q = v.nearestTwo(10.0f, 20.0f, /*cellSize*/128.0f, /*boundaryWidth*/24.0f);

    (void)v0; (void)v1; (void)v2; (void)v3; (void)q;
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

**Call examples**

```cpp
siv::SimplexNoise n0;
siv::SimplexNoise n1(12345u);
n1.reseed(777u);

float a = n1.noise2D(x, z);
float b = n1.noise3D(x, y, z);

float fbmRaw2D = n1.octave2D(x, z, 6, 0.5f, 2.0f);
float fbmRaw3D = n1.octave3D(x, y, z, 5, 0.5f, 2.0f);

float fbmNorm2D = n1.normalizedOctave2D(x, z, 6, 0.5f, 2.0f);
float fbmNorm3D = n1.normalizedOctave3D(x, y, z, 5, 0.5f, 2.0f);

float rid2 = n1.ridged2D(x, z, 6);
float rid3 = n1.ridged3D(x, y, z, 6);

float warpBase2 = n1.domainWarp2D(x, z, 1.0f, 0.8f);
float warpBase3 = n1.domainWarp3D(x, y, z, 1.0f, 0.8f);

float warpFbm2 = n1.domainWarpOctave2D(x, z, 6, 0.5f, 2.0f, 1.0f, 0.8f);
float warpFbm3 = n1.domainWarpOctave3D(x, y, z, 5, 0.5f, 2.0f, 1.0f, 0.8f);

float warpRid2 = n1.domainWarpRidged2D(x, z, 6, 1.0f, 0.8f, 2.0f, 2.0f, 1.0f);
float warpRid3 = n1.domainWarpRidged3D(x, y, z, 6, 1.0f, 0.8f, 2.0f, 2.0f, 1.0f);

float v01 = siv::SimplexNoise::remap01(a);

const std::uint8_t* state = n1.serialize();
siv::SimplexNoise n2;
n2.deserialize(state);
```

### `struct siv::VoronoiNoise2D`

Deterministic 2D Voronoi/cellular nearest-feature queries with one feature point per coarse cell.

**Constructors**
- `VoronoiNoise2D() noexcept`
- `explicit VoronoiNoise2D(std::uint32_t seed) noexcept`

**Seeding**
- `void reseed(std::uint32_t seed) noexcept`
- `std::uint32_t seed() const noexcept`

**Queries** (`cellSize` controls macro region scale; distances are squared)
- `VoronoiNearest2DResult nearest(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `VoronoiNearestTwo2DResult nearestTwo(float x, float y, float cellSize = 1.0f, float boundaryWidth = 0.25f, int neighborhoodRadius = 1) const noexcept`
- `VoronoiNearby2DResult nearby(float x, float y, int k = 3, float cellSize = 1.0f, float boundaryWidth = 0.25f, int neighborhoodRadius = 1) const noexcept`

**F-value helpers** (squared distance domain)
- `float F1(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `float F2(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `float F2MinusF1(float x, float y, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`

**Result structs**
- `VoronoiNearest2DResult`: nearest `region_id`, owner coarse-cell coordinates, `f1_sq`
- `VoronoiNearestTwo2DResult`: primary/secondary ids, `f1_sq`, `f2_sq`, `gap_sq`, `boundary01`
- `VoronoiNearby2DResult`: top-k `ids`, `distances_sq`, normalized `weights`, `boundary01`

**Call examples**

```cpp
siv::VoronoiNoise2D v2a;
siv::VoronoiNoise2D v2b(12345u);
v2b.reseed(999u);

auto owner2 = v2b.nearest(px, pz, /*cellSize*/128.0f, /*radius*/1);
auto border2 = v2b.nearestTwo(px, pz, /*cellSize*/128.0f, /*boundaryWidth*/24.0f, /*radius*/1);
auto nearby2 = v2b.nearby(px, pz, /*k*/3, /*cellSize*/128.0f, /*boundaryWidth*/24.0f, /*radius*/1);

float f1_2 = v2b.F1(px, pz, 128.0f, 1);
float f2_2 = v2b.F2(px, pz, 128.0f, 1);
float gap2 = v2b.F2MinusF1(px, pz, 128.0f, 1);

// Typical high-level usage:
// owner2.region_id           -> hard region ownership
// border2.boundary01         -> proximity to border [0..1]
// nearby2.ids/weights/count  -> smooth influence blend input
```

### `struct siv::VoronoiNoise3D`

Deterministic 3D Voronoi/cellular nearest-feature queries.

**Constructors**
- `VoronoiNoise3D() noexcept`
- `explicit VoronoiNoise3D(std::uint32_t seed) noexcept`

**Seeding**
- `void reseed(std::uint32_t seed) noexcept`
- `std::uint32_t seed() const noexcept`

**Queries** (`cellSize` controls region scale; distances are squared)
- `VoronoiNearest3DResult nearest(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `VoronoiNearestTwo3DResult nearestTwo(float x, float y, float z, float cellSize = 1.0f, float boundaryWidth = 0.25f, int neighborhoodRadius = 1) const noexcept`
- `VoronoiNearby3DResult nearby(float x, float y, float z, int k = 3, float cellSize = 1.0f, float boundaryWidth = 0.25f, int neighborhoodRadius = 1) const noexcept`

**F-value helpers** (squared distance domain)
- `float F1(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `float F2(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`
- `float F2MinusF1(float x, float y, float z, float cellSize = 1.0f, int neighborhoodRadius = 1) const noexcept`

**Call examples**

```cpp
siv::VoronoiNoise3D v3a;
siv::VoronoiNoise3D v3b(12345u);
v3b.reseed(777u);

auto owner3 = v3b.nearest(px, py, pz, /*cellSize*/96.0f, /*radius*/1);
auto border3 = v3b.nearestTwo(px, py, pz, /*cellSize*/96.0f, /*boundaryWidth*/18.0f, /*radius*/1);
auto nearby3 = v3b.nearby(px, py, pz, /*k*/4, /*cellSize*/96.0f, /*boundaryWidth*/18.0f, /*radius*/1);

float f1_3 = v3b.F1(px, py, pz, 96.0f, 1);
float f2_3 = v3b.F2(px, py, pz, 96.0f, 1);
float gap3 = v3b.F2MinusF1(px, py, pz, 96.0f, 1);
```

### `struct siv::BlueNoise2D`

Deterministic, chunk-safe, tile-free blue-noise-like distribution for **object placement**, not heightmaps.

**Fields**
- `std::uint32_t seed` - world seed
- `std::uint32_t additional_seed` - chunk seed (or any derived local seed)

**Constructors**
- `explicit BlueNoise2D(std::uint32_t world_seed = 0u, std::uint32_t chunk_seed = 0u) noexcept`

**Hash / RNG**
- `std::uint32_t hash(int x, int z) const noexcept`
- `float rand01(int x, int z) const noexcept`  (`[0,1)`)

**Placement**
- `template<class F> void forEachPointInRect(int minX, int minZ, int sizeX, int sizeZ, float r, F&& cb) const noexcept`

Where:
- `(minX, minZ)` is the region start in world "block" coordinates
- `(sizeX, sizeZ)` is the region size (e.g. `16x16` for a chunk)
- `r` is the minimum spacing in blocks (e.g. `4..9` for trees)
- callback receives `(px, pz)` candidate positions in world space

**Call examples**

```cpp
siv::BlueNoise2D bn(/*worldSeed*/1337u, /*chunkSeed*/42u);

std::uint32_t h = bn.hash(10, 20);
float u = bn.rand01(10, 20);

bn.forEachPointInRect(/*minX*/0, /*minZ*/0, /*sizeX*/16, /*sizeZ*/16, /*r*/6.0f,
    [&](float px, float pz) {
        // candidate placement at (px, pz)
    });
```

## Notes on ranges (important)

- `noise2D/noise3D` are designed to be near `[-1, 1]`
- `octave*` / `domainWarpOctave*` are **raw sums** and can exceed `[-1,1]`
- Voronoi `F1/F2/...` values are returned as **squared distances**
- for stable "thresholding" workflows (caves, biome masks), prefer:
  - `normalizedOctave*` or clamp/remap in your own pipeline

## What `example.cpp` does

`example.cpp` is a standalone demo that generates a BMP "gallery" so users can **visually understand** each noise mode.

It outputs:
- base simplex (`noise2D`)
- FBM: raw + normalized (`octave2D`, `normalizedOctave2D`)
- ridged multifractal (`ridged2D`)
- domain-warped base / FBM / ridged
- Voronoi region-id map (`VoronoiNoise2D::nearest`)
- Voronoi boundary map (`VoronoiNoise2D::nearestTwo`)
- BlueNoise2D placement maps (think "tree positions")

Key points:
- generates multiple images with different **seeds**
- generates multiple frequencies (e.g. `2 / 8 / 32`) to show scale differences
- BlueNoise images visualize points with spacing `r` (denser for smaller `r`)

If you want to tweak output, edit these parts inside `main()`:
- `frequencies[]`
- `octaves`, `persistence`, `lacunarity`
- `warpAmp`, `warpFreq`
- BlueNoise `minDistances[]` and `dotRadius`
