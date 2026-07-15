#include "Game.h"
#include "raylib/raymath.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <climits>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

void SpawnParticles(std::vector<Particle>& Particles, const Vector2& Position, const Color& Tint, const int Count)
{
    for (int i = 0; i < Count; ++i)
    {
        const float Angle = GetRandomFloatValue(0.f, 360.f) * DEG2RAD;
        const float Speed = GetRandomFloatValue(60.f, 280.f);
        const float Lifetime = GetRandomFloatValue(0.25f, 0.75f);
        const float Radius = GetRandomFloatValue(2.f, 8.f);

        Particles.push_back({
            .Position = Position,
            .Velocity = {.x = std::cosf(Angle) * Speed, .y = std::sinf(Angle) * Speed},
            .Tint = Tint,
            .InitialRadius = Radius,
            .Lifetime = Lifetime,
            .MaxLifetime = Lifetime
        });
    }
}

void UpdateParticles(std::vector<Particle>& Particles, const float DeltaTime)
{
    for (Particle& Particle : Particles)
    {
        Particle.Position.x += Particle.Velocity.x * DeltaTime;
        Particle.Position.y += Particle.Velocity.y * DeltaTime;
        Particle.Lifetime -= DeltaTime;
    }

    std::erase_if(Particles, [](const Particle& P)
    {
        return P.Lifetime <= 0.f;
    });
}

void SpawnFloatingText(std::vector<FloatingText>& Texts, const Vector2 Position, const char* Text, const Color Tint, const int FontSize, const float Lifetime, const bool Shake)
{
    FloatingText T = {.Position = Position, .Text = {}, .Tint = Tint, .FontSize = FontSize, .Lifetime = Lifetime, .MaxLifetime = Lifetime, .Shake = Shake};

    const int WrittenBytes = snprintf(T.Text, sizeof(T.Text), "%s", Text);

    // Ref: https://www.sandordargo.com/blog/2023/10/11/cpp20-intcmp-utilities
    if (WrittenBytes < 0 || std::cmp_greater_equal(WrittenBytes, sizeof(T.Text)))
    {
        spdlog::warn("SpawnFloatingText: text '{}' truncated to {} chars", Text, sizeof(T.Text) - 1);
    }

    Texts.push_back(T);
}

void UpdateFloatingTexts(std::vector<FloatingText>& Texts, const float DeltaTime)
{
    for (FloatingText& T : Texts)
    {
        T.Position.y -= 40.f * DeltaTime;
        T.Lifetime -= DeltaTime;
    }

    std::erase_if(Texts, [](const FloatingText& T)
    {
        return T.Lifetime <= 0.f;
    });
}

void InitBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const int Count, const float Width, const float Height)
{
    BackgroundParticles.clear();
    BackgroundParticles.reserve(Count);

    for (int i = 0; i < Count; ++i)
    {
        // Fake parallax
        // Depth 0 = far (small, slow, dim)
        // Depth 1 = near (big, fast, bright)
        const float Depth = GetRandomFloatValue(0.f, 1.f);

        BackgroundParticles.push_back({
            .Position = {.x = GetRandomFloatValue(0.f, Width), .y = GetRandomFloatValue(0.f, Height)},
            .Speed = Lerp(20.f, 80.f, Depth),
            .Radius = Lerp(1.f, 3.f, Depth),
            .Alpha = Lerp(40.f, 140.f, Depth)
        });
    }
}

void UpdateBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const float DeltaTime, const float Width, const float Height)
{
    for (BackgroundParticle& Particle : BackgroundParticles)
    {
        Particle.Position.y += Particle.Speed * DeltaTime;

        // Wrap to top once fully below the field, re-roll x for variety
        if (Particle.Position.y - Particle.Radius > Height)
        {
            Particle.Position.y = -Particle.Radius;
            Particle.Position.x = GetRandomFloatValue(0.f, Width);
        }
    }
}

void LoadLevelFromPath(SimState& Sim, const char* Path, const float PlayfieldWidth)
{
    std::ifstream File(Path);
    if (!File.is_open())
    {
        spdlog::error("LoadLevelFromPath: failed to open {}", Path);
        return;
    }

    std::vector<std::string> Lines;
    std::string Line;
    while (std::getline(File, Line))
    {
        Lines.push_back(Line);
    }

    if (Lines.empty())
    {
        spdlog::error("LoadLevelFromPath: file {} is empty", Path);
        return;
    }

    size_t MaxCols = 0;
    for (const std::string& L : Lines)
    {
        MaxCols = std::max(MaxCols, L.size());
    }

    constexpr float WallMarginPx = 4.f;
    const float BlockWidth = (PlayfieldWidth - WallMarginPx * 2.f) / static_cast<float>(MaxCols);
    const float BlockHeight = BlockWidth * 0.4f;

    Sim.Terrain.clear();
    Sim.Terrain.reserve(Lines.size() * MaxCols);

    for (size_t Row = 0; Row < Lines.size(); ++Row)
    {
        for (size_t Col = 0; Col < Lines[Row].size(); ++Col)
        {
            const char Token = Lines[Row][Col];
            if (Token == ' ')
            {
                continue;
            }

            int Hits;
            Color BlockColor;

            switch (Token)
            {
                case 'Y': Hits = 1;       BlockColor = YELLOW; break;
                case 'G': Hits = 1;       BlockColor = GREEN;  break;
                case 'O': Hits = 1;       BlockColor = ORANGE; break;
                case 'R': Hits = 2;       BlockColor = RED;    break;
                case 'W': Hits = INT_MAX; BlockColor = {.r = 80, .g = 80, .b = 80, .a = 255}; break;
                default: continue;
            }

            Sim.Terrain.push_back({
                .Position = {.x = WallMarginPx + static_cast<float>(Col) * BlockWidth, .y = WallMarginPx + static_cast<float>(Row) * BlockHeight},
                .Width = BlockWidth,
                .Height = BlockHeight,
                .HitsLeft = Hits,
                .Alive = true,
                .DeathSec = 0.f,
                .Tint = BlockColor
            });
        }
    }

    spdlog::info("LoadLevelFromPath: {} terrain blocks loaded", Sim.Terrain.size());
}

