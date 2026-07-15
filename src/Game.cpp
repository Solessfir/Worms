#include "Game.h"
#include "raylib/raymath.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <climits>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

void UpdateBall(SimState& Sim, const float DeltaTime)
{
    for (Ball& Ball : Sim.Balls)
    {
        if (Ball.Caught)
        {
            continue;
        }

        // Hit stop: freeze this Ball briefly after it kills a Brick
        if (Ball.HitStopSec > 0.f)
        {
            Ball.HitStopSec -= DeltaTime;
            continue;
        }

        Ball.Position.x += Ball.Velocity.x * DeltaTime;
        Ball.Position.y += Ball.Velocity.y * DeltaTime;
    }
}

void UpdateBallSpeed(SimState& Sim, const float BaseSpeed)
{
    const int Tier1 = Sim.TotalInteractiveBricks / 4;     // 25% cleared
    const int Tier2 = Sim.TotalInteractiveBricks / 2;     // 50% cleared
    const int Tier3 = Sim.TotalInteractiveBricks * 3 / 4; // 75% cleared

    float TargetSpeed = BaseSpeed;

    if      (Sim.BricksDestroyed >= Tier3) TargetSpeed *= 1.75f;
    else if (Sim.BricksDestroyed >= Tier2) TargetSpeed *= 1.5f;
    else if (Sim.BricksDestroyed >= Tier1) TargetSpeed *= 1.25f;

    TargetSpeed *= Sim.BallSpeedMultiplier; // SlowBall/FastBall Powerup

    for (Ball& Ball : Sim.Balls)
    {
        const float CurrentSpeed = std::sqrtf(Ball.Velocity.x * Ball.Velocity.x + Ball.Velocity.y * Ball.Velocity.y);
        if (CurrentSpeed > 0.f)
        {
            // Normalize and Scale velocity
            Ball.Velocity.x = (Ball.Velocity.x / CurrentSpeed) * TargetSpeed;
            Ball.Velocity.y = (Ball.Velocity.y / CurrentSpeed) * TargetSpeed;

            // Ball can soft-lock if angle near-horizontal (ping-pongs side to side forever)
            // Force minimum deviation angle
            constexpr float MinAngle = 20.f * DEG2RAD;
            const float MinVerticalSpeed = TargetSpeed * std::sinf(MinAngle);
            if (std::abs(Ball.Velocity.y) < MinVerticalSpeed)
            {
                const float SignY = Ball.Velocity.y < 0.f ? -1.f : 1.f;
                const float SignX = Ball.Velocity.x < 0.f ? -1.f : 1.f;
                Ball.Velocity.y = SignY * MinVerticalSpeed;
                Ball.Velocity.x = SignX * TargetSpeed * std::cosf(MinAngle);
            }
        }
    }
}

bool UpdateBallWallCollision(SimState& Sim, const float PlayfieldWidth, const float PlayfieldHeight, std::vector<Vector2>& OutDeadBalls)
{
    bool HitWall = false;
    OutDeadBalls.clear();

    for (int i = static_cast<int>(Sim.Balls.size()) - 1; i >= 0; --i)
    {
        Ball& Ball = Sim.Balls[i];

        if (Ball.Caught || Ball.HitStopSec > 0.f)
        {
            continue;
        }

        const bool HitLeft = Ball.Position.x - Ball.Radius <= 0.f;
        const bool HitRight = Ball.Position.x + Ball.Radius >= PlayfieldWidth;
        const bool HitTop = Ball.Position.y - Ball.Radius <= 0.f;
        const bool HitBottom = Ball.Position.y + Ball.Radius >= PlayfieldHeight;

        if (HitLeft)
        {
            Ball.Position.x = Ball.Radius;
            Ball.Velocity.x = std::abs(Ball.Velocity.x);
            HitWall = true;
        }
        else if (HitRight)
        {
            Ball.Position.x = PlayfieldWidth - Ball.Radius;
            Ball.Velocity.x = -std::abs(Ball.Velocity.x);
            HitWall = true;
        }

        if (HitTop)
        {
            Ball.Position.y = Ball.Radius;
            Ball.Velocity.y = std::abs(Ball.Velocity.y);
            HitWall = true;
        }
        else if (HitBottom)
        {
            if (Sim.ProtectionActive)
            {
                Ball.Position.y = PlayfieldHeight - Ball.Radius;
                Ball.Velocity.y = -std::abs(Ball.Velocity.y);
                Sim.ProtectionActive = false;
                HitWall = true;
            }
            else
            {
                OutDeadBalls.push_back(Ball.Position);
                Sim.Balls.erase(Sim.Balls.begin() + i);
            }
        }
    }

    if (Sim.Balls.empty() && Sim.BallLaunched)
    {
        --Sim.Lives;
        Sim.Combo = 0;
        Sim.BallLaunched = false;
    }

    return HitWall;
}

