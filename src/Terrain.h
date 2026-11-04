#pragma once

#include <cstdint>
#include <vector>
#include "Physics.h"

// Destructible pixel terrain. Solid drives collision, Pixels drives visuals.
// A pixel can be visible but not solid (grass blades), never solid but invisible.
struct TerrainMap
{
    int Width = 0;
    int Height = 0;
    std::vector<std::uint8_t> Solid;
    std::vector<Color> Pixels;

    Texture2D Texture{};
    int DirtyRowMin = 0;
    int DirtyRowMax = -1;

    // One static body per chunk holding that chunk's contour chains, so a crater only rebuilds the chunks it touched
    static constexpr int ChunkSize = 64;
    int ChunksX = 0;
    int ChunksY = 0;
    std::vector<b2BodyId> ChunkBodies;
    std::vector<std::uint8_t> ChunkDirty;

    // Flood fill scratch, generation counter avoids clearing millions of entries per fill
    std::vector<std::uint32_t> VisitMark;
    std::uint32_t VisitGeneration = 0;
};

// A floating piece cut out of the terrain, ready to become a debris body
struct TerrainPiece
{
    int X = 0;
    int Y = 0;
    int Width = 0;
    int Height = 0;
    int SolidCount = 0;
    std::vector<std::uint8_t> Solid;
    std::vector<Color> Pixels;
};

void InitTerrain(TerrainMap& Map, int Width, int Height);

void UnloadTerrain(TerrainMap& Map);

inline bool IsSolidAt(const TerrainMap& Map, const int X, const int Y)
{
    return X >= 0 && Y >= 0 && X < Map.Width && Y < Map.Height && Map.Solid[static_cast<size_t>(Y * Map.Width + X)] != 0;
}

bool IsCircleFree(const TerrainMap& Map, Vector2 Center, float Radius);

// Inclusive pixel rect: marks texture rows and collision chunks for the next SyncTerrain
void MarkTerrainDirty(TerrainMap& Map, int X0, int Y0, int X1, int Y1);

void SetTerrainPixel(TerrainMap& Map, int X, int Y, Color C, bool IsSolid);

// Removes a disc and scorches a ring around it. Returns the color of a solid pixel that was removed (for dirt particles), or blank if the disc was empty
Color CarveTerrain(TerrainMap& Map, Vector2 Center, float Radius);

// After a carve, finds solid regions near the crater that are no longer attached to anything big and cuts them out
std::vector<TerrainPiece> DetachFloatingPieces(TerrainMap& Map, Vector2 Center, float Radius, int MaxPiecePixels);

// Uploads dirty texture rows and rebuilds dirty collision chunks
void SyncTerrain(TerrainMap& Map, b2WorldId World);
