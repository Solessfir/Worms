#include "Biome.h"
#include "raylib/raymath.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr std::array<BiomeTheme, static_cast<size_t>(BiomeKind::Count)> Themes =
    {{
        {
            .Name = "Meadow",
            .SkyTop = {.r = 64, .g = 124, .b = 204, .a = 255},
            .SkyBottom = {.r = 186, .g = 220, .b = 238, .a = 255},
            .FarRidge = {.r = 128, .g = 166, .b = 196, .a = 255},
            .NearRidge = {.r = 92, .g = 140, .b = 128, .a = 255},
            .RidgeRim = {.r = 210, .g = 230, .b = 236, .a = 255},
            .Surface = {.r = 96, .g = 176, .b = 62, .a = 255},
            .SurfaceDark = {.r = 62, .g = 134, .b = 46, .a = 255},
            .Soil = {.r = 136, .g = 94, .b = 58, .a = 255},
            .SoilDark = {.r = 104, .g = 70, .b = 44, .a = 255},
            .Rock = {.r = 116, .g = 110, .b = 104, .a = 255},
            .RockDark = {.r = 84, .g = 80, .b = 76, .a = 255},
            .LiquidTop = {.r = 64, .g = 128, .b = 208, .a = 255},
            .LiquidDeep = {.r = 18, .g = 46, .b = 108, .a = 255},
            .Ambient = {.r = 255, .g = 248, .b = 196, .a = 255},
            .Cloud = {.r = 255, .g = 255, .b = 255, .a = 255},
            .LiquidIsLava = false,
            .AmbientFallSpeed = 12.f
        },
        {
            .Name = "Dunes",
            .SkyTop = {.r = 84, .g = 146, .b = 214, .a = 255},
            .SkyBottom = {.r = 250, .g = 214, .b = 164, .a = 255},
            .FarRidge = {.r = 214, .g = 168, .b = 126, .a = 255},
            .NearRidge = {.r = 200, .g = 142, .b = 92, .a = 255},
            .RidgeRim = {.r = 250, .g = 220, .b = 170, .a = 255},
            .Surface = {.r = 238, .g = 206, .b = 136, .a = 255},
            .SurfaceDark = {.r = 218, .g = 180, .b = 108, .a = 255},
            .Soil = {.r = 206, .g = 156, .b = 94, .a = 255},
            .SoilDark = {.r = 174, .g = 124, .b = 72, .a = 255},
            .Rock = {.r = 156, .g = 98, .b = 62, .a = 255},
            .RockDark = {.r = 122, .g = 76, .b = 50, .a = 255},
            .LiquidTop = {.r = 52, .g = 176, .b = 184, .a = 255},
            .LiquidDeep = {.r = 18, .g = 84, .b = 112, .a = 255},
            .Ambient = {.r = 244, .g = 214, .b = 166, .a = 255},
            .Cloud = {.r = 255, .g = 244, .b = 228, .a = 255},
            .LiquidIsLava = false,
            .AmbientFallSpeed = 4.f
        },
        {
            .Name = "Tundra",
            .SkyTop = {.r = 38, .g = 66, .b = 118, .a = 255},
            .SkyBottom = {.r = 176, .g = 204, .b = 228, .a = 255},
            .FarRidge = {.r = 150, .g = 172, .b = 204, .a = 255},
            .NearRidge = {.r = 112, .g = 136, .b = 170, .a = 255},
            .RidgeRim = {.r = 244, .g = 248, .b = 255, .a = 255},
            .Surface = {.r = 242, .g = 247, .b = 252, .a = 255},
            .SurfaceDark = {.r = 206, .g = 222, .b = 240, .a = 255},
            .Soil = {.r = 112, .g = 102, .b = 104, .a = 255},
            .SoilDark = {.r = 86, .g = 78, .b = 82, .a = 255},
            .Rock = {.r = 122, .g = 138, .b = 160, .a = 255},
            .RockDark = {.r = 88, .g = 100, .b = 124, .a = 255},
            .LiquidTop = {.r = 86, .g = 146, .b = 196, .a = 255},
            .LiquidDeep = {.r = 20, .g = 48, .b = 92, .a = 255},
            .Ambient = {.r = 255, .g = 255, .b = 255, .a = 255},
            .Cloud = {.r = 226, .g = 234, .b = 246, .a = 255},
            .LiquidIsLava = false,
            .AmbientFallSpeed = 42.f
        },
        {
            .Name = "Volcano",
            .SkyTop = {.r = 26, .g = 10, .b = 18, .a = 255},
            .SkyBottom = {.r = 130, .g = 44, .b = 26, .a = 255},
            .FarRidge = {.r = 74, .g = 30, .b = 30, .a = 255},
            .NearRidge = {.r = 46, .g = 24, .b = 28, .a = 255},
            .RidgeRim = {.r = 210, .g = 86, .b = 40, .a = 255},
            .Surface = {.r = 78, .g = 72, .b = 78, .a = 255},
            .SurfaceDark = {.r = 56, .g = 52, .b = 58, .a = 255},
            .Soil = {.r = 64, .g = 54, .b = 58, .a = 255},
            .SoilDark = {.r = 48, .g = 40, .b = 44, .a = 255},
            .Rock = {.r = 44, .g = 38, .b = 44, .a = 255},
            .RockDark = {.r = 30, .g = 26, .b = 32, .a = 255},
            .LiquidTop = {.r = 255, .g = 150, .b = 40, .a = 255},
            .LiquidDeep = {.r = 196, .g = 40, .b = 12, .a = 255},
            .Ambient = {.r = 255, .g = 150, .b = 60, .a = 255},
            .Cloud = {.r = 70, .g = 50, .b = 52, .a = 255},
            .LiquidIsLava = true,
            .AmbientFallSpeed = -34.f
        }
    }};

    // ---------- Noise ----------

    std::uint32_t Hash(const int X, const int Y, const std::uint32_t Seed)
    {
        std::uint32_t H = Seed ^ (static_cast<std::uint32_t>(X) * 374761393u) ^ (static_cast<std::uint32_t>(Y) * 668265263u);
        H = (H ^ (H >> 13)) * 1274126177u;
        return H ^ (H >> 16);
    }

    float Hash01(const int X, const int Y, const std::uint32_t Seed)
    {
        return static_cast<float>(Hash(X, Y, Seed) >> 8) * (1.f / 16777216.f);
    }

    float ValueNoise(const float X, const float Y, const std::uint32_t Seed)
    {
        const float Fx = std::floor(X);
        const float Fy = std::floor(Y);
        const int X0 = static_cast<int>(Fx);
        const int Y0 = static_cast<int>(Fy);
        const float Tx = X - Fx;
        const float Ty = Y - Fy;
        const float Sx = Tx * Tx * (3.f - 2.f * Tx);
        const float Sy = Ty * Ty * (3.f - 2.f * Ty);

        const float A = Hash01(X0, Y0, Seed);
        const float B = Hash01(X0 + 1, Y0, Seed);
        const float C = Hash01(X0, Y0 + 1, Seed);
        const float D = Hash01(X0 + 1, Y0 + 1, Seed);
        return Lerp(Lerp(A, B, Sx), Lerp(C, D, Sx), Sy);
    }

    // Normalized to 0..1
    float Fbm(const float X, const float Y, const std::uint32_t Seed, const int Octaves)
    {
        float Sum = 0.f;
        float Amplitude = 0.5f;
        float Frequency = 1.f;
        float Total = 0.f;
        for (int i = 0; i < Octaves; ++i)
        {
            Sum += ValueNoise(X * Frequency, Y * Frequency, Seed + static_cast<std::uint32_t>(i) * 1013u) * Amplitude;
            Total += Amplitude;
            Amplitude *= 0.5f;
            Frequency *= 2.03f;
        }
        return Sum / Total;
    }

    Color Shade(const Color C, const float Factor)
    {
        return {
            .r = static_cast<unsigned char>(std::clamp(static_cast<float>(C.r) * Factor, 0.f, 255.f)),
            .g = static_cast<unsigned char>(std::clamp(static_cast<float>(C.g) * Factor, 0.f, 255.f)),
            .b = static_cast<unsigned char>(std::clamp(static_cast<float>(C.b) * Factor, 0.f, 255.f)),
            .a = C.a
        };
    }

    // ---------- Shape ----------

    struct ShapeParams
    {
        float SurfaceBase;   // fraction of map height where land tops out on average
        float SurfaceSwing;  // fraction of map height the surface wanders
        float Roughness;     // overhang/floating chunk strength
        float CaveAmount;
        float IslandBreak;   // higher = more water gaps
    };

    ShapeParams GetShapeParams(const BiomeKind Kind)
    {
        switch (Kind)
        {
            case BiomeKind::Dunes:   return {.SurfaceBase = 0.52f, .SurfaceSwing = 0.18f, .Roughness = 0.7f, .CaveAmount = 0.4f, .IslandBreak = 0.36f};
            case BiomeKind::Tundra:  return {.SurfaceBase = 0.44f, .SurfaceSwing = 0.30f, .Roughness = 1.3f, .CaveAmount = 0.8f, .IslandBreak = 0.40f};
            case BiomeKind::Volcano: return {.SurfaceBase = 0.46f, .SurfaceSwing = 0.28f, .Roughness = 1.1f, .CaveAmount = 1.f, .IslandBreak = 0.42f};
            default:                 return {.SurfaceBase = 0.48f, .SurfaceSwing = 0.24f, .Roughness = 1.f, .CaveAmount = 0.7f, .IslandBreak = 0.38f};
        }
    }

    void ShapeLand(TerrainMap& Map, const BiomeKind Kind, const std::uint32_t Seed, const float LiquidLevel)
    {
        const ShapeParams Params = GetShapeParams(Kind);
        const float W = static_cast<float>(Map.Width);
        const float H = static_cast<float>(Map.Height);

        // Density evaluated on a coarse grid then interpolated; per-pixel fbm would take seconds in Debug
        constexpr int Step = 4;
        const int GridW = Map.Width / Step + 2;
        const int GridH = Map.Height / Step + 2;
        std::vector<float> Density(static_cast<size_t>(GridW * GridH));

        std::vector<float> SurfaceY(static_cast<size_t>(GridW));
        for (int Gx = 0; Gx < GridW; ++Gx)
        {
            const float X = static_cast<float>(Gx * Step);
            const float Nx = X / W;

            float Surface = H * (Params.SurfaceBase + (Fbm(X / 900.f, 0.5f, Seed, 4) - 0.5f) * 2.f * Params.SurfaceSwing);

            // Island mask drops the surface under the liquid to split landmasses
            const float Island = Fbm(X / 520.f, 3.7f, Seed + 17u, 2);
            const float Keep = std::clamp((Island - Params.IslandBreak) / 0.16f, 0.f, 1.f);

            // Map edges always end in liquid
            const float Edge = std::clamp(std::min(Nx, 1.f - Nx) / 0.12f, 0.f, 1.f);

            Surface = Lerp(LiquidLevel + 160.f, Surface, Keep * Edge);
            SurfaceY[static_cast<size_t>(Gx)] = Surface;
        }

        for (int Gy = 0; Gy < GridH; ++Gy)
        {
            for (int Gx = 0; Gx < GridW; ++Gx)
            {
                const float X = static_cast<float>(Gx * Step);
                const float Y = static_cast<float>(Gy * Step);

                float D = (Y - SurfaceY[static_cast<size_t>(Gx)]) / 140.f;
                D += (Fbm(X / 170.f, Y / 170.f, Seed + 3u, 4) - 0.5f) * 2.4f * Params.Roughness;
                D += (Fbm(X / 48.f, Y / 48.f, Seed + 5u, 2) - 0.5f) * 0.5f;

                // Tunnels follow the zero line of a noise field
                const float Cave = Fbm(X / 210.f, Y / 130.f, Seed + 11u, 3);
                const float CaveWidth = 0.035f * Params.CaveAmount;
                if (Y > SurfaceY[static_cast<size_t>(Gx)] + 40.f)
                {
                    D -= std::max(0.f, CaveWidth - std::abs(Cave - 0.5f)) * 60.f;
                }

                // Keep the sky clear for airstrikes and the camera
                D -= std::max(0.f, 0.16f - Y / H) * 30.f;

                Density[static_cast<size_t>(Gy * GridW + Gx)] = D;
            }
        }

        for (int Y = 0; Y < Map.Height; ++Y)
        {
            const int Gy = Y / Step;
            const float Ty = static_cast<float>(Y % Step) / Step;
            for (int X = 0; X < Map.Width; ++X)
            {
                const int Gx = X / Step;
                const float Tx = static_cast<float>(X % Step) / Step;
                const float A = Density[static_cast<size_t>(Gy * GridW + Gx)];
                const float B = Density[static_cast<size_t>(Gy * GridW + Gx + 1)];
                const float C = Density[static_cast<size_t>((Gy + 1) * GridW + Gx)];
                const float D = Density[static_cast<size_t>((Gy + 1) * GridW + Gx + 1)];
                const float Value = Lerp(Lerp(A, B, Tx), Lerp(C, D, Tx), Ty);
                Map.Solid[static_cast<size_t>(Y * Map.Width + X)] = Value > 0.f ? 1 : 0;
            }
        }
    }

    // Drops specks of land too small to stand on and fills pinholes in the rock
    void CleanupComponents(TerrainMap& Map, const std::uint8_t Value, const int MinPixels)
    {
        ++Map.VisitGeneration;
        const std::uint32_t Generation = Map.VisitGeneration;
        std::vector<int> Stack;
        std::vector<int> Filled;

        for (int Start = 0; Start < Map.Width * Map.Height; ++Start)
        {
            if (Map.Solid[static_cast<size_t>(Start)] != Value || Map.VisitMark[static_cast<size_t>(Start)] == Generation)
            {
                continue;
            }

            Stack.clear();
            Filled.clear();
            Stack.push_back(Start);
            Map.VisitMark[static_cast<size_t>(Start)] = Generation;
            bool TouchesBorder = false;

            while (!Stack.empty())
            {
                const int Index = Stack.back();
                Stack.pop_back();
                Filled.push_back(Index);
                const int X = Index % Map.Width;
                const int Y = Index / Map.Width;
                TouchesBorder |= X == 0 || Y == 0 || X == Map.Width - 1 || Y == Map.Height - 1;

                const int Neighbors[4][2] = {{X - 1, Y}, {X + 1, Y}, {X, Y - 1}, {X, Y + 1}};
                for (const auto& N : Neighbors)
                {
                    if (N[0] < 0 || N[1] < 0 || N[0] >= Map.Width || N[1] >= Map.Height)
                    {
                        continue;
                    }
                    const int NeighborIndex = N[1] * Map.Width + N[0];
                    if (Map.Solid[static_cast<size_t>(NeighborIndex)] == Value && Map.VisitMark[static_cast<size_t>(NeighborIndex)] != Generation)
                    {
                        Map.VisitMark[static_cast<size_t>(NeighborIndex)] = Generation;
                        Stack.push_back(NeighborIndex);
                    }
                }
            }

            // Air touching the border is the sky, never a hole to fill
            const bool KeepComponent = static_cast<int>(Filled.size()) >= MinPixels || (Value == 0 && TouchesBorder);
            if (!KeepComponent)
            {
                for (const int Index : Filled)
                {
                    Map.Solid[static_cast<size_t>(Index)] = Value == 0 ? 1 : 0;
                }
            }
        }
    }

    // ---------- Materials ----------

    void PaintMaterials(TerrainMap& Map, const BiomeKind Kind, const std::uint32_t Seed)
    {
        const BiomeTheme& Theme = GetBiomeTheme(Kind);
        const int SurfaceExtra = Kind == BiomeKind::Tundra ? 6 : (Kind == BiomeKind::Dunes ? 10 : 0);

        for (int X = 0; X < Map.Width; ++X)
        {
            const int SurfaceThickness = 5 + SurfaceExtra + static_cast<int>(Hash01(X / 3, 0, Seed) * 3.f);
            const int SoilThickness = 70 + static_cast<int>(ValueNoise(static_cast<float>(X) / 60.f, 1.f, Seed + 21u) * 70.f);
            const float Strata = Fbm(static_cast<float>(X) / 240.f, 2.f, Seed + 23u, 3) * 6.f;

            int Depth = 0;
            for (int Y = 0; Y < Map.Height; ++Y)
            {
                const size_t Index = static_cast<size_t>(Y * Map.Width + X);
                if (!Map.Solid[Index])
                {
                    Depth = 0;
                    Map.Pixels[Index] = BLANK;
                    continue;
                }

                const float Fx = static_cast<float>(X);
                const float Fy = static_cast<float>(Y);
                float Grain = 0.94f + Hash01(X, Y, Seed + 31u) * 0.1f;
                Color C;

                if (Depth < SurfaceThickness)
                {
                    const float Blend = Fbm(Fx / 10.f, Fy / 10.f, Seed + 33u, 2);
                    C = ColorLerp(Theme.Surface, Theme.SurfaceDark, std::clamp((Blend - 0.35f) * 2.f, 0.f, 1.f));
                    // Fade the bottom edge of the surface layer into the soil instead of a hard line
                    if (Depth >= SurfaceThickness - 2)
                    {
                        C = ColorLerp(C, Theme.Soil, 0.4f);
                    }
                }
                else if (Depth < SurfaceThickness + SoilThickness + static_cast<int>((Fbm(Fx / 26.f, Fy / 26.f, Seed + 45u, 2) - 0.5f) * 40.f))
                {
                    const float Spots = Fbm(Fx / 16.f, Fy / 16.f, Seed + 35u, 2);
                    C = Spots > 0.6f ? Theme.SoilDark : Theme.Soil;

                    // Pebbles: one candidate per 9x9 cell
                    const int CellX = X / 9;
                    const int CellY = Y / 9;
                    if (Hash01(CellX, CellY, Seed + 37u) < 0.35f)
                    {
                        const float PebbleX = static_cast<float>(CellX * 9) + 2.f + Hash01(CellX, CellY, Seed + 38u) * 5.f;
                        const float PebbleY = static_cast<float>(CellY * 9) + 2.f + Hash01(CellX, CellY, Seed + 39u) * 5.f;
                        const float PebbleR = 1.2f + Hash01(CellX, CellY, Seed + 40u) * 1.6f;
                        if (Vector2Distance({.x = Fx, .y = Fy}, {.x = PebbleX, .y = PebbleY}) < PebbleR)
                        {
                            C = ColorLerp(Theme.Rock, WHITE, 0.15f);
                        }
                    }
                }
                else
                {
                    const float Band = 0.5f + 0.5f * std::sin(Fy * 0.045f + Strata + Fbm(Fx / 90.f, Fy / 70.f, Seed + 41u, 3) * 7.f);
                    const float Cracks = Fbm(Fx / 22.f, Fy / 22.f, Seed + 47u, 3);
                    C = ColorLerp(Theme.Rock, Theme.RockDark, Band * 0.35f + std::max(0.f, Cracks - 0.6f) * 1.5f);

                    if (Kind == BiomeKind::Volcano && std::abs(Fbm(Fx / 70.f, Fy / 70.f, Seed + 43u, 3) - 0.5f) < 0.006f)
                    {
                        C = {.r = 236, .g = 96, .b = 24, .a = 255}; // glowing magma vein
                        Grain = 1.f;
                    }
                }

                // Darken undersides and walls a bit so overhangs read as 3D
                const bool AirBelow = Y + 1 < Map.Height && !Map.Solid[Index + static_cast<size_t>(Map.Width)];
                const bool AirSide = (X > 0 && !Map.Solid[Index - 1]) || (X + 1 < Map.Width && !Map.Solid[Index + 1]);
                float Light = Grain;
                if (AirBelow)
                {
                    Light *= 0.72f;
                }
                else if (AirSide && Depth >= SurfaceThickness)
                {
                    Light *= 0.86f;
                }

                Map.Pixels[Index] = Shade(C, Light);
                ++Depth;
            }
        }
    }

    // ---------- Decorations ----------

    // Decorations are stamped as real solid pixels, so trees and cacti can be blown apart and fall like everything else
    void StampDisc(TerrainMap& Map, const float Cx, const float Cy, const float R, const Color Base, const std::uint32_t Seed)
    {
        for (int Y = static_cast<int>(Cy - R); Y <= static_cast<int>(Cy + R); ++Y)
        {
            for (int X = static_cast<int>(Cx - R); X <= static_cast<int>(Cx + R); ++X)
            {
                const float Dx = static_cast<float>(X) - Cx;
                const float Dy = static_cast<float>(Y) - Cy;
                if (Dx * Dx + Dy * Dy > R * R)
                {
                    continue;
                }
                // Light from the top left
                const float Light = 1.08f - ((Dx + Dy) / (2.f * R) + 0.5f) * 0.3f;
                const float Grain = 0.92f + Hash01(X, Y, Seed) * 0.14f;
                SetTerrainPixel(Map, X, Y, Shade(Base, Light * Grain), true);
            }
        }
    }

    void StampRect(TerrainMap& Map, const int X0, const int Y0, const int W, const int H, const Color Base, const std::uint32_t Seed)
    {
        for (int Y = Y0; Y < Y0 + H; ++Y)
        {
            for (int X = X0; X < X0 + W; ++X)
            {
                // Vertical streaks read as bark or cactus ribs
                const float Streak = (X - X0) % 3 == 0 ? 0.86f : 1.f;
                const float Edge = (X == X0 || X == X0 + W - 1) ? 0.8f : 1.f;
                SetTerrainPixel(Map, X, Y, Shade(Base, Streak * Edge * (0.94f + Hash01(X, Y, Seed) * 0.1f)), true);
            }
        }
    }

    void StampTriangle(TerrainMap& Map, const float Cx, const float TopY, const float HalfWidth, const float Height, const Color Base, const Color Cap, const std::uint32_t Seed)
    {
        for (int Y = static_cast<int>(TopY); Y <= static_cast<int>(TopY + Height); ++Y)
        {
            const float T = (static_cast<float>(Y) - TopY) / Height;
            const float Half = HalfWidth * T;
            for (int X = static_cast<int>(Cx - Half); X <= static_cast<int>(Cx + Half); ++X)
            {
                const float EdgeDist = std::min(static_cast<float>(X) - (Cx - Half), (Cx + Half) - static_cast<float>(X));
                const bool IsCap = Cap.a > 0 && (T < 0.25f || EdgeDist < 1.5f) && (static_cast<float>(Y) - TopY) < Height * 0.6f;
                const float Light = static_cast<float>(X) < Cx ? 1.05f : 0.85f;
                SetTerrainPixel(Map, X, Y, Shade(IsCap ? Cap : Base, Light * (0.94f + Hash01(X, Y, Seed) * 0.1f)), true);
            }
        }
    }

    int FindSurfaceY(const TerrainMap& Map, const int X)
    {
        for (int Y = 0; Y < Map.Height; ++Y)
        {
            if (IsSolidAt(Map, X, Y))
            {
                return Y;
            }
        }
        return Map.Height;
    }

    void PlaceDecorations(TerrainMap& Map, const BiomeKind Kind, const std::uint32_t Seed, const float LiquidLevel)
    {
        std::mt19937 Rng(Seed + 101u);
        std::uniform_real_distribution<float> Unit(0.f, 1.f);
        std::vector<int> Placed;

        const int Attempts = 220;
        const int Target = Kind == BiomeKind::Volcano ? 12 : 16;

        for (int Attempt = 0; Attempt < Attempts && static_cast<int>(Placed.size()) < Target; ++Attempt)
        {
            const int X = 60 + static_cast<int>(Unit(Rng) * static_cast<float>(Map.Width - 120));
            const int Ground = FindSurfaceY(Map, X);
            if (Ground < 140 || static_cast<float>(Ground) > LiquidLevel - 40.f)
            {
                continue;
            }

            // Only on reasonably flat ground
            if (std::abs(FindSurfaceY(Map, X - 8) - Ground) > 6 || std::abs(FindSurfaceY(Map, X + 8) - Ground) > 6)
            {
                continue;
            }

            if (std::any_of(Placed.begin(), Placed.end(), [X](const int Other) { return std::abs(Other - X) < 110; }))
            {
                continue;
            }
            Placed.push_back(X);

            const float Fx = static_cast<float>(X);
            const float Base = static_cast<float>(Ground) + 3.f; // sink in so it stays attached to the ground
            const std::uint32_t DecorSeed = Seed + static_cast<std::uint32_t>(Attempt) * 7u;
            const float Size = 0.8f + Unit(Rng) * 0.5f;

            switch (Kind)
            {
                case BiomeKind::Meadow:
                {
                    if (Unit(Rng) < 0.3f) // bush
                    {
                        const Color Leaf = {.r = 70, .g = 150, .b = 58, .a = 255};
                        StampDisc(Map, Fx - 8.f, Base - 8.f, 9.f * Size, Leaf, DecorSeed);
                        StampDisc(Map, Fx + 7.f, Base - 7.f, 8.f * Size, Shade(Leaf, 0.9f), DecorSeed + 1u);
                        StampDisc(Map, Fx, Base - 13.f, 9.f * Size, Shade(Leaf, 1.08f), DecorSeed + 2u);
                        break;
                    }
                    const int TrunkH = static_cast<int>(48.f * Size);
                    StampRect(Map, X - 4, static_cast<int>(Base) - TrunkH, 8, TrunkH + 2, {.r = 100, .g = 68, .b = 42, .a = 255}, DecorSeed);
                    const float TopY = Base - static_cast<float>(TrunkH);
                    const Color Leaf = Unit(Rng) < 0.5f ? Color{.r = 64, .g = 148, .b = 56, .a = 255} : Color{.r = 92, .g = 160, .b = 48, .a = 255};
                    StampDisc(Map, Fx - 14.f * Size, TopY + 2.f, 15.f * Size, Shade(Leaf, 0.88f), DecorSeed + 1u);
                    StampDisc(Map, Fx + 14.f * Size, TopY + 4.f, 14.f * Size, Shade(Leaf, 0.84f), DecorSeed + 2u);
                    StampDisc(Map, Fx, TopY - 10.f * Size, 19.f * Size, Leaf, DecorSeed + 3u);
                    StampDisc(Map, Fx + 6.f, TopY - 22.f * Size, 12.f * Size, Shade(Leaf, 1.1f), DecorSeed + 4u);
                    break;
                }
                case BiomeKind::Dunes:
                {
                    if (Unit(Rng) < 0.3f) // boulder
                    {
                        StampDisc(Map, Fx, Base - 6.f, 12.f * Size, {.r = 170, .g = 120, .b = 84, .a = 255}, DecorSeed);
                        break;
                    }
                    const Color Cactus = {.r = 72, .g = 146, .b = 74, .a = 255};
                    const int TrunkH = static_cast<int>(44.f * Size);
                    StampRect(Map, X - 5, static_cast<int>(Base) - TrunkH, 10, TrunkH + 2, Cactus, DecorSeed);
                    StampDisc(Map, Fx, Base - static_cast<float>(TrunkH), 5.f, Cactus, DecorSeed);

                    const int ArmY = static_cast<int>(Base) - TrunkH / 2;
                    StampRect(Map, X - 15, ArmY, 11, 6, Cactus, DecorSeed + 1u);
                    StampRect(Map, X - 15, ArmY - 14, 6, 15, Cactus, DecorSeed + 2u);
                    StampDisc(Map, Fx - 12.f, static_cast<float>(ArmY - 14), 3.f, Cactus, DecorSeed);
                    if (Unit(Rng) < 0.6f)
                    {
                        StampRect(Map, X + 4, ArmY + 6, 11, 6, Cactus, DecorSeed + 3u);
                        StampRect(Map, X + 9, ArmY - 6, 6, 13, Cactus, DecorSeed + 4u);
                        StampDisc(Map, Fx + 12.f, static_cast<float>(ArmY - 6), 3.f, Cactus, DecorSeed);
                    }
                    break;
                }
                case BiomeKind::Tundra:
                {
                    const Color Needles = {.r = 34, .g = 86, .b = 66, .a = 255};
                    const Color Snow = {.r = 240, .g = 246, .b = 252, .a = 255};
                    StampRect(Map, X - 3, static_cast<int>(Base) - 14, 6, 16, {.r = 86, .g = 60, .b = 44, .a = 255}, DecorSeed);
                    const int Layers = 3 + (Unit(Rng) < 0.5f ? 1 : 0);
                    for (int L = 0; L < Layers; ++L)
                    {
                        const float LayerTop = Base - 12.f - static_cast<float>(Layers - L) * 15.f * Size - 10.f;
                        StampTriangle(Map, Fx, LayerTop, (9.f + static_cast<float>(L) * 5.f) * Size, 24.f * Size, Needles, Snow, DecorSeed + static_cast<std::uint32_t>(L));
                    }
                    break;
                }
                case BiomeKind::Volcano:
                {
                    const Color Obsidian = {.r = 44, .g = 32, .b = 56, .a = 255};
                    const float SpireH = 50.f * Size + Unit(Rng) * 30.f;
                    StampTriangle(Map, Fx, Base - SpireH, 10.f * Size, SpireH + 2.f, Obsidian, BLANK, DecorSeed);
                    StampTriangle(Map, Fx - 11.f, Base - SpireH * 0.55f, 6.f * Size, SpireH * 0.55f + 2.f, Shade(Obsidian, 1.2f), BLANK, DecorSeed + 1u);
                    StampDisc(Map, Fx + 2.f, Base - SpireH * 0.35f, 2.5f, {.r = 255, .g = 120, .b = 40, .a = 255}, DecorSeed);
                    break;
                }
                default: break;
            }
        }
    }

    // Visual-only grass blades: not solid, so they never block movement
    void PlantGrass(TerrainMap& Map, const std::uint32_t Seed)
    {
        for (int X = 0; X < Map.Width; ++X)
        {
            for (int Y = 1; Y < Map.Height; ++Y)
            {
                const size_t Index = static_cast<size_t>(Y * Map.Width + X);
                if (!Map.Solid[Index] || Map.Solid[Index - static_cast<size_t>(Map.Width)])
                {
                    continue;
                }

                // Only on grassy pixels, never on tree trunks or rock faces
                const Color Ground = Map.Pixels[Index];
                if (Ground.g < Ground.r + 30 || Hash01(X, Y, Seed + 51u) < 0.4f)
                {
                    continue;
                }

                const int BladeH = 1 + static_cast<int>(Hash01(X, Y, Seed + 52u) * 4.f);
                for (int B = 1; B <= BladeH && Y - B >= 0; ++B)
                {
                    const size_t Above = Index - static_cast<size_t>(B * Map.Width);
                    if (Map.Solid[Above])
                    {
                        break;
                    }
                    Map.Pixels[Above] = Shade(Ground, 1.05f + static_cast<float>(B) * 0.04f);
                }
            }
        }
    }

    float SampleRidge(const std::vector<float>& Ridge, const float X)
    {
        const int Count = static_cast<int>(Ridge.size());
        const float Wrapped = std::fmod(std::fmod(X, static_cast<float>(Count)) + static_cast<float>(Count), static_cast<float>(Count));
        const int I0 = static_cast<int>(Wrapped);
        const int I1 = (I0 + 1) % Count;
        return Lerp(Ridge[static_cast<size_t>(I0)], Ridge[static_cast<size_t>(I1)], Wrapped - static_cast<float>(I0));
    }

    void DrawRidge(const std::vector<float>& Ridge, const Color Fill, const Color Rim, const float ScrollX, const float BaseY, const float Amplitude, const float ScreenW, const float ScreenH)
    {
        constexpr float ColumnW = 2.f;
        constexpr float PixelsPerSample = 10.f;
        for (float Sx = 0.f; Sx < ScreenW; Sx += ColumnW)
        {
            const float Height = SampleRidge(Ridge, (Sx + ScrollX) / PixelsPerSample) * Amplitude;
            const float Top = BaseY - Height;
            DrawRectangleV({.x = Sx, .y = Top}, {.x = ColumnW, .y = ScreenH - Top}, Fill);
            DrawRectangleV({.x = Sx, .y = Top}, {.x = ColumnW, .y = 2.f}, ColorAlpha(Rim, 0.5f));
        }
    }
}

