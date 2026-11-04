#include "Terrain.h"
#include "raylib/raymath.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
    // Marching squares points live on cell edges. Doubled integer coordinates keep them exact (world px = value * 0.5)
    struct EdgePoint
    {
        int X;
        int Y;
    };

    struct Segment
    {
        EdgePoint A;
        EdgePoint B;
    };

    bool SamePoint(const EdgePoint A, const EdgePoint B)
    {
        return A.X == B.X && A.Y == B.Y;
    }

    std::uint64_t PackKey(const EdgePoint P)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(P.X + 8)) << 32) | static_cast<std::uint32_t>(P.Y + 8);
    }

    Vector2 ToWorld(const EdgePoint P)
    {
        return {.x = static_cast<float>(P.X) * 0.5f, .y = static_cast<float>(P.Y) * 0.5f};
    }

    // Cell (Cx, Cy) has corner pixels Cx-1..Cx and Cy-1..Cy, so cells 0..Width cover the map plus one ring of air and every contour closes.
    // Segments come out with solid on the left, because Box2D chains only collide on their right side.
    int EmitCellSegments(const TerrainMap& Map, const int Cx, const int Cy, Segment Out[2])
    {
        const bool TL = IsSolidAt(Map, Cx - 1, Cy - 1);
        const bool TR = IsSolidAt(Map, Cx, Cy - 1);
        const bool BL = IsSolidAt(Map, Cx - 1, Cy);
        const bool BR = IsSolidAt(Map, Cx, Cy);
        const int Case = (TL ? 8 : 0) | (TR ? 4 : 0) | (BR ? 2 : 0) | (BL ? 1 : 0);
        if (Case == 0 || Case == 15)
        {
            return 0;
        }

        const EdgePoint Top = {.X = 2 * Cx, .Y = 2 * Cy - 1};
        const EdgePoint Bottom = {.X = 2 * Cx, .Y = 2 * Cy + 1};
        const EdgePoint Left = {.X = 2 * Cx - 1, .Y = 2 * Cy};
        const EdgePoint Right = {.X = 2 * Cx + 1, .Y = 2 * Cy};
        const EdgePoint CornerTL = {.X = 2 * Cx - 1, .Y = 2 * Cy - 1};
        const EdgePoint CornerTR = {.X = 2 * Cx + 1, .Y = 2 * Cy - 1};
        const EdgePoint CornerBL = {.X = 2 * Cx - 1, .Y = 2 * Cy + 1};
        const EdgePoint CornerBR = {.X = 2 * Cx + 1, .Y = 2 * Cy + 1};

        // Solid corners centroid tells which side is solid; saddles pass the single solid corner they cut off instead
        float RefX = 0.f;
        float RefY = 0.f;
        int SolidCount = 0;
        for (const auto& [IsSolid, Corner] : {std::pair{TL, CornerTL}, std::pair{TR, CornerTR}, std::pair{BL, CornerBL}, std::pair{BR, CornerBR}})
        {
            if (IsSolid)
            {
                RefX += static_cast<float>(Corner.X);
                RefY += static_cast<float>(Corner.Y);
                ++SolidCount;
            }
        }
        RefX /= static_cast<float>(SolidCount);
        RefY /= static_cast<float>(SolidCount);

        int Count = 0;
        auto Add = [&](EdgePoint A, EdgePoint B, const float SolidX, const float SolidY)
        {
            const float RightX = static_cast<float>(B.Y - A.Y);
            const float RightY = static_cast<float>(A.X - B.X);
            const float MidX = static_cast<float>(A.X + B.X) * 0.5f;
            const float MidY = static_cast<float>(A.Y + B.Y) * 0.5f;
            if ((SolidX - MidX) * RightX + (SolidY - MidY) * RightY > 0.f)
            {
                std::swap(A, B);
            }
            Out[Count++] = {.A = A, .B = B};
        };

        auto AddCut = [&](const EdgePoint A, const EdgePoint B, const EdgePoint SolidCorner)
        {
            Add(A, B, static_cast<float>(SolidCorner.X), static_cast<float>(SolidCorner.Y));
        };

        switch (Case)
        {
            case 1: case 14: Add(Left, Bottom, RefX, RefY); break;
            case 2: case 13: Add(Bottom, Right, RefX, RefY); break;
            case 3: case 12: Add(Left, Right, RefX, RefY); break;
            case 4: case 11: Add(Top, Right, RefX, RefY); break;
            case 6: case 9:  Add(Top, Bottom, RefX, RefY); break;
            case 7: case 8:  Add(Left, Top, RefX, RefY); break;
            // Saddles keep diagonal pixels separate, matching the 4-connected flood fill used for debris
            case 5:
                AddCut(Left, Bottom, CornerBL);
                AddCut(Top, Right, CornerTR);
                break;
            case 10:
                AddCut(Left, Top, CornerTL);
                AddCut(Bottom, Right, CornerBR);
                break;
            default: break;
        }
        return Count;
    }

    // The two cells sharing the edge a point sits on
    void GetEdgeCells(const EdgePoint P, int OutCells[2][2])
    {
        if ((P.X & 1) == 0) // horizontal edge: bottom of one cell, top of the next
        {
            OutCells[0][0] = P.X / 2;
            OutCells[0][1] = (P.Y - 1) / 2;
            OutCells[1][0] = P.X / 2;
            OutCells[1][1] = (P.Y + 1) / 2;
        }
        else // vertical edge: right of one cell, left of the next
        {
            OutCells[0][0] = (P.X - 1) / 2;
            OutCells[0][1] = P.Y / 2;
            OutCells[1][0] = (P.X + 1) / 2;
            OutCells[1][1] = P.Y / 2;
        }
    }

    // Ghost points let a chain that ends at a chunk border collide smoothly with the chain continuing in the neighbor chunk
    Vector2 FindGhost(const TerrainMap& Map, const EdgePoint P, const bool Before, const Vector2 Fallback)
    {
        int Cells[2][2];
        GetEdgeCells(P, Cells);
        for (const auto& Cell : Cells)
        {
            Segment Segments[2];
            const int Count = EmitCellSegments(Map, Cell[0], Cell[1], Segments);
            for (int i = 0; i < Count; ++i)
            {
                if (Before && SamePoint(Segments[i].B, P))
                {
                    return ToWorld(Segments[i].A);
                }
                if (!Before && SamePoint(Segments[i].A, P))
                {
                    return ToWorld(Segments[i].B);
                }
            }
        }
        return Fallback;
    }

    float PointSegmentDistance(const Vector2 P, const Vector2 A, const Vector2 B)
    {
        const float Dx = B.x - A.x;
        const float Dy = B.y - A.y;
        const float LengthSq = Dx * Dx + Dy * Dy;
        const float T = LengthSq > 0.f ? std::clamp(((P.x - A.x) * Dx + (P.y - A.y) * Dy) / LengthSq, 0.f, 1.f) : 0.f;
        const float Px = A.x + Dx * T - P.x;
        const float Py = A.y + Dy * T - P.y;
        return std::sqrt(Px * Px + Py * Py);
    }

    // Douglas-Peucker, keeps both endpoints so neighbor chunks still meet exactly
    std::vector<Vector2> Simplify(const std::vector<Vector2>& Points, const float Tolerance)
    {
        const int Count = static_cast<int>(Points.size());
        if (Count <= 2)
        {
            return Points;
        }

        std::vector<std::uint8_t> Keep(Points.size(), 0);
        Keep.front() = 1;
        Keep.back() = 1;

        std::vector<std::pair<int, int>> Stack = {{0, Count - 1}};
        while (!Stack.empty())
        {
            const auto [First, Last] = Stack.back();
            Stack.pop_back();

            float MaxDistance = 0.f;
            int MaxIndex = -1;
            for (int i = First + 1; i < Last; ++i)
            {
                const float Distance = PointSegmentDistance(Points[static_cast<size_t>(i)], Points[static_cast<size_t>(First)], Points[static_cast<size_t>(Last)]);
                if (Distance > MaxDistance)
                {
                    MaxDistance = Distance;
                    MaxIndex = i;
                }
            }

            if (MaxIndex != -1 && MaxDistance > Tolerance)
            {
                Keep[static_cast<size_t>(MaxIndex)] = 1;
                Stack.emplace_back(First, MaxIndex);
                Stack.emplace_back(MaxIndex, Last);
            }
        }

        std::vector<Vector2> Result;
        for (size_t i = 0; i < Points.size(); ++i)
        {
            if (Keep[i])
            {
                Result.push_back(Points[i]);
            }
        }
        return Result;
    }

    constexpr float SimplifyTolerancePx = 0.8f;

    void CreateChain(const b2BodyId Body, const std::vector<Vector2>& PointsPx, const bool IsLoop, const Vector2 Ghost1, const Vector2 Ghost2)
    {
        std::vector<b2Vec2> Points;
        Points.reserve(PointsPx.size());
        for (const Vector2& P : PointsPx)
        {
            const b2Vec2 M = ToMeters(P);
            // Box2D rejects segments shorter than its linear slop
            if (!Points.empty() && b2Distance(Points.back(), M) < 0.02f)
            {
                continue;
            }
            Points.push_back(M);
        }

        if (IsLoop && Points.size() > 1 && b2Distance(Points.front(), Points.back()) < 0.02f)
        {
            Points.pop_back();
        }

        if (Points.size() < (IsLoop ? 3u : 2u))
        {
            return;
        }

        b2SurfaceMaterial Material = b2DefaultSurfaceMaterial();
        Material.friction = 0.9f;

        b2ChainDef Def = b2DefaultChainDef();
        Def.points = Points.data();
        Def.pointCount = static_cast<int>(Points.size());
        Def.isLoop = IsLoop;
        Def.ghost1 = ToMeters(Ghost1);
        Def.ghost2 = ToMeters(Ghost2);
        Def.materials = &Material;
        Def.materialCount = 1;
        Def.filter.categoryBits = Category::Terrain;
        Def.filter.maskBits = ~Category::Terrain;
        b2CreateChain(Body, &Def);
    }

    void RebuildChunk(TerrainMap& Map, const b2WorldId World, const int ChunkX, const int ChunkY)
    {
        b2BodyId& Body = Map.ChunkBodies[static_cast<size_t>(ChunkY * Map.ChunksX + ChunkX)];
        if (B2_IS_NON_NULL(Body))
        {
            b2DestroyBody(Body);
            Body = b2_nullBodyId;
        }

        const int Cx0 = ChunkX * TerrainMap::ChunkSize;
        const int Cy0 = ChunkY * TerrainMap::ChunkSize;
        const int Cx1 = std::min(Cx0 + TerrainMap::ChunkSize, Map.Width + 1);
        const int Cy1 = std::min(Cy0 + TerrainMap::ChunkSize, Map.Height + 1);

        std::vector<Segment> Segments;
        for (int Cy = Cy0; Cy < Cy1; ++Cy)
        {
            for (int Cx = Cx0; Cx < Cx1; ++Cx)
            {
                Segment Cell[2];
                const int Count = EmitCellSegments(Map, Cx, Cy, Cell);
                for (int i = 0; i < Count; ++i)
                {
                    Segments.push_back(Cell[i]);
                }
            }
        }

        if (Segments.empty())
        {
            return;
        }

        std::unordered_map<std::uint64_t, int> StartIndex;
        StartIndex.reserve(Segments.size());
        for (int i = 0; i < static_cast<int>(Segments.size()); ++i)
        {
            StartIndex[PackKey(Segments[static_cast<size_t>(i)].A)] = i;
        }

        std::vector<std::uint8_t> HasPredecessor(Segments.size(), 0);
        for (const Segment& S : Segments)
        {
            if (const auto It = StartIndex.find(PackKey(S.B)); It != StartIndex.end())
            {
                HasPredecessor[static_cast<size_t>(It->second)] = 1;
            }
        }

        b2BodyDef BodyDef = b2DefaultBodyDef();
        BodyDef.type = b2_staticBody;
        Body = b2CreateBody(World, &BodyDef);

        std::vector<std::uint8_t> Used(Segments.size(), 0);
        std::vector<EdgePoint> Path;

        auto Walk = [&](const int Start)
        {
            Path.clear();
            Path.push_back(Segments[static_cast<size_t>(Start)].A);
            int Current = Start;
            while (true)
            {
                Used[static_cast<size_t>(Current)] = 1;
                Path.push_back(Segments[static_cast<size_t>(Current)].B);
                const auto It = StartIndex.find(PackKey(Segments[static_cast<size_t>(Current)].B));
                if (It == StartIndex.end() || Used[static_cast<size_t>(It->second)])
                {
                    break;
                }
                Current = It->second;
            }
        };

        auto ToWorldPath = [&]()
        {
            std::vector<Vector2> Points;
            Points.reserve(Path.size());
            for (const EdgePoint& P : Path)
            {
                Points.push_back(ToWorld(P));
            }
            return Points;
        };

        // Open polylines enter and leave through the chunk border
        for (size_t i = 0; i < Segments.size(); ++i)
        {
            if (HasPredecessor[i] || Used[i])
            {
                continue;
            }

            Walk(static_cast<int>(i));
            const std::vector<Vector2> Points = Simplify(ToWorldPath(), SimplifyTolerancePx);
            const Vector2 Ghost1 = FindGhost(Map, Path.front(), true, Vector2Subtract(Points[0], Vector2Subtract(Points[1], Points[0])));
            const Vector2 Ghost2 = FindGhost(Map, Path.back(), false, Vector2Add(Points.back(), Vector2Subtract(Points.back(), Points[Points.size() - 2])));
            CreateChain(Body, Points, false, Ghost1, Ghost2);
        }

        // Whatever is left are closed loops fully inside the chunk
        for (size_t i = 0; i < Segments.size(); ++i)
        {
            if (Used[i])
            {
                continue;
            }

            Walk(static_cast<int>(i));
            std::vector<Vector2> Points = ToWorldPath();
            Points.pop_back(); // the walk ends where it started

            if (Points.size() < 3)
            {
                continue;
            }

            // Split at the farthest point so Douglas-Peucker has two real ends to anchor
            size_t Far = 0;
            float FarDistance = 0.f;
            for (size_t p = 1; p < Points.size(); ++p)
            {
                const float Distance = Vector2DistanceSqr(Points[0], Points[p]);
                if (Distance > FarDistance)
                {
                    FarDistance = Distance;
                    Far = p;
                }
            }

            std::vector<Vector2> FirstHalf(Points.begin(), Points.begin() + static_cast<std::ptrdiff_t>(Far) + 1);
            std::vector<Vector2> SecondHalf(Points.begin() + static_cast<std::ptrdiff_t>(Far), Points.end());
            SecondHalf.push_back(Points[0]);

            std::vector<Vector2> Loop = Simplify(FirstHalf, SimplifyTolerancePx);
            const std::vector<Vector2> Second = Simplify(SecondHalf, SimplifyTolerancePx);
            Loop.insert(Loop.end(), Second.begin() + 1, Second.end() - 1);
            CreateChain(Body, Loop, true, {}, {});
        }
    }
}