bool UpdateBallPadCollision(SimState& Sim)
{
    const Rectangle PaddleRect =
    {
        .x = Sim.Pad.Position.x,
        .y = Sim.Pad.Position.y,
        .width = Sim.Pad.Width,
        .height = Sim.Pad.Height
    };

    bool AnyHit = false;
    for (Ball& Ball : Sim.Balls)
    {
        if (Ball.Caught || Ball.HitStopSec > 0.f)
        {
            continue;
        }

        if (CheckCollisionCircleRec(Ball.Position, Ball.Radius, PaddleRect))
        {
            if (Sim.CatchActive)
            {
                Ball.Caught = true;
                Ball.CaughtOffsetX = Ball.Position.x - (Sim.Pad.Position.x + Sim.Pad.Width * 0.5f);
                Ball.Velocity = {};
                Ball.Position.y = Sim.Pad.Position.y - Ball.Radius;
                Sim.CatchActive = false; // consumed on first catch
            }
            else
            {
                if (std::abs(Sim.Pad.VelocityX) < 1.f)
                {
                    // Pad is not moving, but we have to break straight up/down Ball motion
                    const float PadCenterX = Sim.Pad.Position.x + Sim.Pad.Width * 0.5f;
                    float Offset = std::clamp((Ball.Position.x - PadCenterX) / (Sim.Pad.Width * 0.5f), -1.f, 1.f);

                    constexpr float DeadZone = 0.15f;
                    if (std::abs(Offset) < DeadZone)
                    {
                        const float Sign = GetRandomValue(0, 1) ? -1.f : 1.f;
                        Offset = Sign * DeadZone;
                    }

                    constexpr float MaxBounceAngle = 60.f * DEG2RAD;
                    const float Angle = Offset * MaxBounceAngle;
                    const float CurrentSpeed = std::sqrtf(Ball.Velocity.x * Ball.Velocity.x + Ball.Velocity.y * Ball.Velocity.y);

                    Ball.Velocity.x = CurrentSpeed * std::sinf(Angle);
                    Ball.Velocity.y = -CurrentSpeed * std::cosf(Angle);
                    Ball.Position.y = Sim.Pad.Position.y - Ball.Radius;
                }
                else
                {
                    Ball.Velocity.y = -std::abs(Ball.Velocity.y);
                    Ball.Position.y = Sim.Pad.Position.y - Ball.Radius;
                    constexpr float PaddleVelocityInfluence = 0.5f;
                    Ball.Velocity.x += Sim.Pad.VelocityX * PaddleVelocityInfluence;
                }
            }

            Sim.Pad.FlashSec = Paddle::FlashDuration;
            AnyHit = true;
        }
    }
    return AnyHit;
}


bool TryResolveBallRectCollision(Ball& Ball, const Rectangle& Rect)
{
    if (!CheckCollisionCircleRec(Ball.Position, Ball.Radius, Rect))
    {
        return false;
    }

    // We have to know which Rect face the Ball hit. Here we go AABB.
    // Ref: https://c.har.li/e/2024/03/28/implementing-robust-2D-collision-resolution.html

    const float CenterX = Rect.x + Rect.width * 0.5f;
    const float CenterY = Rect.y + Rect.height * 0.5f;

    const float OverlapX = (Ball.Radius + Rect.width * 0.5f) - std::abs(Ball.Position.x - CenterX);
    const float OverlapY = (Ball.Radius + Rect.height * 0.5f) - std::abs(Ball.Position.y - CenterY);

    if (OverlapX < OverlapY) // hit a vertical (left/right) face
    {
        if (Ball.Position.x < CenterX) // hit left face
        {
            Ball.Velocity.x = -std::abs(Ball.Velocity.x);
            Ball.Position.x = Rect.x - Ball.Radius;
        }
        else // hit right face
        {
            Ball.Velocity.x = std::abs(Ball.Velocity.x);
            Ball.Position.x = Rect.x + Rect.width + Ball.Radius;
        }
    }
    else // hit a horizontal (top/bottom) face
    {
        if (Ball.Position.y < CenterY) // hit top face
        {
            Ball.Velocity.y = -std::abs(Ball.Velocity.y);
            Ball.Position.y = Rect.y - Ball.Radius;
        }
        else // hit bottom face
        {
            Ball.Velocity.y = std::abs(Ball.Velocity.y);
            Ball.Position.y = Rect.y + Rect.height + Ball.Radius;
        }
    }

    return true;
}