const BiomeTheme& GetBiomeTheme(const BiomeKind Kind)
{
    return Themes[static_cast<size_t>(Kind)];
}

void GenerateBiomeTerrain(TerrainMap& Map, const BiomeKind Kind, const std::uint32_t Seed, const float LiquidLevel)
{
    ShapeLand(Map, Kind, Seed, LiquidLevel);
    CleanupComponents(Map, 1, 1800);
    CleanupComponents(Map, 0, 600);
    PaintMaterials(Map, Kind, Seed);
    PlaceDecorations(Map, Kind, Seed, LiquidLevel);
    if (Kind == BiomeKind::Meadow)
    {
        PlantGrass(Map, Seed);
    }
    MarkTerrainDirty(Map, 0, 0, Map.Width - 1, Map.Height - 1);
}

std::vector<Vector2> FindSpawnPoints(const TerrainMap& Map, const int Count, float MinSpacing, const float Clearance, const float LiquidLevel, std::mt19937& Rng)
{
    std::vector<Vector2> Candidates;
    const int NeededAir = static_cast<int>(Clearance);

    for (int X = 40; X < Map.Width - 40; X += 6)
    {
        int AirRun = 0;
        for (int Y = 0; Y < Map.Height; ++Y)
        {
            if (!IsSolidAt(Map, X, Y))
            {
                ++AirRun;
                continue;
            }

            const bool Flat = IsSolidAt(Map, X - 5, Y + 3) && IsSolidAt(Map, X + 5, Y + 3);
            if (AirRun >= NeededAir && Flat && static_cast<float>(Y) < LiquidLevel - 30.f)
            {
                Candidates.push_back({.x = static_cast<float>(X), .y = static_cast<float>(Y)});
            }
            AirRun = 0;
        }
    }

    std::shuffle(Candidates.begin(), Candidates.end(), Rng);

    std::vector<Vector2> Result;
    while (static_cast<int>(Result.size()) < Count && MinSpacing > 1.f)
    {
        for (const Vector2& Candidate : Candidates)
        {
            if (static_cast<int>(Result.size()) >= Count)
            {
                break;
            }
            const bool FarEnough = std::none_of(Result.begin(), Result.end(), [&](const Vector2& Other) { return Vector2Distance(Other, Candidate) < MinSpacing; });
            if (FarEnough)
            {
                Result.push_back(Candidate);
            }
        }
        MinSpacing *= 0.75f;
    }

    // Degenerate map: drop the rest from the sky, they will land somewhere
    while (static_cast<int>(Result.size()) < Count)
    {
        Result.push_back({.x = static_cast<float>(Map.Width) * (0.2f + 0.6f * static_cast<float>(Result.size()) / static_cast<float>(Count)), .y = 40.f});
    }
    return Result;
}