void InitTerrain(TerrainMap& Map, const int Width, const int Height)
{
    Map.Width = Width;
    Map.Height = Height;
    const size_t Count = static_cast<size_t>(Width) * static_cast<size_t>(Height);
    Map.Solid.assign(Count, 0);
    Map.Pixels.assign(Count, BLANK);
    Map.VisitMark.assign(Count, 0);
    Map.VisitGeneration = 0;

    Map.ChunksX = (Width + 1 + TerrainMap::ChunkSize - 1) / TerrainMap::ChunkSize;
    Map.ChunksY = (Height + 1 + TerrainMap::ChunkSize - 1) / TerrainMap::ChunkSize;
    Map.ChunkBodies.assign(static_cast<size_t>(Map.ChunksX * Map.ChunksY), b2_nullBodyId);
    Map.ChunkDirty.assign(static_cast<size_t>(Map.ChunksX * Map.ChunksY), 1);

    Map.DirtyRowMin = 0;
    Map.DirtyRowMax = Height - 1;
}

void UnloadTerrain(TerrainMap& Map)
{
    if (Map.Texture.id != 0)
    {
        UnloadTexture(Map.Texture);
        Map.Texture = {};
    }

    // Chunk bodies are owned by the physics world and die with it
    Map.ChunkBodies.clear();
    Map.ChunkDirty.clear();
    Map.Solid.clear();
    Map.Pixels.clear();
    Map.VisitMark.clear();
}