void UpdateBallBrickCollision(SimState& Sim, std::vector<int>& OutHitBricks)
{
    OutHitBricks.clear();

    for (Ball& Ball : Sim.Balls)
    {
        if (Ball.Caught || Ball.HitStopSec > 0.f)
        {
            continue;
        }

        for (int i = 0; i < static_cast<int>(Sim.Bricks.size()); ++i)
        {
            Brick& Brick = Sim.Bricks[i];
            if (!Brick.Alive)
            {
                continue;
            }

            const Rectangle BrickRect =
            {
                .x = Brick.Position.x,
                .y = Brick.Position.y,
                .width = Brick.Width,
                .height = Brick.Height
            };

            bool Dead = false;

            // Powerup Through
            if (Sim.ThroughActive)
            {
                if (!CheckCollisionCircleRec(Ball.Position, Ball.Radius, BrickRect))
                {
                    continue;
                }

                // Brick Walls (INT_MAX) don't count toward level clear
                if (Brick.HitsLeft != INT_MAX)
                {
                    ++Sim.BricksDestroyed;
                    Dead = true;
                }

                Brick.HitsLeft = 0;
                Brick.Alive = false;
                Brick.DeathSec = Brick::DeathDuration;
            }
            else // Normal collision
            {
                if (!TryResolveBallRectCollision(Ball, BrickRect))
                {
                    continue;
                }

                // Brick Walls (INT_MAX) are indestructible
                if (Brick.HitsLeft != INT_MAX)
                {
                    --Brick.HitsLeft;
                    if (Brick.HitsLeft <= 0)
                    {
                        Brick.Alive = false;
                        ++Sim.BricksDestroyed;
                        Brick.DeathSec = Brick::DeathDuration;
                        Ball.HitStopSec = Ball::HitStopDuration;
                        Dead = true;
                    }
                }
            }

            // Combo builds on Brick death only. Some Bricks need more than one hit
            if (Dead)
            {
                ++Sim.Combo;
                Sim.Score += Brick.PointValue * Sim.Combo;
            }

            OutHitBricks.push_back(i);

            if (!Sim.ThroughActive)
            {
                break;
            }
        }
    }
}

void UpdatePad(SimState& Sim, const float DeltaTime, const float PlayfieldWidth, const float PlayfieldOffsetX, const float PaddleSpeed, const bool MouseControl)
{
    const float PaddleOldPosX = Sim.Pad.Position.x;

    if (IsGamepadAvailable(0))
    {
        const float Axis = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        constexpr float Deadzone = 0.1f;

        if (std::abs(Axis) > Deadzone)
        {
            Sim.Pad.Position.x = std::clamp(Sim.Pad.Position.x + Axis * PaddleSpeed * DeltaTime, 0.f, PlayfieldWidth - Sim.Pad.Width);
        }
    }
    else
    {
        if (MouseControl)
        {
            constexpr float PadMouseSmoothing = 15.f;
            const float LocalX = static_cast<float>(GetMouseX()) - PlayfieldOffsetX - Sim.Pad.Width * 0.5f;
            const float TargetX = std::clamp(LocalX, 0.f, PlayfieldWidth - Sim.Pad.Width);
            Sim.Pad.Position.x += (TargetX - Sim.Pad.Position.x) * PadMouseSmoothing * DeltaTime;
        }
        else
        {
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
            {
                Sim.Pad.Position.x = std::clamp(Sim.Pad.Position.x - PaddleSpeed * DeltaTime, 0.f, PlayfieldWidth - Sim.Pad.Width);
            }

            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
            {
                Sim.Pad.Position.x = std::clamp(Sim.Pad.Position.x + PaddleSpeed * DeltaTime, 0.f, PlayfieldWidth - Sim.Pad.Width);
            }
        }
    }

    // Paddle velocity influence ball direction
    Sim.Pad.VelocityX = (Sim.Pad.Position.x - PaddleOldPosX) / DeltaTime;
}

