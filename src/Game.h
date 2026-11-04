#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>
#include "Biome.h"
#include "Physics.h"
#include "Terrain.h"

// Ref: https://github.com/raysan5/raylib/discussions/4636
inline float GetRandomFloatValue(const float Min, const float Max)
{
    constexpr int Resolution = 10000;
    return Min + (Max - Min) * (static_cast<float>(GetRandomValue(0, Resolution)) / Resolution);
}

enum class GameScreen : std::uint8_t
{
    MainMenu,
    Game,
    Victory
};

enum class TurnPhase : std::uint8_t
{
    Aiming,   // active worm moves and attacks
    Retreat,  // attack is spent, a few seconds to run
    Settling, // waiting for everything to come to rest
    Deaths    // worms at 0 HP blow up one by one
};

enum class WeaponKind : std::uint8_t
{
    Rocket,
    Grenade,
    ClusterBomb,
    Airstrike,
    NinjaRope,
    Teleport,
    Shotgun,
    Count
};

constexpr int WeaponCount = static_cast<int>(WeaponKind::Count);

enum class AimMode : std::uint8_t
{
    Charge,  // hold to charge, release to throw
    Instant, // fires on click along the aim line
    Target   // fires on click at the cursor position
};

struct WeaponInfo
{
    const char* Name;
    int StartAmmo; // -1 = unlimited
    AimMode Mode;
};

const WeaponInfo& GetWeaponInfo(WeaponKind Kind);

enum class WormMotion : std::uint8_t
{
    Idle,
    Walking,
    Airborne, // jumped or fell: no control until landing
    Knocked,  // thrown by a blast: no control until it comes to rest
    Roping
};

struct Worm
{
    b2BodyId Body = b2_nullBodyId;
    std::uint32_t Id = 0;
    char Name[16] = {};
    int Team = 0;
    int HP = 100;
    bool Alive = true;
    WormMotion Motion = WormMotion::Airborne;
    bool Grounded = false;
    float FacingDir = 1.f;
    float AimAngle = 0.f;
    WeaponKind Weapon = WeaponKind::Rocket; // remembered between this worm's turns
    int FuseSeconds = 3;
    float MoveInput = 0.f;
    float FallSpeed = 0.f;   // m/s, peak downward speed while airborne, for fall damage
    float RestTimer = 0.f;   // time spent nearly motionless while airborne
    float SettleTimer = 0.f; // time spent resting after being knocked
    float FlashSec = 0.f;
    float AnimTime = 0.f;
    float SpinAngle = 0.f;   // purely visual tumble while knocked
    Vector2 Position{};      // physics state at the last step, gameplay reads this
    Vector2 PrevPosition{};
    Vector2 DrawPosition{};  // interpolated between steps, rendering reads this
    Vector2 Velocity{};

    static constexpr int MaxHP = 100;
    static constexpr float Radius = 9.f;
    static constexpr float FlashDuration = 0.2f;
};

constexpr int TeamCount = 2;
constexpr int WormsPerTeam = 4;

struct Team
{
    char Name[16] = {};
    Color Tint{};
    int NextWorm = 0; // index into this team's worms for round robin
    std::array<int, WeaponCount> Ammo{};
};

enum class ProjectileKind : std::uint8_t
{
    Rocket,
    Grenade,
    ClusterBomb,
    Bomblet,
    AirstrikeBomb
};

struct Projectile
{
    ProjectileKind Kind = ProjectileKind::Rocket;
    b2BodyId Body = b2_nullBodyId;
    std::uint32_t Id = 0;
    float Fuse = 0.f;     // seconds, <= 0 means impact fused
    bool Detonate = false;
    Vector2 Position{};
    Vector2 Velocity{};
    float Angle = 0.f;
    Vector2 PrevPosition{};
    float PrevAngle = 0.f;
    Vector2 DrawPosition{};
    float DrawAngle = 0.f;
    bool UsesWind = false;
};

struct Debris
{
    b2BodyId Body = b2_nullBodyId;
    std::uint32_t Id = 0;
    int Width = 0;
    int Height = 0;
    std::vector<std::uint8_t> Solid;
    std::vector<Color> Pixels;
    Texture2D Texture{};
    float RestTimer = 0.f;
    float Age = 0.f;
    Vector2 Position{};
    float Angle = 0.f;
    Vector2 PrevPosition{};
    float PrevAngle = 0.f;
    Vector2 DrawPosition{};
    float DrawAngle = 0.f;
};