void GenerateBackdrop(Backdrop& Out, const std::uint32_t Seed)
{
    constexpr int RidgeSamples = 512;
    Out.FarRidge.resize(RidgeSamples);
    Out.NearRidge.resize(RidgeSamples);

    // Sample a circle in noise space so the ridge wraps seamlessly
    for (int i = 0; i < RidgeSamples; ++i)
    {
        const float Angle = 2.f * PI * static_cast<float>(i) / RidgeSamples;
        const float Cx = std::cos(Angle);
        const float Cy = std::sin(Angle);
        Out.FarRidge[static_cast<size_t>(i)] = Fbm(Cx * 6.f + 20.f, Cy * 6.f + 20.f, Seed + 61u, 5);
        Out.NearRidge[static_cast<size_t>(i)] = Fbm(Cx * 9.f + 40.f, Cy * 9.f + 40.f, Seed + 62u, 4);
    }

    std::mt19937 Rng(Seed + 63u);
    std::uniform_real_distribution<float> Unit(0.f, 1.f);

    Out.Ambient.clear();
    for (int i = 0; i < 140; ++i)
    {
        Out.Ambient.push_back({
            .Position = {.x = Unit(Rng) * 4000.f, .y = Unit(Rng) * 2400.f},
            .Size = 1.f + Unit(Rng) * 2.f,
            .Phase = Unit(Rng) * 2.f * PI,
            .Depth = 0.3f + Unit(Rng) * 0.7f
        });
    }

    Out.Clouds.clear();
    for (int i = 0; i < 9; ++i)
    {
        Out.Clouds.push_back({
            .Position = {.x = Unit(Rng) * 3000.f, .y = 40.f + Unit(Rng) * 260.f},
            .Scale = 0.6f + Unit(Rng) * 0.9f,
            .Depth = 0.2f + Unit(Rng) * 0.5f
        });
    }
}