namespace
{
    constexpr float WormGravity = 1400.f;
    constexpr float WormWalkSpeed = 220.f;
    constexpr float WormHopVelocity = -420.f;

    // Pushes the Worm out of a single overlapping Rect. Unlike TryResolveBallRectCollision,
    // this stops the Worm's velocity on the resolved axis instead of reflecting it (no bounce).
    void ResolveWormRect(Worm& W, const Rectangle& Rect)
    {
        const Rectangle WormRect =
        {
            .x = W.Position.x - Worm::Radius,
            .y = W.Position.y - Worm::Radius,
            .width = Worm::Radius * 2.f,
            .height = Worm::Radius * 2.f
        };

        if (!CheckCollisionRecs(WormRect, Rect))
        {
            return;
        }

        const float CenterX = Rect.x + Rect.width * 0.5f;
        const float CenterY = Rect.y + Rect.height * 0.5f;

        const float OverlapX = (Worm::Radius + Rect.width * 0.5f) - std::abs(W.Position.x - CenterX);
        const float OverlapY = (Worm::Radius + Rect.height * 0.5f) - std::abs(W.Position.y - CenterY);

        if (OverlapX < OverlapY)
        {
            if (W.Position.x < CenterX)
            {
                W.Position.x = Rect.x - Worm::Radius;
            }
            else
            {
                W.Position.x = Rect.x + Rect.width + Worm::Radius;
            }
            W.Velocity.x = 0.f;
        }
        else
        {
            if (W.Position.y < CenterY) // landed on top of the block
            {
                W.Position.y = Rect.y - Worm::Radius;
                W.Velocity.y = 0.f;
                W.OnGround = true;
            }
            else // hit the block from below
            {
                W.Position.y = Rect.y + Rect.height + Worm::Radius;
                W.Velocity.y = 0.f;
            }
        }
    }
}

namespace
{
    constexpr float ShotMinSpeed = 300.f;
    constexpr float ShotMaxSpeed = 1100.f;
    constexpr float ChargeRatePerSec = 1.f; // full charge in 1 second
}

bool UpdateWormAim(Worm& W, const Vector2 MouseVirtualPos, const float DeltaTime, Vector2& OutFireVelocity)
{
    const Vector2 ToMouse = {.x = MouseVirtualPos.x - W.Position.x, .y = MouseVirtualPos.y - W.Position.y};
    W.AimAngle = std::atan2f(ToMouse.y, ToMouse.x);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        W.Charging = true;
        W.ChargePower = std::min(1.f, W.ChargePower + ChargeRatePerSec * DeltaTime);
        return false;
    }

    if (W.Charging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        const float Speed = Lerp(ShotMinSpeed, ShotMaxSpeed, W.ChargePower);
        OutFireVelocity = {.x = std::cosf(W.AimAngle) * Speed, .y = std::sinf(W.AimAngle) * Speed};
        W.Charging = false;
        W.ChargePower = 0.f;
        return true;
    }

    return false;
}

std::vector<Vector2> SimulateTrajectory(const Vector2 Origin, const Vector2 Velocity, const float TimeStep, const int MaxPoints)
{
    std::vector<Vector2> Points;
    Points.reserve(MaxPoints);

    Vector2 Position = Origin;
    Vector2 Vel = Velocity;

    for (int i = 0; i < MaxPoints; ++i)
    {
        Vel.y += WormGravity * TimeStep;
        Position.x += Vel.x * TimeStep;
        Position.y += Vel.y * TimeStep;
        Points.push_back(Position);
    }

    return Points;
}

void UpdateWorm(Worm& W, const std::vector<TerrainBlock>& Terrain, const float DeltaTime, const float PlayfieldWidth, const float PlayfieldHeight, const bool AcceptInput)
{
    if (!W.Alive)
    {
        return;
    }

    if (AcceptInput)
    {
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        {
            W.Position.x -= WormWalkSpeed * DeltaTime;
            W.FacingDir = -1.f;
        }
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        {
            W.Position.x += WormWalkSpeed * DeltaTime;
            W.FacingDir = 1.f;
        }
        if (IsKeyPressed(KEY_SPACE) && W.OnGround)
        {
            W.Velocity.y = WormHopVelocity;
            W.OnGround = false;
        }
    }

    W.Velocity.y += WormGravity * DeltaTime;
    W.Position.y += W.Velocity.y * DeltaTime;

    W.OnGround = false;
    for (const TerrainBlock& Block : Terrain)
    {
        if (!Block.Alive)
        {
            continue;
        }

        const Rectangle BlockRect = {.x = Block.Position.x, .y = Block.Position.y, .width = Block.Width, .height = Block.Height};
        ResolveWormRect(W, BlockRect);
    }

    W.Position.x = std::clamp(W.Position.x, Worm::Radius, PlayfieldWidth - Worm::Radius);

    if (W.FlashSec > 0.f)
    {
        W.FlashSec -= DeltaTime;
    }

    // PlayfieldHeight is intentionally not clamped here - falling below it is death, checked separately
    (void)PlayfieldHeight;
}