bool IsCircleFree(const TerrainMap& Map, const Vector2 Center, const float Radius)
{
    const int X0 = static_cast<int>(std::floor(Center.x - Radius));
    const int X1 = static_cast<int>(std::ceil(Center.x + Radius));
    const int Y0 = static_cast<int>(std::floor(Center.y - Radius));
    const int Y1 = static_cast<int>(std::ceil(Center.y + Radius));
    const float RadiusSq = Radius * Radius;

    for (int Y = Y0; Y <= Y1; ++Y)
    {
        for (int X = X0; X <= X1; ++X)
        {
            const float Dx = static_cast<float>(X) + 0.5f - Center.x;
            const float Dy = static_cast<float>(Y) + 0.5f - Center.y;
            if (Dx * Dx + Dy * Dy <= RadiusSq && IsSolidAt(Map, X, Y))
            {
                return false;
            }
        }
    }
    return true;
}

void MarkTerrainDirty(TerrainMap& Map, int X0, int Y0, int X1, int Y1)
{
    X0 = std::clamp(X0, 0, Map.Width - 1);
    X1 = std::clamp(X1, 0, Map.Width - 1);
    Y0 = std::clamp(Y0, 0, Map.Height - 1);
    Y1 = std::clamp(Y1, 0, Map.Height - 1);
    if (X0 > X1 || Y0 > Y1)
    {
        return;
    }

    if (Map.DirtyRowMax < Map.DirtyRowMin)
    {
        Map.DirtyRowMin = Y0;
        Map.DirtyRowMax = Y1;
    }
    else
    {
        Map.DirtyRowMin = std::min(Map.DirtyRowMin, Y0);
        Map.DirtyRowMax = std::max(Map.DirtyRowMax, Y1);
    }

    // Pixel X feeds cells X and X + 1
    const int ChunkX0 = X0 / TerrainMap::ChunkSize;
    const int ChunkX1 = std::min((X1 + 1) / TerrainMap::ChunkSize, Map.ChunksX - 1);
    const int ChunkY0 = Y0 / TerrainMap::ChunkSize;
    const int ChunkY1 = std::min((Y1 + 1) / TerrainMap::ChunkSize, Map.ChunksY - 1);
    for (int Cy = ChunkY0; Cy <= ChunkY1; ++Cy)
    {
        for (int Cx = ChunkX0; Cx <= ChunkX1; ++Cx)
        {
            Map.ChunkDirty[static_cast<size_t>(Cy * Map.ChunksX + Cx)] = 1;
        }
    }
}