void UpdateBackdrop(Backdrop& Out, const BiomeTheme& Theme, const float Wind, const float DeltaTime, const float ScreenWidth, const float ScreenHeight)
{
    for (AmbientParticle& P : Out.Ambient)
    {
        P.Phase += DeltaTime * 1.7f;
        P.Position.x += (Wind * 60.f + std::sin(P.Phase) * 12.f) * P.Depth * DeltaTime;
        P.Position.y += Theme.AmbientFallSpeed * P.Depth * DeltaTime;

        // Wrap with a margin so particles don't pop at the edges
        constexpr float Margin = 20.f;
        P.Position.x = std::fmod(std::fmod(P.Position.x + Margin, ScreenWidth + Margin * 2.f) + ScreenWidth + Margin * 2.f, ScreenWidth + Margin * 2.f) - Margin;
        P.Position.y = std::fmod(std::fmod(P.Position.y + Margin, ScreenHeight + Margin * 2.f) + ScreenHeight + Margin * 2.f, ScreenHeight + Margin * 2.f) - Margin;
    }

    for (Cloud& C : Out.Clouds)
    {
        C.Position.x += (Wind * 40.f + 4.f) * C.Depth * DeltaTime;
    }
}

void DrawSkyAndRidges(const Backdrop& In, const BiomeTheme& Theme, const Camera2D& View, const float WorldHeight)
{
    const float ScreenW = static_cast<float>(GetScreenWidth());
    const float ScreenH = static_cast<float>(GetScreenHeight());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Theme.SkyTop, Theme.SkyBottom);

    // Vertical parallax: ridges sink as the camera looks up into the sky
    const float LookOffset = (View.target.y - WorldHeight * 0.5f) * View.zoom;

    for (const Cloud& C : In.Clouds)
    {
        const float Span = ScreenW + 600.f;
        const float X = std::fmod(std::fmod(C.Position.x - View.target.x * C.Depth * 0.3f, Span) + Span, Span) - 300.f;
        const float Y = C.Position.y * (ScreenH / 1080.f) - LookOffset * C.Depth * 0.2f;
        const float S = C.Scale * ScreenH / 1080.f;
        // Opaque blend toward the sky, so overlapping puffs do not show seams
        const Color Tint = ColorLerp(Theme.SkyTop, Theme.Cloud, 0.6f);
        DrawEllipse(static_cast<int>(X), static_cast<int>(Y), 90.f * S, 26.f * S, Tint);
        DrawCircleV({.x = X - 34.f * S, .y = Y - 10.f * S}, 30.f * S, Tint);
        DrawCircleV({.x = X + 20.f * S, .y = Y - 18.f * S}, 38.f * S, Tint);
    }

    DrawRidge(In.FarRidge, Theme.FarRidge, Theme.RidgeRim, View.target.x * View.zoom * 0.12f, ScreenH * 0.72f - LookOffset * 0.12f, ScreenH * 0.5f, ScreenW, ScreenH);
    DrawRidge(In.NearRidge, Theme.NearRidge, Theme.RidgeRim, View.target.x * View.zoom * 0.3f + 900.f, ScreenH * 0.86f - LookOffset * 0.3f, ScreenH * 0.4f, ScreenW, ScreenH);
}

