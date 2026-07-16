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
    Victory
};

enum class TurnPhase : std::uint8_t
{
    Aiming,
    Firing,
    Settling
};

struct Worm
{
    Vector2 Position;
    Vector2 Velocity;
    int HP;
    char Name[16];
    float FacingDir;
    bool OnGround;
    float AimAngle;
    bool Charging;
    float ChargePower;
    bool Alive;
    float FlashSec;

    // Global data
    static constexpr int MaxHP = 100;
    static constexpr float FlashDuration = 0.15f;
    static constexpr float Radius = 20.f;
};

struct TerrainBlock
{
    Vector2 Position;
    float Width;
    float Height;
    int HitsLeft;
    bool Alive;
    float DeathSec;
    Color Tint;

    // Global data
    static constexpr float DeathDuration = 0.18f;
};

struct Projectile
{
    Vector2 Position;
    Vector2 Velocity;
    bool Active;

    // Global data
    static constexpr float Radius = 8.f;
};

struct SimState
{
    Worm Worms[2];
    std::vector<TerrainBlock> Terrain;
    Projectile Shot;
    int CurrentPlayer;
    TurnPhase Phase;
    float SettleTimer;
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
    std::vector<FloatingText> FloatingTexts;
    float ShakeScale;
    int Winner; // -1 = no winner yet, otherwise 0 or 1
};

void UpdateWorm(Worm& W, const std::vector<TerrainBlock>& Terrain, const float DeltaTime, const float PlayfieldWidth, const float PlayfieldHeight, const bool AcceptInput);

bool UpdateWormAim(Worm& W, const Vector2 MouseVirtualPos, const float DeltaTime, Vector2& OutFireVelocity);

void UpdateProjectile(Projectile& Shot, const float DeltaTime);

struct ExplosionResult
{
    std::vector<int> HitTerrainIndices;
    std::vector<int> HitWormIndices;
    Vector2 Center;
};

ExplosionResult ApplyExplosionDamage(SimState& Sim, const Vector2 Center, const float Radius, const int MaxDamage);

bool CheckWormBelowField(Worm& W, const float PlayfieldHeight);

std::vector<Vector2> SimulateTrajectory(const Vector2 Origin, const Vector2 Velocity, const float TimeStep, const int MaxPoints);

void LoadLevelFromPath(SimState& Sim, const char* Path, const float PlayfieldWidth, const float PlayfieldHeight);

void SpawnParticles(std::vector<Particle>& Particles, const Vector2& Position, const Color& Tint, const int Count);

void UpdateParticles(std::vector<Particle>& Particles, const float DeltaTime);

void SpawnFloatingText(std::vector<FloatingText>& Texts, const Vector2 Position, const char* Text, const Color Tint, const int FontSize, const float Lifetime, const bool Shake);

void UpdateFloatingTexts(std::vector<FloatingText>& Texts, const float DeltaTime);

void InitBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const int Count, const float Width, const float Height);

void UpdateBackgroundParticles(std::vector<BackgroundParticle>& BackgroundParticles, const float DeltaTime, const float Width, const float Height);