void SetTerrainPixel(TerrainMap& Map, const int X, const int Y, const Color C, const bool IsSolid)
{
    if (X < 0 || Y < 0 || X >= Map.Width || Y >= Map.Height)
    {
        return;
    }
    const size_t Index = static_cast<size_t>(Y * Map.Width + X);
    Map.Solid[Index] = IsSolid ? 1 : 0;
    Map.Pixels[Index] = C;
}

Color CarveTerrain(TerrainMap& Map, const Vector2 Center, const float Radius)
{
    constexpr float ScorchWidth = 5.f;
    constexpr Color ScorchColor = {.r = 34, .g = 24, .b = 18, .a = 255};

    const float Outer = Radius + ScorchWidth;
    const int X0 = std::max(0, static_cast<int>(std::floor(Center.x - Outer)));
    const int X1 = std::min(Map.Width - 1, static_cast<int>(std::ceil(Center.x + Outer)));
    const int Y0 = std::max(0, static_cast<int>(std::floor(Center.y - Outer)));
    const int Y1 = std::min(Map.Height - 1, static_cast<int>(std::ceil(Center.y + Outer)));

    Color Sample = BLANK;
    for (int Y = Y0; Y <= Y1; ++Y)
    {
        for (int X = X0; X <= X1; ++X)
        {
            const size_t Index = static_cast<size_t>(Y * Map.Width + X);
            if (Map.Pixels[Index].a == 0)
            {
                continue;
            }

            const float Distance = Vector2Distance({.x = static_cast<float>(X) + 0.5f, .y = static_cast<float>(Y) + 0.5f}, Center);
            if (Distance <= Radius)
            {
                if (Map.Solid[Index] && Sample.a == 0)
                {
                    Sample = Map.Pixels[Index];
                }
                Map.Solid[Index] = 0;
                Map.Pixels[Index] = BLANK;
            }
            else if (Distance <= Outer)
            {
                const float Burn = 0.6f * (1.f - (Distance - Radius) / ScorchWidth);
                Map.Pixels[Index] = ColorLerp(Map.Pixels[Index], ScorchColor, Burn);
            }
        }
    }

    MarkTerrainDirty(Map, X0, Y0, X1, Y1);
    return Sample;
}