Ball MakeBall(const Vector2 Position, const Vector2 Velocity, const float Radius, const bool Caught, const float CaughtOffsetX)
{
    return {.Position = Position, .Velocity = Velocity, .Radius = Radius, .Caught = Caught, .CaughtOffsetX = CaughtOffsetX, .HitStopSec = 0.f};
}

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
    const float BrickWidth = (PlayfieldWidth - WallMarginPx * 2.f) / static_cast<float>(MaxCols);
    const float BrickHeight = BrickWidth * 0.4f;

    Sim.Bricks.clear();
    Sim.Bricks.reserve(Lines.size() * MaxCols);

    for (size_t Row = 0; Row < Lines.size(); ++Row)
    {
        for (size_t Col = 0; Col < Lines[Row].size(); ++Col)
        {
            const char Token = Lines[Row][Col];
            if (Token == ' ')
            {
                continue;
            }

            int Points;
            int Hits;
            Color BrickColor;

            switch (Token)
            {
                case 'Y': Points = 1; Hits = 1;       BrickColor = YELLOW; break;
                case 'G': Points = 3; Hits = 1;       BrickColor = GREEN;  break;
                case 'O': Points = 5; Hits = 1;       BrickColor = ORANGE; break;
                case 'R': Points = 7; Hits = 2;       BrickColor = RED;    break;
                case 'W': Points = 0; Hits = INT_MAX; BrickColor = {.r = 80, .g = 80, .b = 80, .a = 255}; break;
                default: continue;
            }

            Sim.Bricks.push_back({
                .Position = {.x = WallMarginPx + static_cast<float>(Col) * BrickWidth, .y = WallMarginPx + static_cast<float>(Row) * BrickHeight},
                .Width = BrickWidth,
                .Height = BrickHeight,
                .PointValue = Points,
                .HitsLeft = Hits,
                .Alive = true,
                .DeathSec = 0.f,
                .Tint = BrickColor
            });
        }
    }

    Sim.TotalInteractiveBricks = 0;
    for (const Brick& Brick : Sim.Bricks)
    {
        if (Brick.HitsLeft < INT_MAX)
        {
            ++Sim.TotalInteractiveBricks;
        }
    }

    spdlog::info("LoadLevelFromPath: {} bricks loaded ({} destructible)", Sim.Bricks.size(), Sim.TotalInteractiveBricks);
}

namespace
{
    void RecomputePadWidth(GameState& State)
    {
        constexpr float WideFactor = 1.6f;
        constexpr float ShrinkFactor = 0.6f;
        constexpr float MinWidth = 40.f;

        float Width = GetBasePadWidth(State);
        for (const ActiveEffect& Effect : State.ActiveEffects)
        {
            if (Effect.Type == PowerupType::WidePaddle)
            {
                Width *= WideFactor;
            }
            else if (Effect.Type == PowerupType::ShrinkPaddle)
            {
                Width *= ShrinkFactor;
            }
        }

        Paddle& Pad = State.Sim.Pad;
        const float Center = Pad.Position.x + Pad.Width * 0.5f;
        Pad.Width = std::clamp(Width, MinWidth, State.Field.Width);
        Pad.Position.x = std::clamp(Center - Pad.Width * 0.5f, 0.f, State.Field.Width - Pad.Width);
    }

    // Buffs
    void ApplyMultiball(GameState& State)
    {
        const size_t Count = State.Sim.Balls.size();
        if (Count == 0)
        {
            return;
        }

        constexpr float SpreadAngle = 20.f * DEG2RAD;
        const float Cos = std::cosf(SpreadAngle);
        const float Sin = std::sinf(SpreadAngle);

        State.Sim.Balls.reserve(Count * 3);

        for (size_t i = 0; i < Count; ++i)
        {
            // Copy on purpose
            const Ball CurrentBall = State.Sim.Balls[i];

            Ball LeftBall = CurrentBall;
            Ball RightBall = CurrentBall;

            // Flying Ball spreads immediately by rotating its velocity
            // Caught Ball (on the pad) has no velocity - just stack other spawned Balls and spread apart on LaunchBall
            if (!CurrentBall.Caught)
            {
                LeftBall.Velocity  = {.x = CurrentBall.Velocity.x * Cos - CurrentBall.Velocity.y * Sin, .y = CurrentBall.Velocity.x * Sin + CurrentBall.Velocity.y * Cos};
                RightBall.Velocity = {.x = CurrentBall.Velocity.x * Cos + CurrentBall.Velocity.y * Sin, .y = -CurrentBall.Velocity.x * Sin + CurrentBall.Velocity.y * Cos};
            }

            State.Sim.Balls.push_back(LeftBall);
            State.Sim.Balls.push_back(RightBall);
        }
    }