void DrawLiquid(const BiomeTheme& Theme, const float LiquidLevel, const float Time, const Rectangle VisibleWorld, const bool Front)
{
    constexpr float ColumnW = 6.f;
    const float X0 = std::floor(VisibleWorld.x / ColumnW) * ColumnW - ColumnW;
    const float X1 = VisibleWorld.x + VisibleWorld.width + ColumnW;
    const float Bottom = std::max(VisibleWorld.y + VisibleWorld.height, LiquidLevel) + 50.f;

    for (float X = X0; X < X1; X += ColumnW)
    {
        if (Front)
        {
            const float WaveY = std::sin(X * 0.018f + Time * 1.6f) * 4.f + std::sin(X * 0.043f - Time * 2.3f) * 2.f;
            const float Top = LiquidLevel + WaveY;
            const float Alpha = Theme.LiquidIsLava ? 1.f : 0.88f;
            DrawRectangleGradientV(static_cast<int>(X), static_cast<int>(Top), static_cast<int>(ColumnW), static_cast<int>(Bottom - Top), ColorAlpha(Theme.LiquidTop, Alpha), ColorAlpha(Theme.LiquidDeep, Alpha));
            DrawRectangleV({.x = X, .y = Top}, {.x = ColumnW, .y = 3.f}, ColorAlpha(ColorLerp(Theme.LiquidTop, WHITE, Theme.LiquidIsLava ? 0.6f : 0.45f), 0.9f));
        }
        else
        {
            const float WaveY = std::sin(X * 0.012f - Time * 1.1f) * 5.f;
            const float Top = LiquidLevel - 10.f + WaveY;
            DrawRectangleV({.x = X, .y = Top}, {.x = ColumnW, .y = Bottom - Top}, ColorLerp(Theme.LiquidDeep, Theme.LiquidTop, 0.4f));
        }
    }

    // Lava bubbles pop at the surface
    if (Front && Theme.LiquidIsLava)
    {
        for (int i = 0; i < 24; ++i)
        {
            const float Cycle = std::fmod(Time * 0.6f + static_cast<float>(i) * 0.37f, 1.f);
            const float X = X0 + std::fmod(static_cast<float>(i) * 397.f + std::floor(Time * 0.6f + static_cast<float>(i) * 0.37f) * 131.f, X1 - X0);
            DrawCircleV({.x = X, .y = LiquidLevel + 6.f - Cycle * 10.f}, 2.f + Cycle * 4.f, ColorAlpha({.r = 255, .g = 220, .b = 120, .a = 255}, 1.f - Cycle));
        }
    }
}

void DrawAmbient(const Backdrop& In, const BiomeTheme& Theme)
{
    for (const AmbientParticle& P : In.Ambient)
    {
        float Alpha = 0.35f + 0.5f * P.Depth;
        if (Theme.LiquidIsLava)
        {
            Alpha *= 0.6f + 0.4f * std::sin(P.Phase * 3.f); // embers flicker
        }
        DrawCircleV(P.Position, P.Size * (0.6f + P.Depth), ColorAlpha(Theme.Ambient, Alpha));
    }
}