std::vector<TerrainPiece> DetachFloatingPieces(TerrainMap& Map, const Vector2 Center, const float Radius, const int MaxPiecePixels)
{
    std::vector<TerrainPiece> Pieces;

    // Every fill gets its own mark. Freed pieces leave no solid pixels behind, so any marked pixel from an earlier fill in
    // this call belongs to an anchored blob, and touching it means this fill is anchored too. Without this, every rim seed
    // on the mainland would re-walk MaxPiecePixels before giving up
    const std::uint32_t CallStart = Map.VisitGeneration + 1;

    // Anything that lost its connection must touch the crater rim, so seeding from the rim is enough
    const float SeedRadius = Radius + 1.5f;
    const int SeedCount = std::max(16, static_cast<int>(2.f * PI * SeedRadius / 1.5f));

    std::vector<int> Stack;
    std::vector<int> Filled;

    for (int s = 0; s < SeedCount; ++s)
    {
        const float Angle = 2.f * PI * static_cast<float>(s) / static_cast<float>(SeedCount);
        const int SeedX = static_cast<int>(std::floor(Center.x + std::cos(Angle) * SeedRadius));
        const int SeedY = static_cast<int>(std::floor(Center.y + std::sin(Angle) * SeedRadius));
        if (!IsSolidAt(Map, SeedX, SeedY))
        {
            continue;
        }

        const int SeedIndex = SeedY * Map.Width + SeedX;
        if (Map.VisitMark[static_cast<size_t>(SeedIndex)] >= CallStart)
        {
            continue;
        }

        const std::uint32_t Generation = ++Map.VisitGeneration;

        Stack.clear();
        Filled.clear();
        Stack.push_back(SeedIndex);
        Map.VisitMark[static_cast<size_t>(SeedIndex)] = Generation;
        bool Anchored = false;

        while (!Stack.empty())
        {
            const int Index = Stack.back();
            Stack.pop_back();
            Filled.push_back(Index);

            const int X = Index % Map.Width;
            const int Y = Index / Map.Width;

            // Too big, or held by the map border: stays put. Visited pixels stay marked so other seeds skip this blob
            if (static_cast<int>(Filled.size()) > MaxPiecePixels || X == 0 || X == Map.Width - 1 || Y == Map.Height - 1)
            {
                Anchored = true;
                break;
            }

            const int Neighbors[4][2] = {{X - 1, Y}, {X + 1, Y}, {X, Y - 1}, {X, Y + 1}};
            for (const auto& N : Neighbors)
            {
                if (!IsSolidAt(Map, N[0], N[1]))
                {
                    continue;
                }
                const int NeighborIndex = N[1] * Map.Width + N[0];
                const std::uint32_t Mark = Map.VisitMark[static_cast<size_t>(NeighborIndex)];
                if (Mark >= CallStart && Mark != Generation)
                {
                    Anchored = true;
                    break;
                }
                if (Mark != Generation)
                {
                    Map.VisitMark[static_cast<size_t>(NeighborIndex)] = Generation;
                    Stack.push_back(NeighborIndex);
                }
            }

            if (Anchored)
            {
                break;
            }
        }

        if (Anchored)
        {
            continue;
        }

        // Grass blades sitting on the piece go with it
        constexpr int DecorReach = 4;

        int MinX = Map.Width;
        int MinY = Map.Height;
        int MaxX = -1;
        int MaxY = -1;
        for (const int Index : Filled)
        {
            const int X = Index % Map.Width;
            const int Y = Index / Map.Width;
            MinX = std::min(MinX, X);
            MaxX = std::max(MaxX, X);
            MinY = std::min(MinY, Y);
            MaxY = std::max(MaxY, Y);
        }
        MinY = std::max(0, MinY - DecorReach);

        TerrainPiece Piece;
        Piece.X = MinX;
        Piece.Y = MinY;
        Piece.Width = MaxX - MinX + 1;
        Piece.Height = MaxY - MinY + 1;
        Piece.SolidCount = static_cast<int>(Filled.size());
        Piece.Solid.assign(static_cast<size_t>(Piece.Width * Piece.Height), 0);
        Piece.Pixels.assign(static_cast<size_t>(Piece.Width * Piece.Height), BLANK);

        for (const int Index : Filled)
        {
            const int X = Index % Map.Width;
            const int Y = Index / Map.Width;
            const size_t Local = static_cast<size_t>((Y - MinY) * Piece.Width + (X - MinX));
            Piece.Solid[Local] = 1;
            Piece.Pixels[Local] = Map.Pixels[static_cast<size_t>(Index)];

            for (int Up = 1; Up <= DecorReach && Y - Up >= MinY; ++Up)
            {
                const size_t Above = static_cast<size_t>((Y - Up) * Map.Width + X);
                if (Map.Solid[Above] || Map.Pixels[Above].a == 0)
                {
                    break;
                }
                Piece.Pixels[static_cast<size_t>((Y - Up - MinY) * Piece.Width + (X - MinX))] = Map.Pixels[Above];
                Map.Pixels[Above] = BLANK;
            }

            Map.Solid[static_cast<size_t>(Index)] = 0;
            Map.Pixels[static_cast<size_t>(Index)] = BLANK;
        }

        MarkTerrainDirty(Map, MinX, MinY, MaxX, MaxY);
        Pieces.push_back(std::move(Piece));
    }

    return Pieces;
}