    void ApplyExtraLife(GameState& State)
    {
        ++State.Sim.Lives;
    }

    void ApplySlowBall(GameState& State)
    {
        State.Sim.BallSpeedMultiplier *= 0.6f;
    }

    void RevertSlowBall(GameState& State)
    {
        State.Sim.BallSpeedMultiplier /= 0.6f;
    }

    void ApplyLaser(GameState& State)
    {
        State.Sim.LaserActive = true;
    }

    void RevertLaser(GameState& State)
    {
        State.Sim.LaserActive = false;
    }

    void ApplyCatch(GameState& State)
    {
        State.Sim.CatchActive = true;
    }

    void RevertCatch(GameState& State)
    {
        State.Sim.CatchActive = false;
    }

    void ApplyThrough(GameState& State)
    {
        State.Sim.ThroughActive = true;
    }

    void RevertThrough(GameState& State)
    {
        State.Sim.ThroughActive = false;
    }

    void ApplyProtection(GameState& State)
    {
        State.Sim.ProtectionActive = true;
    }

    void RevertProtection(GameState& State)
    {
        State.Sim.ProtectionActive = false;
    }

    // Debuffs
    void ApplyFastBall(GameState& State)
    {
        State.Sim.BallSpeedMultiplier *= 1.4f;
    }

    void RevertFastBall(GameState& State)
    {
        State.Sim.BallSpeedMultiplier /= 1.4f;
    }
}

// Ref: https://strategywiki.org/wiki/Arkanoid/Gameplay
// Ref: https://strategywiki.org/wiki/Arkanoid:_Revenge_of_Doh/Gameplay
static constexpr PowerupDef Powerups[] =
{
    // Buffs
    {.Symbol = 'M', .Tint = RAYWHITE, .Duration = 0.f, .Text = "MULTIBALL!",   .Apply = ApplyMultiball,  .Revert = nullptr},          // Multiball - split every ball into three
    {.Symbol = 'H', .Tint = RED,      .Duration = 0.f, .Text = "EXTRA LIFE!",  .Apply = ApplyExtraLife,  .Revert = nullptr},          // Extra Life - grants +1 life
    {.Symbol = 'W', .Tint = SKYBLUE,  .Duration = 8.f, .Text = "WIDE PADDLE!", .Apply = nullptr,         .Revert = nullptr},          // Wide Paddle - width driven by RecomputePadWidth
    {.Symbol = 'S', .Tint = BEIGE,    .Duration = 8.f, .Text = "SLOW BALL!",   .Apply = ApplySlowBall,   .Revert = RevertSlowBall},   // Slow Ball - slow every ball
    {.Symbol = 'L', .Tint = ORANGE,   .Duration = 8.f, .Text = "LASER!",       .Apply = ApplyLaser,      .Revert = RevertLaser},      // Laser - paddle shoots lasers
    {.Symbol = 'C', .Tint = YELLOW,   .Duration = 8.f, .Text = "CATCH!",       .Apply = ApplyCatch,      .Revert = RevertCatch},      // Catch - make the ball stick to the paddle on next touch
    {.Symbol = 'T', .Tint = GREEN,    .Duration = 8.f, .Text = "THROUGH!",     .Apply = ApplyThrough,    .Revert = RevertThrough},    // Through - let the ball go through bricks without bouncing
    {.Symbol = 'P', .Tint = PINK,     .Duration = 15.f,.Text = "PROTECTION!",  .Apply = ApplyProtection, .Revert = RevertProtection}, // Protection - temporary wall at the bottom, saves one drop

    // Debuffs
    {.Symbol = 'X', .Tint = BROWN,    .Duration = 8.f, .Text = "SHRINK!",      .Apply = nullptr,         .Revert = nullptr},          // Shrink Paddle - width driven by RecomputePadWidth
    {.Symbol = 'F', .Tint = VIOLET,   .Duration = 8.f, .Text = "FAST BALL!",   .Apply = ApplyFastBall,   .Revert = RevertFastBall},   // Fast Ball - speed up every ball
};