struct Grave
{
    b2BodyId Body = b2_nullBodyId;
    std::uint32_t Id = 0;
    Color Tint{};
    Vector2 Position{};
    float Angle = 0.f;
    Vector2 PrevPosition{};
    float PrevAngle = 0.f;
    Vector2 DrawPosition{};
    float DrawAngle = 0.f;
};

enum class ParticleKind : std::uint8_t
{
    Spark, // additive-looking glow, no gravity
    Smoke, // grows and fades, drifts up
    Dirt   // falls with gravity
};

struct Particle
{
    ParticleKind Kind;
    Vector2 Position;
    Vector2 Velocity;
    Color Tint;
    float InitialRadius;
    float Lifetime;
    float MaxLifetime;
};

struct Tracer
{
    Vector2 From;
    Vector2 To;
    float Lifetime;
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

struct GameCamera
{
    Vector2 Target{};
    float Zoom = 1.f;     // user zoom on top of the window fit
    bool Manual = false;  // middle mouse panned, stop following until something new happens
    Vector2 Focus{};
};

struct Match
{
    b2WorldId World = b2_nullWorldId;
    b2BodyId AnchorBody = b2_nullBodyId; // static body the rope attaches to, chunk bodies get rebuilt under it
    TerrainMap Terrain;
    BiomeKind Biome = BiomeKind::Meadow;
    Backdrop Background;
    float LiquidLevel = 0.f;
    std::mt19937 Rng;

    std::vector<Worm> Worms;
    std::array<Team, TeamCount> Teams;
    std::vector<Projectile> Projectiles;
    std::vector<Debris> DebrisPieces;
    std::vector<Grave> Graves;
    std::uint32_t NextId = 1;

    int CurrentTeam = 0;
    int CurrentWorm = 0;
    TurnPhase Phase = TurnPhase::Aiming;
    float TurnTimer = 0.f;
    float PhaseTimer = 0.f;
    float Wind = 0.f; // m/s^2 applied to wind-affected projectiles
    int TurnNumber = 0;

    WeaponKind Weapon = WeaponKind::Rocket;
    bool Charging = false;
    float Charge = 0.f;
    int FuseSeconds = 3;
    int ShotsLeft = 0;

    b2JointId Rope = b2_nullJointId;
    Vector2 RopeAnchor{};

    float PhysicsAccumulator = 0.f;
    float Time = 0.f;
    std::uint32_t FollowProjectile = 0; // camera tracks this projectile id while it lives
};

struct GameSounds
{
    Sound Explosion{};
};

struct GameState
{
    GameScreen Screen = GameScreen::MainMenu;
    Match Game;
    GameCamera Camera;
    std::vector<Particle> Particles;
    std::vector<Tracer> Tracers;
    std::vector<FloatingText> FloatingTexts;
    float ShakeScale = 0.f;
    int Winner = -1; // team index, -2 = draw
    bool WaitForMouseRelease = false;
    bool WeaponPanelOpen = false;
    bool Paused = false;
    int MenuBiome = -1; // -1 = random
    GameSounds Sounds;
};

constexpr int MapWidth = 3200;
constexpr int MapHeight = 1500;

void StartMatch(GameState& State, std::uint32_t Seed, BiomeKind Biome);

void DestroyMatch(Match& Game);

void UpdateGameplay(GameState& State, float DeltaTime);

void UpdateEffects(GameState& State, float DeltaTime);

// World transform for this frame: window fit * user zoom, centered on the camera target, without shake
Camera2D GetWorldCamera(const GameState& State);

Rectangle GetVisibleWorld(const Camera2D& View);

const Worm* GetActiveWorm(const Match& Game);

// Shared by gameplay and HUD so the preview matches what will fire
float GetLaunchSpeed(WeaponKind Weapon, float Charge);

// Screen-space slot in the weapon panel, shared by input hit-testing and drawing
Rectangle GetWeaponSlotRect(int Index);

// HUD scale relative to a 1080p window
float GetUiScale();

// Screen-space centered button column, shared by the main menu and the pause menu
Rectangle GetMenuButtonRect(int Index);

const char* GetMenuBiomeName(int MenuBiome);

void SpawnFloatingText(std::vector<FloatingText>& Texts, Vector2 Position, const char* Text, Color Tint, int FontSize, float Lifetime, bool Shake);

void RenderFrame(const GameState& State);