void SyncTerrain(TerrainMap& Map, const b2WorldId World)
{
    if (Map.Texture.id == 0)
    {
        const Image Source =
        {
            .data = Map.Pixels.data(),
            .width = Map.Width,
            .height = Map.Height,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
        };
        Map.Texture = LoadTextureFromImage(Source);
        SetTextureFilter(Map.Texture, TEXTURE_FILTER_BILINEAR);
        // Default REPEAT wrap makes bilinear filtering bleed the bottom row into the top edge as a visible line
        SetTextureWrap(Map.Texture, TEXTURE_WRAP_CLAMP);
        Map.DirtyRowMax = -1;
    }
    else if (Map.DirtyRowMax >= Map.DirtyRowMin)
    {
        // Whole rows are contiguous in memory, so no staging copy is needed
        const Rectangle Rows = {.x = 0.f, .y = static_cast<float>(Map.DirtyRowMin), .width = static_cast<float>(Map.Width), .height = static_cast<float>(Map.DirtyRowMax - Map.DirtyRowMin + 1)};
        UpdateTextureRec(Map.Texture, Rows, &Map.Pixels[static_cast<size_t>(Map.DirtyRowMin * Map.Width)]);
        Map.DirtyRowMax = -1;
    }

    for (int Cy = 0; Cy < Map.ChunksY; ++Cy)
    {
        for (int Cx = 0; Cx < Map.ChunksX; ++Cx)
        {
            std::uint8_t& Dirty = Map.ChunkDirty[static_cast<size_t>(Cy * Map.ChunksX + Cx)];
            if (Dirty)
            {
                RebuildChunk(Map, World, Cx, Cy);
                Dirty = 0;
            }
        }
    }
}