static_assert(std::size(Powerups) == static_cast<size_t>(PowerupType::Count), "Powerups table out of sync with PowerupType");

const PowerupDef& GetPowerupDefinition(const PowerupType Type)
{
    return Powerups[static_cast<size_t>(Type)];
}

void UpdateDrops(std::vector<Drop>& Drops, const Paddle& Pad, const float DeltaTime, const float PlayfieldHeight, std::vector<PowerupType>& OutCaught, std::vector<Drop>& OutMissed)
{
    OutCaught.clear();
    OutMissed.clear();

    constexpr float FallSpeed = 180.f;

    const Rectangle PaddleRect =
    {
        .x = Pad.Position.x,
        .y = Pad.Position.y,
        .width = Pad.Width,
        .height = Pad.Height
    };

    for (int i = static_cast<int>(Drops.size()) - 1; i >= 0; --i)
    {
        Drop& D = Drops[i];
        D.Position.y += FallSpeed * DeltaTime;

        const Rectangle DropRect =
        {
            .x = D.Position.x,
            .y = D.Position.y,
            .width = D.Width,
            .height = D.Height
        };

        if (CheckCollisionRecs(DropRect, PaddleRect))
        {
            OutCaught.push_back(D.Type);
            Drops.erase(Drops.begin() + i);
        }
        else if (D.Position.y > PlayfieldHeight - DropRect.height)
        {
            OutMissed.push_back(D);
            Drops.erase(Drops.begin() + i);
        }
    }
}

void UpdateActiveEffects(GameState& State, const float DeltaTime)
{
    for (int i = static_cast<int>(State.ActiveEffects.size()) - 1; i >= 0; --i)
    {
        ActiveEffect& Effect = State.ActiveEffects[i];
        Effect.TimeLeft -= DeltaTime;

        if (Effect.TimeLeft <= 0.f)
        {
            const PowerupDef& Def = GetPowerupDefinition(Effect.Type);
            if (Def.Revert)
            {
                Def.Revert(State);
            }

            State.ActiveEffects.erase(State.ActiveEffects.begin() + i);
        }
    }

    // Pad width is derived from the Active Effects (picked up Powerups), so we need to recompute it
    RecomputePadWidth(State);
}

void UpdateLasers(SimState& Sim, const float DeltaTime, std::vector<int>& OutHitBricks)
{
    OutHitBricks.clear();

    for (int i = static_cast<int>(Sim.Lasers.size()) - 1; i >= 0; --i)
    {
        Laser& Laser = Sim.Lasers[i];

        constexpr float LaserSpeed = 900.f;
        Laser.Position.y -= LaserSpeed * DeltaTime;

        if (Laser.Position.y < 0.f)
        {
            Sim.Lasers.erase(Sim.Lasers.begin() + i);
            continue;
        }

        for (int j = 0; j < static_cast<int>(Sim.Bricks.size()); ++j)
        {
            Brick& Brick = Sim.Bricks[j];
            if (!Brick.Alive)
            {
                continue;
            }

            const Rectangle BrickRect =
            {
                .x = Brick.Position.x,
                .y = Brick.Position.y,
                .width = Brick.Width,
                .height = Brick.Height
            };

            if (!CheckCollisionPointRec(Laser.Position, BrickRect))
            {
                continue;
            }

            // Brick Walls (INT_MAX) die to Lasers but don't count toward level clear
            if (Brick.HitsLeft == INT_MAX)
            {
                Brick.Alive = false;
                Brick.DeathSec = Brick::DeathDuration;
            }
            else
            {
                --Brick.HitsLeft;
                if (Brick.HitsLeft <= 0)
                {
                    Brick.Alive = false;
                    Brick.DeathSec = Brick::DeathDuration;
                    ++Sim.BricksDestroyed;

                    // Combo builds on kills only, not on every hit
                    ++Sim.Combo;
                    Sim.Score += Brick.PointValue * Sim.Combo;
                }
            }

            OutHitBricks.push_back(j);
            Sim.Lasers.erase(Sim.Lasers.begin() + i);
            break;
        }
    }
}
