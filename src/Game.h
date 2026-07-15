#pragma once

#include <cstdint>
#include <vector>
#include "raylib/raylib.h"

// Ref: https://github.com/raysan5/raylib/discussions/4636
inline float GetRandomFloatValue(const float Min, const float Max)
{
    constexpr int Resolution = 10000;
    return Min + (Max - Min) * (static_cast<float>(GetRandomValue(0, Resolution)) / Resolution);
}

enum class GameScreen : std::uint8_t
{
    MainMenu,
    Options,
    Game,
    Pause,
    GameOver,
    Victory
};

enum class PowerupType : std::uint8_t
{
    // Buffs
    Multiball,
    ExtraLife,
    WidePaddle,
    SlowBall,
    Laser,
    Catch,
    Through,
    Protection,

    // Debuffs
    ShrinkPaddle,
    FastBall,

    Count
};

struct Ball
{
    Vector2 Position;
    Vector2 Velocity;
    float Radius;
    bool Caught;
    float CaughtOffsetX;
    float HitStopSec;

    // Global data
    static constexpr float HitStopDuration = 0.04f;
};

struct Paddle
{
    Vector2 Position;
    float VelocityX;
    float Width;
    float Height;
    float FlashSec;

    // Global data
    static constexpr float FlashDuration = 0.12f;
};

struct Brick
{
    Vector2 Position;
    float Width;
    float Height;
    int PointValue;
    int HitsLeft;
    bool Alive;
    float DeathSec;
    Color Tint;

    // Global data
    static constexpr float DeathDuration = 0.18f;
};

struct Particle
{
    Vector2 Position;
    Vector2 Velocity;
    Color Tint;
    float InitialRadius;
    float Lifetime;
    float MaxLifetime;
};

struct Laser
{
    Vector2 Position;

    // Global data
    static constexpr float FireCooldown = 0.25f;
};

struct SimState // if you add new vars in here - do NOT forget to reset them in "StartLevel"
{
    int Lives;
    int Score;
    int Combo;

    Paddle Pad;
    std::vector<Ball> Balls;
    std::vector<Brick> Bricks;

    float BallSpeedMultiplier;
    bool BallLaunched;

    int TotalInteractiveBricks;
    int BricksDestroyed;
    double LastDropTime;

    // Buffs/Debuffs
    std::vector<Laser> Lasers;
    float LaserCooldownSec;
    bool LaserActive;

    bool CatchActive;
    bool ThroughActive;
    bool ProtectionActive;
};

struct Playfield
{
    float X;
    float Y;
    float Width;
    float Height;
};

struct BackgroundParticle
{
    Vector2 Position;
    float Speed;
    float Radius;
    float Alpha;
};

struct Drop
{
    Vector2 Position;
    float Width;
    float Height;
    PowerupType Type;
};

// We use free func, so there is no point for std::function<void(GameState&)>
using PowerupFn = void(*)(struct GameState&);

struct PowerupDef
{
    char Symbol;
    Color Tint;
    float Duration;
    const char* Text;
    PowerupFn Apply;
    PowerupFn Revert;
};

struct ActiveEffect
{
    PowerupType Type;
    float TimeLeft;
};

struct FloatingText
{
    Vector2 Position;
    char Text[32];
    Color Tint;
    int FontSize;
    float Lifetime;
    float MaxLifetime;
    bool Shake;
};

struct GameState
{
    GameScreen Screen;
    Playfield Field;
    SimState Sim;
    std::vector<Particle> Particles;
    std::vector<BackgroundParticle> BackgroundParticles;
    std::vector<Drop> Drops;
    std::vector<ActiveEffect> ActiveEffects;
    std::vector<FloatingText> FloatingTexts;
    float ShakeScale;
    float LevelIntroSec;
    int HighestScore;
    int CurrentLevel;
    bool MouseControl;

    // Global data
    static constexpr float LevelIntroDuration = 0.75f;
};

inline float GetBasePadWidth(const GameState& State)
{
    constexpr float WidthFactor = 8.f;
    return State.Field.Width / WidthFactor;
}

Ball MakeBall(Vector2 Position, Vector2 Velocity, float Radius, bool Caught = false, float CaughtOffsetX = 0.f);

void UpdateBall(SimState& Sim, const float DeltaTime);

void UpdateBallSpeed(SimState& Sim, const float BaseSpeed);

bool UpdateBallWallCollision(SimState& Sim, const float PlayfieldWidth, const float PlayfieldHeight, std::vector<Vector2>& OutDeadBalls);

bool UpdateBallPadCollision(SimState& Sim);

bool TryResolveBallRectCollision(Ball& Ball, const Rectangle& Rect);

void UpdateBallBrickCollision(SimState& Sim, std::vector<int>& OutHitBricks);

void UpdatePad(SimState& Sim, const float DeltaTime, const float PlayfieldWidth, const float PlayfieldOffsetX, const float PaddleSpeed, const bool MouseControl);

void LoadLevelFromPath(SimState& Sim, const char* Path, const float PlayfieldWidth);

void SpawnParticles(std::vector<Particle>& Particles, const Vector2& Position, const Color& Tint, const int Count);

void UpdateParticles(std::vector<Particle>& Particles, const float DeltaTime);

void SpawnFloatingText(std::vector<FloatingText>& Texts, const Vector2 Position, const char* Text, const Color Tint, const int FontSize, const float Lifetime, const bool Shake);

void UpdateFloatingTexts(std::vector<FloatingText>& Texts, const float DeltaTime);

void InitBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const int Count, const float Width, const float Height);

void UpdateBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const float DeltaTime, const float Width, const float Height);

const PowerupDef& GetPowerupDefinition(const PowerupType Type);

void UpdateDrops(std::vector<Drop>& Drops, const Paddle& Pad, const float DeltaTime, const float PlayfieldHeight, std::vector<PowerupType>& OutCaught, std::vector<Drop>& OutMissed);

void UpdateActiveEffects(GameState& State, const float DeltaTime);

void UpdateLasers(SimState& Sim, const float DeltaTime, std::vector<int>& OutHitBricks);
