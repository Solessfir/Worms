#pragma once

#include <cstdint>
#include <random>
#include <vector>
#include "Terrain.h"

enum class BiomeKind : std::uint8_t
{
    Meadow,
    Dunes,
    Tundra,
    Volcano,
    Count
};

struct BiomeTheme
{
    const char* Name;
    Color SkyTop;
    Color SkyBottom;
    Color FarRidge;
    Color NearRidge;
    Color RidgeRim;
    Color Surface;
    Color SurfaceDark;
    Color Soil;
    Color SoilDark;
    Color Rock;
    Color RockDark;
    Color LiquidTop;
    Color LiquidDeep;
    Color Ambient;
    Color Cloud;
    bool LiquidIsLava;
    float AmbientFallSpeed; // screen px/s, negative rises (embers)
};

const BiomeTheme& GetBiomeTheme(BiomeKind Kind);

void GenerateBiomeTerrain(TerrainMap& Map, BiomeKind Kind, std::uint32_t Seed, float LiquidLevel);

// Ground points with Clearance px of open air above, at least MinSpacing apart where possible
std::vector<Vector2> FindSpawnPoints(const TerrainMap& Map, int Count, float MinSpacing, float Clearance, float LiquidLevel, std::mt19937& Rng);

struct AmbientParticle
{
    Vector2 Position;
    float Size;
    float Phase;
    float Depth;
};

struct Cloud
{
    Vector2 Position; // screen px at the reference height
    float Scale;
    float Depth;
};

struct Backdrop
{
    std::vector<float> FarRidge;
    std::vector<float> NearRidge;
    std::vector<AmbientParticle> Ambient;
    std::vector<Cloud> Clouds;
};

void GenerateBackdrop(Backdrop& Out, std::uint32_t Seed);

void UpdateBackdrop(Backdrop& Out, const BiomeTheme& Theme, float Wind, float DeltaTime, float ScreenWidth, float ScreenHeight);

// Screen space, drawn before the world camera
void DrawSkyAndRidges(const Backdrop& In, const BiomeTheme& Theme, const Camera2D& View, float WorldHeight);

// World space. Back pass goes behind the terrain, front pass over everything in the world
void DrawLiquid(const BiomeTheme& Theme, float LiquidLevel, float Time, Rectangle VisibleWorld, bool Front);

// Screen space, drawn over the world
void DrawAmbient(const Backdrop& In, const BiomeTheme& Theme);
