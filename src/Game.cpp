#include "Game.h"
#include "raylib/raymath.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr std::array<WeaponInfo, WeaponCount> Weapons =
    {{
        {.Name = "Rocket Launcher", .StartAmmo = -1, .Mode = AimMode::Charge},
        {.Name = "Grenade", .StartAmmo = -1, .Mode = AimMode::Charge},
        {.Name = "Cluster Bomb", .StartAmmo = 3, .Mode = AimMode::Charge},
        {.Name = "Airstrike", .StartAmmo = 1, .Mode = AimMode::Target},
        {.Name = "Ninja Rope", .StartAmmo = -1, .Mode = AimMode::Instant},
        {.Name = "Teleport", .StartAmmo = 2, .Mode = AimMode::Target},
        {.Name = "Shotgun", .StartAmmo = -1, .Mode = AimMode::Instant}
    }};

    constexpr float GravityMps2 = 18.f;
    constexpr float PhysicsStep = 1.f / 60.f;
    constexpr int PhysicsSubSteps = 4;

    constexpr float TurnDuration = 45.f;
    constexpr float RetreatDuration = 3.f;
    constexpr float SettleMinDuration = 0.5f;
    constexpr float SettleMaxDuration = 10.f;
    constexpr float DeathInterval = 0.6f;
    constexpr float MaxWind = 5.f;

    constexpr float WalkSpeed = 2.2f;
    constexpr float FallDamageSpeed = 11.f;
    constexpr int MaxDebrisPixels = 9000;
    constexpr float RopeMaxLength = 420.f; // px
    constexpr float ChargeRatePerSec = 0.9f;
    constexpr float ViewWorldHeight = 760.f; // world px visible vertically at zoom 1

    constexpr std::array<const char*, 16> WormNames =
    {
        "Noodle", "Wiggles", "Gus", "Pickle", "Sprout", "Moss", "Biscuit", "Taco",
        "Pip", "Dumpling", "Clover", "Nugget", "Fizz", "Marble", "Turnip", "Bean"
    };

    struct ExplosionSpec
    {
        float Radius;
        int Damage;
        float Knock; // m/s at the center
    };

    constexpr ExplosionSpec RocketBlast = {.Radius = 46.f, .Damage = 50, .Knock = 9.f};
    constexpr ExplosionSpec GrenadeBlast = {.Radius = 44.f, .Damage = 50, .Knock = 9.f};
    constexpr ExplosionSpec ClusterBlast = {.Radius = 30.f, .Damage = 25, .Knock = 6.f};
    constexpr ExplosionSpec BombletBlast = {.Radius = 24.f, .Damage = 20, .Knock = 5.f};
    constexpr ExplosionSpec AirstrikeBlast = {.Radius = 32.f, .Damage = 25, .Knock = 6.f};
    constexpr ExplosionSpec DeathBlast = {.Radius = 30.f, .Damage = 15, .Knock = 6.f};
    constexpr ExplosionSpec ShotgunBlast = {.Radius = 14.f, .Damage = 10, .Knock = 3.f};

    // The default query category is bit 0, which is Terrain, and terrain chains ignore Terrain. Queries need a category terrain accepts
    b2QueryFilter MakeQueryFilter()
    {
        b2QueryFilter Filter = b2DefaultQueryFilter();
        Filter.categoryBits = Category::Projectile;
        return Filter;
    }

    // ---------- Lookup ----------

    Worm* FindWorm(Match& Game, const std::uint32_t Id)
    {
        for (Worm& W : Game.Worms)
        {
            if (W.Id == Id && W.Alive)
            {
                return &W;
            }
        }
        return nullptr;
    }

    Projectile* FindProjectile(Match& Game, const std::uint32_t Id)
    {
        for (Projectile& P : Game.Projectiles)
        {
            if (P.Id == Id)
            {
                return &P;
            }
        }
        return nullptr;
    }

    Worm* GetActiveWormMutable(Match& Game)
    {
        if (Game.CurrentWorm < 0 || Game.CurrentWorm >= static_cast<int>(Game.Worms.size()))
        {
            return nullptr;
        }
        Worm& W = Game.Worms[static_cast<size_t>(Game.CurrentWorm)];
        return W.Alive ? &W : nullptr;
    }

    bool IsActiveWorm(const Match& Game, const Worm& W)
    {
        return &Game.Worms[static_cast<size_t>(Game.CurrentWorm)] == &W;
    }

    // ---------- Effects ----------

    void SpawnBurst(std::vector<Particle>& Particles, const ParticleKind Kind, const Vector2 Position, const Color Tint, const int Count, const float MinSpeed, const float MaxSpeed, const float MinRadius, const float MaxRadius, const float MaxLife)
    {
        for (int i = 0; i < Count; ++i)
        {
            const float Angle = GetRandomFloatValue(0.f, 2.f * PI);
            const float Speed = GetRandomFloatValue(MinSpeed, MaxSpeed);
            const float Life = GetRandomFloatValue(MaxLife * 0.5f, MaxLife);
            Particles.push_back({
                .Kind = Kind,
                .Position = Position,
                .Velocity = {.x = std::cos(Angle) * Speed, .y = std::sin(Angle) * Speed},
                .Tint = Tint,
                .InitialRadius = GetRandomFloatValue(MinRadius, MaxRadius),
                .Lifetime = Life,
                .MaxLifetime = Life
            });
        }
    }

    void SpawnSplash(GameState& State, const Vector2 Position)
    {
        const BiomeTheme& Theme = GetBiomeTheme(State.Game.Biome);
        for (int i = 0; i < 18; ++i)
        {
            const float Life = GetRandomFloatValue(0.4f, 0.9f);
            State.Particles.push_back({
                .Kind = ParticleKind::Dirt,
                .Position = {.x = Position.x + GetRandomFloatValue(-8.f, 8.f), .y = State.Game.LiquidLevel},
                .Velocity = {.x = GetRandomFloatValue(-60.f, 60.f), .y = GetRandomFloatValue(-260.f, -80.f)},
                .Tint = ColorLerp(Theme.LiquidTop, WHITE, 0.4f),
                .InitialRadius = GetRandomFloatValue(2.f, 4.f),
                .Lifetime = Life,
                .MaxLifetime = Life
            });
        }
    }

    void PlayExplosionSound(const GameState& State, const float Radius)
    {
        SetSoundPitch(State.Sounds.Explosion, std::clamp(1.4f - Radius / 80.f, 0.7f, 1.5f) * GetRandomFloatValue(0.92f, 1.08f));
        PlaySfx(State.Sounds.Explosion);
    }

    // ---------- Bodies ----------

    b2BodyId CreateCircleBody(const Match& Game, const Vector2 Position, const float RadiusPx, const b2BodyDef& BaseDef, const b2ShapeDef& ShapeDef)
    {
        b2BodyDef Def = BaseDef;
        Def.position = ToMeters(Position);
        const b2BodyId Body = b2CreateBody(Game.World, &Def);
        const b2Circle Circle = {.center = {0.f, 0.f}, .radius = RadiusPx / PixelsPerMeter};
        b2CreateCircleShape(Body, &ShapeDef, &Circle);
        return Body;
    }

    void CreateWorm(Match& Game, const Vector2 Position, const int TeamIndex, const char* Name)
    {
        Worm W;
        W.Id = Game.NextId++;
        W.Team = TeamIndex;
        W.HP = Worm::MaxHP;
        W.FacingDir = Position.x < static_cast<float>(MapWidth) * 0.5f ? 1.f : -1.f;
        W.AimAngle = W.FacingDir > 0.f ? -0.4f : PI + 0.4f;
        W.Position = Position;
        W.AnimTime = GetRandomFloatValue(0.f, 10.f);
        snprintf(W.Name, sizeof(W.Name), "%s", Name);

        b2BodyDef Def = b2DefaultBodyDef();
        Def.type = b2_dynamicBody;
        Def.motionLocks.angularZ = true;
        // Idle worms switch gravity off to stop sliding down slopes, so they must never sleep through the ground vanishing
        Def.enableSleep = false;
        Def.userData = MakeBodyTag(EntityType::Worm, W.Id);

        b2ShapeDef Shape = b2DefaultShapeDef();
        Shape.density = 1.f;
        Shape.material.friction = 0.6f;
        Shape.material.restitution = 0.15f;
        Shape.filter.categoryBits = Category::Worm;
        Shape.filter.maskBits = Category::Terrain | Category::Debris | Category::Projectile;

        W.Body = CreateCircleBody(Game, Position, Worm::Radius, Def, Shape);
        W.PrevPosition = W.DrawPosition = W.Position;
        Game.Worms.push_back(W);
    }

    void CreateGrave(Match& Game, const Vector2 Position, const Color Tint)
    {
        Grave G;
        G.Id = Game.NextId++;
        G.Tint = Tint;
        G.Position = Position;

        b2BodyDef Def = b2DefaultBodyDef();
        Def.type = b2_dynamicBody;
        Def.position = ToMeters(Position);
        Def.userData = MakeBodyTag(EntityType::Grave, G.Id);
        G.Body = b2CreateBody(Game.World, &Def);

        b2ShapeDef Shape = b2DefaultShapeDef();
        Shape.density = 1.5f;
        Shape.material.friction = 0.8f;
        Shape.filter.categoryBits = Category::Grave;
        Shape.filter.maskBits = Category::Terrain | Category::Debris | Category::Grave;
        const b2Polygon Box = b2MakeBox(6.f / PixelsPerMeter, 8.f / PixelsPerMeter);
        b2CreatePolygonShape(G.Body, &Shape, &Box);

        G.PrevPosition = G.DrawPosition = G.Position;
        Game.Graves.push_back(G);
    }

    void CreateProjectile(Match& Game, const ProjectileKind Kind, const Vector2 Position, const Vector2 VelocityMps, const float Fuse)
    {
        Projectile P;
        P.Kind = Kind;
        P.Id = Game.NextId++;
        P.Fuse = Fuse;
        P.Position = Position;
        P.UsesWind = Kind == ProjectileKind::Rocket || Kind == ProjectileKind::AirstrikeBomb || Kind == ProjectileKind::Bomblet;

        const bool Bouncy = Kind == ProjectileKind::Grenade || Kind == ProjectileKind::ClusterBomb;
        const float Radius = Kind == ProjectileKind::Bomblet ? 3.5f : (Bouncy ? 5.f : 4.f);

        b2BodyDef Def = b2DefaultBodyDef();
        Def.type = b2_dynamicBody;
        Def.linearVelocity = {VelocityMps.x, VelocityMps.y};
        Def.angularDamping = 0.6f;
        Def.isBullet = !Bouncy;
        Def.enableSleep = false;
        Def.userData = MakeBodyTag(EntityType::Projectile, P.Id);

        b2ShapeDef Shape = b2DefaultShapeDef();
        Shape.density = Bouncy ? 3.f : 2.f;
        Shape.material.friction = 0.5f;
        Shape.material.restitution = Bouncy ? 0.45f : 0.f;
        Shape.material.rollingResistance = Bouncy ? 0.15f : 0.f;
        Shape.enableContactEvents = !Bouncy;
        Shape.filter.categoryBits = Category::Projectile;
        Shape.filter.maskBits = Category::Terrain | Category::Debris | Category::Worm;

        P.Body = CreateCircleBody(Game, Position, Radius, Def, Shape);
        P.Angle = Fuse > 0.f ? 0.f : std::atan2(VelocityMps.y, VelocityMps.x); // matches what ReadBodyTransforms will report
        P.PrevPosition = P.DrawPosition = P.Position;
        P.PrevAngle = P.DrawAngle = P.Angle;
        Game.Projectiles.push_back(P);
    }

    void CreateDebris(GameState& State, TerrainPiece& Piece)
    {
        Match& Game = State.Game;

        // Crumbs are not worth a rigid body
        if (Piece.SolidCount < 40)
        {
            const Vector2 Center = {.x = static_cast<float>(Piece.X) + static_cast<float>(Piece.Width) * 0.5f, .y = static_cast<float>(Piece.Y) + static_cast<float>(Piece.Height) * 0.5f};
            Color Tint = GRAY;
            for (size_t i = 0; i < Piece.Pixels.size(); ++i)
            {
                if (Piece.Solid[i])
                {
                    Tint = Piece.Pixels[i];
                    break;
                }
            }
            SpawnBurst(State.Particles, ParticleKind::Dirt, Center, Tint, 6, 20.f, 90.f, 1.f, 2.5f, 0.8f);
            return;
        }

        Debris D;
        D.Id = Game.NextId++;
        D.Width = Piece.Width;
        D.Height = Piece.Height;
        D.Solid = std::move(Piece.Solid);
        D.Pixels = std::move(Piece.Pixels);
        D.Position = {.x = static_cast<float>(Piece.X) + static_cast<float>(D.Width) * 0.5f, .y = static_cast<float>(Piece.Y) + static_cast<float>(D.Height) * 0.5f};

        b2BodyDef Def = b2DefaultBodyDef();
        Def.type = b2_dynamicBody;
        Def.position = ToMeters(D.Position);
        Def.angularDamping = 0.3f;
        Def.userData = MakeBodyTag(EntityType::Debris, D.Id);
        D.Body = b2CreateBody(Game.World, &Def);

        b2ShapeDef Shape = b2DefaultShapeDef();
        Shape.density = 2.f;
        Shape.material.friction = 0.8f;
        Shape.material.restitution = 0.05f;
        Shape.enableHitEvents = true;
        Shape.filter.categoryBits = Category::Debris;
        Shape.filter.maskBits = Category::Terrain | Category::Debris | Category::Worm | Category::Projectile | Category::Grave;

        // Approximate the pixels with boxes: one per horizontal run of mostly-solid cells
        const int Cell = Piece.SolidCount > 2500 ? 6 : 4;
        int ShapeCount = 0;
        for (int BandY = 0; BandY < D.Height; BandY += Cell)
        {
            const int BandH = std::min(Cell, D.Height - BandY);
            int RunStart = -1;
            for (int CellX = 0; CellX <= D.Width; CellX += Cell)
            {
                bool CellSolid = false;
                if (CellX < D.Width)
                {
                    const int CellW = std::min(Cell, D.Width - CellX);
                    int Count = 0;
                    for (int Y = BandY; Y < BandY + BandH; ++Y)
                    {
                        for (int X = CellX; X < CellX + CellW; ++X)
                        {
                            Count += D.Solid[static_cast<size_t>(Y * D.Width + X)];
                        }
                    }
                    CellSolid = Count * 2 >= CellW * BandH;
                }

                if (CellSolid && RunStart < 0)
                {
                    RunStart = CellX;
                }
                else if (!CellSolid && RunStart >= 0)
                {
                    const float RunW = static_cast<float>(std::min(CellX, D.Width) - RunStart);
                    const Vector2 LocalCenter = {
                        .x = static_cast<float>(RunStart) + RunW * 0.5f - static_cast<float>(D.Width) * 0.5f,
                        .y = static_cast<float>(BandY) + static_cast<float>(BandH) * 0.5f - static_cast<float>(D.Height) * 0.5f
                    };
                    const b2Polygon Box = b2MakeOffsetBox(RunW * 0.5f / PixelsPerMeter, static_cast<float>(BandH) * 0.5f / PixelsPerMeter, ToMeters(LocalCenter), b2MakeRot(0.f));
                    b2CreatePolygonShape(D.Body, &Shape, &Box);
                    ++ShapeCount;
                    RunStart = -1;
                }
            }
        }

        // Thin slivers can fail the majority test everywhere
        if (ShapeCount == 0)
        {
            const b2Polygon Box = b2MakeBox(static_cast<float>(D.Width) * 0.4f / PixelsPerMeter, static_cast<float>(D.Height) * 0.4f / PixelsPerMeter);
            b2CreatePolygonShape(D.Body, &Shape, &Box);
        }

        const Image Source = {.data = D.Pixels.data(), .width = D.Width, .height = D.Height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        D.Texture = LoadTextureFromImage(Source);
        SetTextureFilter(D.Texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(D.Texture, TEXTURE_WRAP_CLAMP);

        D.PrevPosition = D.DrawPosition = D.Position;
        Game.DebrisPieces.push_back(std::move(D));
    }

    void DestroyDebris(Debris& D)
    {
        if (B2_IS_NON_NULL(D.Body))
        {
            b2DestroyBody(D.Body);
            D.Body = b2_nullBodyId;
        }
        if (D.Texture.id != 0)
        {
            UnloadTexture(D.Texture);
            D.Texture = {};
        }
    }

    // Settled debris goes back into the terrain mask so it can be destroyed again and costs nothing to simulate
    void RestampDebris(Match& Game, const Debris& D)
    {
        const b2WorldTransform Transform = b2Body_GetTransform(D.Body);
        const float HalfW = static_cast<float>(D.Width) * 0.5f;
        const float HalfH = static_cast<float>(D.Height) * 0.5f;

        float MinX = 1e9f;
        float MinY = 1e9f;
        float MaxX = -1e9f;
        float MaxY = -1e9f;
        for (const Vector2 Corner : {Vector2{.x = -HalfW, .y = -HalfH}, Vector2{.x = HalfW, .y = -HalfH}, Vector2{.x = -HalfW, .y = HalfH}, Vector2{.x = HalfW, .y = HalfH}})
        {
            const Vector2 World = ToPixels(b2TransformWorldPoint(Transform, ToMeters(Corner)));
            MinX = std::min(MinX, World.x);
            MinY = std::min(MinY, World.y);
            MaxX = std::max(MaxX, World.x);
            MaxY = std::max(MaxY, World.y);
        }

        const int X0 = static_cast<int>(std::floor(MinX));
        const int Y0 = static_cast<int>(std::floor(MinY));
        const int X1 = static_cast<int>(std::ceil(MaxX));
        const int Y1 = static_cast<int>(std::ceil(MaxY));

        // Inverse mapping so rotated pieces don't come out full of holes
        for (int Y = Y0; Y <= Y1; ++Y)
        {
            for (int X = X0; X <= X1; ++X)
            {
                if (X < 0 || Y < 0 || X >= Game.Terrain.Width || Y >= Game.Terrain.Height)
                {
                    continue;
                }

                const Vector2 WorldPx = {.x = static_cast<float>(X) + 0.5f, .y = static_cast<float>(Y) + 0.5f};
                const Vector2 Local = ToPixels(b2InvTransformWorldPoint(Transform, ToMeters(WorldPx)));
                const int Lx = static_cast<int>(std::floor(Local.x + HalfW));
                const int Ly = static_cast<int>(std::floor(Local.y + HalfH));
                if (Lx < 0 || Ly < 0 || Lx >= D.Width || Ly >= D.Height)
                {
                    continue;
                }

                const size_t LocalIndex = static_cast<size_t>(Ly * D.Width + Lx);
                const size_t MapIndex = static_cast<size_t>(Y * Game.Terrain.Width + X);
                if (D.Pixels[LocalIndex].a == 0 || Game.Terrain.Solid[MapIndex])
                {
                    continue;
                }

                // Never bury a worm: one-sided chains would let it fall through from inside
                const bool Blocked = D.Solid[LocalIndex] && std::any_of(Game.Worms.begin(), Game.Worms.end(), [&](const Worm& W)
                {
                    return W.Alive && Vector2Distance(W.Position, WorldPx) < Worm::Radius + 1.f;
                });
                if (Blocked)
                {
                    continue;
                }

                SetTerrainPixel(Game.Terrain, X, Y, D.Pixels[LocalIndex], D.Solid[LocalIndex] != 0);
            }
        }

        MarkTerrainDirty(Game.Terrain, X0, Y0, X1, Y1);
    }

    // ---------- Damage ----------

    void DetachRope(Match& Game)
    {
        if (B2_IS_NON_NULL(Game.Rope))
        {
            b2DestroyJoint(Game.Rope);
            Game.Rope = b2_nullJointId;
        }
    }

    void EnterSettling(Match& Game)
    {
        DetachRope(Game);
        Game.Charging = false;
        Game.Charge = 0.f;
        Game.Phase = TurnPhase::Settling;
        Game.PhaseTimer = 0.f;

        if (Worm* Active = GetActiveWormMutable(Game); Active && Active->Motion == WormMotion::Roping)
        {
            Active->Motion = WormMotion::Airborne;
        }
    }

    void ApplyDamage(GameState& State, Worm& W, const int Damage)
    {
        if (Damage <= 0 || !W.Alive)
        {
            return;
        }

        W.HP = std::max(0, W.HP - Damage);
        W.FlashSec = Worm::FlashDuration;
        SpawnFloatingText(State.FloatingTexts, {.x = W.Position.x, .y = W.Position.y - 30.f}, TextFormat("-%d", Damage), State.Game.Teams[static_cast<size_t>(W.Team)].Tint, 30, 1.4f, true);

        // Getting hurt ends your turn
        Match& Game = State.Game;
        if (IsActiveWorm(Game, W) && (Game.Phase == TurnPhase::Aiming || Game.Phase == TurnPhase::Retreat))
        {
            EnterSettling(Game);
        }
    }

    void Knock(Match& Game, Worm& W, const Vector2 DeltaVelocityMps)
    {
        if (IsActiveWorm(Game, W) && W.Motion == WormMotion::Roping)
        {
            DetachRope(Game);
        }

        const b2Vec2 Current = b2Body_GetLinearVelocity(W.Body);
        b2Body_SetGravityScale(W.Body, 1.f);
        b2Body_SetLinearVelocity(W.Body, {Current.x + DeltaVelocityMps.x, Current.y + DeltaVelocityMps.y});
        W.Motion = WormMotion::Knocked;
        W.RestTimer = 0.f;
        W.FallSpeed = 0.f;
    }

    void Explode(GameState& State, const Vector2 Center, const ExplosionSpec& Spec)
    {
        Match& Game = State.Game;

        const Color Sample = CarveTerrain(Game.Terrain, Center, Spec.Radius);
        std::vector<TerrainPiece> Pieces = DetachFloatingPieces(Game.Terrain, Center, Spec.Radius, MaxDebrisPixels);
        for (TerrainPiece& Piece : Pieces)
        {
            CreateDebris(State, Piece);
        }

        for (Worm& W : Game.Worms)
        {
            if (!W.Alive)
            {
                continue;
            }

            const float Distance = Vector2Distance(Center, W.Position);
            const float Reach = Spec.Radius + Worm::Radius;
            if (Distance >= Reach)
            {
                continue;
            }

            const float Strength = std::clamp(1.f - std::max(0.f, Distance - Worm::Radius) / Spec.Radius, 0.f, 1.f);
            ApplyDamage(State, W, static_cast<int>(std::round(static_cast<float>(Spec.Damage) * Strength)));

            // Bias upward so worms get launched instead of pushed into the ground
            Vector2 Direction = Vector2Subtract(W.Position, Center);
            Direction.y -= 8.f;
            Direction = Vector2Length(Direction) > 0.01f ? Vector2Normalize(Direction) : Vector2{.x = 0.f, .y = -1.f};
            Knock(Game, W, Vector2Scale(Direction, Spec.Knock * (0.35f + 0.65f * Strength)));
        }

        b2ExplosionDef Blast = b2DefaultExplosionDef();
        Blast.maskBits = Category::Debris | Category::Projectile | Category::Grave;
        Blast.position = ToMeters(Center);
        Blast.radius = Spec.Radius * 1.6f / PixelsPerMeter;
        Blast.falloff = Spec.Radius / PixelsPerMeter;
        Blast.impulsePerLength = Spec.Knock * 0.8f;
        b2World_Explode(Game.World, &Blast);

        // Visuals scale with the blast
        const float Scale = Spec.Radius / 40.f;
        SpawnBurst(State.Particles, ParticleKind::Spark, Center, {.r = 255, .g = 214, .b = 90, .a = 255}, static_cast<int>(16.f * Scale), 80.f, 320.f, 3.f, 7.f, 0.35f);
        SpawnBurst(State.Particles, ParticleKind::Spark, Center, {.r = 255, .g = 120, .b = 40, .a = 255}, static_cast<int>(12.f * Scale), 40.f, 200.f, 5.f, 11.f, 0.45f);
        SpawnBurst(State.Particles, ParticleKind::Smoke, Center, {.r = 70, .g = 66, .b = 64, .a = 200}, static_cast<int>(10.f * Scale), 10.f, 60.f, 6.f, 12.f, 1.4f);
        if (Sample.a > 0)
        {
            SpawnBurst(State.Particles, ParticleKind::Dirt, Center, Sample, static_cast<int>(22.f * Scale), 120.f, 380.f, 1.5f, 3.5f, 1.2f);
        }

        // Flash: one big short-lived spark
        State.Particles.push_back({.Kind = ParticleKind::Spark, .Position = Center, .Velocity = {}, .Tint = {.r = 255, .g = 240, .b = 200, .a = 255}, .InitialRadius = Spec.Radius * 1.1f, .Lifetime = 0.12f, .MaxLifetime = 0.12f});

        State.ShakeScale = std::min(1.f, State.ShakeScale + 0.25f + Spec.Radius / 90.f);
        PlayExplosionSound(State, Spec.Radius);
    }

    // ---------- Turn flow ----------

    int CountAliveWorms(const Match& Game, const int TeamIndex)
    {
        return static_cast<int>(std::count_if(Game.Worms.begin(), Game.Worms.end(), [TeamIndex](const Worm& W) { return W.Alive && W.HP > 0 && W.Team == TeamIndex; }));
    }

    void BeginTurn(GameState& State)
    {
        Match& Game = State.Game;

        if (Game.TurnNumber > 0)
        {
            // Selection belongs to the worm that made it, not to whoever plays next
            Worm& Previous = Game.Worms[static_cast<size_t>(Game.CurrentWorm)];
            Previous.Weapon = Game.Weapon;
            Previous.FuseSeconds = Game.FuseSeconds;

            for (int Step = 1; Step <= TeamCount; ++Step)
            {
                const int Candidate = (Game.CurrentTeam + Step) % TeamCount;
                if (CountAliveWorms(Game, Candidate) > 0)
                {
                    Game.CurrentTeam = Candidate;
                    break;
                }
            }
        }

        Team& T = Game.Teams[static_cast<size_t>(Game.CurrentTeam)];
        for (int Step = 0; Step < WormsPerTeam; ++Step)
        {
            const int Slot = (T.NextWorm + Step) % WormsPerTeam;
            const int Index = Game.CurrentTeam * WormsPerTeam + Slot;
            if (Game.Worms[static_cast<size_t>(Index)].Alive)
            {
                Game.CurrentWorm = Index;
                T.NextWorm = (Slot + 1) % WormsPerTeam;
                break;
            }
        }

        std::uniform_real_distribution<float> WindRoll(-1.f, 1.f);
        Game.Wind = std::round(WindRoll(Game.Rng) * MaxWind * 4.f) / 4.f;

        Game.Phase = TurnPhase::Aiming;
        Game.TurnTimer = TurnDuration;
        Game.PhaseTimer = 0.f;
        Game.Charging = false;
        Game.Charge = 0.f;
        Game.ShotsLeft = 0;
        const Worm& Next = Game.Worms[static_cast<size_t>(Game.CurrentWorm)];
        Game.Weapon = T.Ammo[static_cast<size_t>(Next.Weapon)] == 0 ? WeaponKind::Rocket : Next.Weapon;
        Game.FuseSeconds = Next.FuseSeconds;
        ++Game.TurnNumber;

        State.Camera.Manual = false;
        State.WaitForMouseRelease = true;
        State.WeaponPanelOpen = false;
    }

    void FinishSettling(GameState& State)
    {
        Match& Game = State.Game;

        const bool AnyDying = std::any_of(Game.Worms.begin(), Game.Worms.end(), [](const Worm& W) { return W.Alive && W.HP <= 0; });
        if (AnyDying)
        {
            Game.Phase = TurnPhase::Deaths;
            Game.PhaseTimer = 0.f;
            return;
        }

        int TeamsAlive = 0;
        int LastTeam = -2;
        for (int t = 0; t < TeamCount; ++t)
        {
            if (CountAliveWorms(Game, t) > 0)
            {
                ++TeamsAlive;
                LastTeam = t;
            }
        }

        if (TeamsAlive <= 1)
        {
            State.Winner = LastTeam;
            State.Screen = GameScreen::Victory;
            return;
        }

        BeginTurn(State);
    }

    void KillWorm(GameState& State, Worm& W, const bool Drowned)
    {
        Match& Game = State.Game;
        if (IsActiveWorm(Game, W))
        {
            DetachRope(Game);
        }

        W.HP = 0;
        W.Alive = false;
        b2DestroyBody(W.Body);
        W.Body = b2_nullBodyId;

        const Color Tint = Game.Teams[static_cast<size_t>(W.Team)].Tint;
        if (Drowned)
        {
            SpawnSplash(State, W.Position);
            SpawnFloatingText(State.FloatingTexts, {.x = W.Position.x, .y = Game.LiquidLevel - 40.f}, TextFormat("%s drowned", W.Name), Tint, 22, 2.f, false);
        }
        else
        {
            CreateGrave(Game, W.Position, Tint);
        }

        if (IsActiveWorm(Game, W) && (Game.Phase == TurnPhase::Aiming || Game.Phase == TurnPhase::Retreat))
        {
            EnterSettling(Game);
        }
    }

    // ---------- Weapons ----------

    Vector2 GetAimDirection(const Worm& W)
    {
        return {.x = std::cos(W.AimAngle), .y = std::sin(W.AimAngle)};
    }

    void BeginRetreat(Match& Game)
    {
        Game.Phase = TurnPhase::Retreat;
        Game.PhaseTimer = RetreatDuration;
        Game.Charging = false;
        Game.Charge = 0.f;
    }

    void ConsumeAmmo(Match& Game)
    {
        int& Ammo = Game.Teams[static_cast<size_t>(Game.CurrentTeam)].Ammo[static_cast<size_t>(Game.Weapon)];
        if (Ammo > 0)
        {
            --Ammo;
        }
    }

    void FireCharged(GameState& State, Worm& W)
    {
        Match& Game = State.Game;
        const Vector2 Direction = GetAimDirection(W);
        const Vector2 Spawn = Vector2Add(W.Position, Vector2Scale(Direction, Worm::Radius + 9.f));
        const Vector2 Velocity = Vector2Scale(Direction, GetLaunchSpeed(Game.Weapon, Game.Charge));
        const float Fuse = static_cast<float>(Game.FuseSeconds);

        switch (Game.Weapon)
        {
            case WeaponKind::Rocket: CreateProjectile(Game, ProjectileKind::Rocket, Spawn, Velocity, 0.f); break;
            case WeaponKind::Grenade: CreateProjectile(Game, ProjectileKind::Grenade, Spawn, Velocity, Fuse); break;
            case WeaponKind::ClusterBomb: CreateProjectile(Game, ProjectileKind::ClusterBomb, Spawn, Velocity, Fuse); break;
            default: return;
        }

        SpawnBurst(State.Particles, ParticleKind::Smoke, Spawn, {.r = 200, .g = 200, .b = 200, .a = 160}, 6, 10.f, 50.f, 3.f, 6.f, 0.6f);
        Game.FollowProjectile = Game.Projectiles.back().Id;
        ConsumeAmmo(Game);
        BeginRetreat(Game);
        State.Camera.Manual = false;
    }

    void FireAirstrike(GameState& State, const Worm& W, const Vector2 Target)
    {
        Match& Game = State.Game;
        constexpr float DropY = -80.f;
        constexpr float DownSpeed = 6.f;
        const float SideSpeed = 2.f * W.FacingDir;

        // Lead the drop so bombs land around the clicked point
        const float FallMeters = std::max(0.f, (Target.y - DropY) / PixelsPerMeter);
        const float FallTime = (-DownSpeed + std::sqrt(DownSpeed * DownSpeed + 2.f * GravityMps2 * FallMeters)) / GravityMps2;
        const float Lead = SideSpeed * FallTime * PixelsPerMeter;

        for (int i = 0; i < 5; ++i)
        {
            const Vector2 Spawn = {.x = Target.x - Lead + static_cast<float>(i - 2) * 34.f, .y = DropY - static_cast<float>(i) * 10.f};
            CreateProjectile(Game, ProjectileKind::AirstrikeBomb, Spawn, {.x = SideSpeed, .y = DownSpeed}, 0.f);
        }

        Game.FollowProjectile = Game.Projectiles[Game.Projectiles.size() - 3].Id;
        ConsumeAmmo(Game);
        BeginRetreat(Game);
        State.Camera.Manual = false;
    }

    void FireTeleport(GameState& State, Worm& W, const Vector2 Target)
    {
        Match& Game = State.Game;
        const bool InBounds = Target.x > 20.f && Target.x < static_cast<float>(MapWidth) - 20.f && Target.y > -200.f && Target.y < Game.LiquidLevel - 30.f;
        const bool Clear = InBounds && IsCircleFree(Game.Terrain, Target, Worm::Radius + 2.f);
        if (!Clear)
        {
            SpawnFloatingText(State.FloatingTexts, Target, "Can't teleport there", RAYWHITE, 20, 1.2f, false);
            return;
        }

        SpawnBurst(State.Particles, ParticleKind::Spark, W.Position, {.r = 160, .g = 220, .b = 255, .a = 255}, 24, 40.f, 160.f, 2.f, 4.f, 0.6f);
        b2Body_SetTransform(W.Body, ToMeters(Target), b2MakeRot(0.f));
        b2Body_SetLinearVelocity(W.Body, {0.f, 0.f});
        b2Body_SetGravityScale(W.Body, 1.f);
        W.Position = W.PrevPosition = W.DrawPosition = Target;
        W.Motion = WormMotion::Airborne;
        W.FallSpeed = 0.f;
        SpawnBurst(State.Particles, ParticleKind::Spark, Target, {.r = 160, .g = 220, .b = 255, .a = 255}, 24, 40.f, 160.f, 2.f, 4.f, 0.6f);

        ConsumeAmmo(Game);
        BeginRetreat(Game);
    }

    void FireRope(GameState& State, Worm& W)
    {
        Match& Game = State.Game;
        if (B2_IS_NON_NULL(Game.Rope))
        {
            DetachRope(Game);
            W.Motion = WormMotion::Airborne;
            return;
        }

        b2QueryFilter Filter = MakeQueryFilter();
        Filter.maskBits = Category::Terrain;
        const Vector2 Direction = GetAimDirection(W);
        const b2RayResult Hit = b2World_CastRayClosest(Game.World, ToMeters(W.Position), ToMeters(Vector2Scale(Direction, RopeMaxLength)), Filter);
        if (!Hit.hit)
        {
            State.Tracers.push_back({.From = W.Position, .To = Vector2Add(W.Position, Vector2Scale(Direction, RopeMaxLength)), .Lifetime = 0.15f});
            return;
        }

        Game.RopeAnchor = ToPixels(Hit.point);
        const float Length = b2Distance(Hit.point, ToMeters(W.Position));

        b2DistanceJointDef Def = b2DefaultDistanceJointDef();
        Def.base.bodyIdA = Game.AnchorBody;
        Def.base.bodyIdB = W.Body;
        Def.base.localFrameA.p = Hit.point; // anchor body sits at the origin, so local == world
        Def.base.localFrameA.q = b2MakeRot(0.f);
        Def.base.localFrameB.q = b2MakeRot(0.f);
        Def.length = Length;
        // Spring on with zero stiffness turns the rod into a rope: free inside the limit, taut at the max
        Def.enableSpring = true;
        Def.hertz = 0.f;
        Def.enableLimit = true;
        Def.minLength = 0.f;
        Def.maxLength = Length;
        Game.Rope = b2CreateDistanceJoint(Game.World, &Def);

        b2Body_SetGravityScale(W.Body, 1.f);
        W.Motion = WormMotion::Roping;
    }

    void FireShotgun(GameState& State, Worm& W)
    {
        Match& Game = State.Game;
        if (Game.ShotsLeft == 0)
        {
            Game.ShotsLeft = 2;
        }
        --Game.ShotsLeft;

        constexpr float Range = 900.f;
        const Vector2 Direction = GetAimDirection(W);
        const Vector2 Start = Vector2Add(W.Position, Vector2Scale(Direction, Worm::Radius + 2.f));

        b2QueryFilter Filter = MakeQueryFilter();
        Filter.maskBits = Category::Terrain | Category::Debris | Category::Worm;
        const b2RayResult Hit = b2World_CastRayClosest(Game.World, ToMeters(Start), ToMeters(Vector2Scale(Direction, Range)), Filter);

        const Vector2 End = Hit.hit ? ToPixels(Hit.point) : Vector2Add(Start, Vector2Scale(Direction, Range));
        State.Tracers.push_back({.From = Start, .To = End, .Lifetime = 0.2f});
        SpawnBurst(State.Particles, ParticleKind::Spark, Start, {.r = 255, .g = 220, .b = 120, .a = 255}, 6, 30.f, 120.f, 2.f, 4.f, 0.15f);
        SetSoundPitch(State.Sounds.Explosion, 2.2f);
        PlaySfx(State.Sounds.Explosion);

        if (Hit.hit)
        {
            const void* Tag = b2Body_GetUserData(b2Shape_GetBody(Hit.shapeId));
            if (GetTagType(Tag) == EntityType::Worm)
            {
                if (Worm* Target = FindWorm(Game, GetTagId(Tag)))
                {
                    ApplyDamage(State, *Target, 25);
                    Knock(Game, *Target, Vector2Add(Vector2Scale(Direction, 6.f), {.x = 0.f, .y = -3.f}));
                }
            }
            Explode(State, End, ShotgunBlast);
        }

        if (Game.ShotsLeft == 0 && Game.Phase == TurnPhase::Aiming)
        {
            BeginRetreat(Game);
        }
    }

    // ---------- Input ----------

    bool CanUseWeapon(const Match& Game, const WeaponKind Kind)
    {
        return Game.Teams[static_cast<size_t>(Game.CurrentTeam)].Ammo[static_cast<size_t>(Kind)] != 0;
    }

    void CycleWeapon(Match& Game, const int Direction)
    {
        for (int Step = 1; Step <= WeaponCount; ++Step)
        {
            const WeaponKind Candidate = static_cast<WeaponKind>((static_cast<int>(Game.Weapon) + Direction * Step + WeaponCount * 2) % WeaponCount);
            if (CanUseWeapon(Game, Candidate))
            {
                Game.Weapon = Candidate;
                return;
            }
        }
    }

    // Returns true when the click was eaten by the panel
    bool HandleWeaponPanel(GameState& State)
    {
        Match& Game = State.Game;
        const bool WeaponLocked = Game.ShotsLeft > 0 || B2_IS_NON_NULL(Game.Rope) || Game.Charging;

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !WeaponLocked)
        {
            State.WeaponPanelOpen = !State.WeaponPanelOpen;
        }

        if (!WeaponLocked)
        {
            if (IsKeyPressed(KEY_Q))
            {
                CycleWeapon(Game, -1);
            }
            if (IsKeyPressed(KEY_E))
            {
                CycleWeapon(Game, 1);
            }
        }

        if (!State.WeaponPanelOpen || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            return State.WeaponPanelOpen;
        }

        for (int i = 0; i < WeaponCount; ++i)
        {
            if (CheckCollisionPointRec(GetMousePosition(), GetWeaponSlotRect(i)) && CanUseWeapon(Game, static_cast<WeaponKind>(i)))
            {
                Game.Weapon = static_cast<WeaponKind>(i);
                break;
            }
        }

        State.WeaponPanelOpen = false;
        State.WaitForMouseRelease = true;
        return true;
    }

    void HandleActiveWormInput(GameState& State, Worm& W, const Vector2 MouseWorld, const float DeltaTime)
    {
        Match& Game = State.Game;
        const bool CanAttack = Game.Phase == TurnPhase::Aiming;
        const bool CanMove = Game.Phase == TurnPhase::Aiming || Game.Phase == TurnPhase::Retreat;
        if (!CanMove)
        {
            W.MoveInput = 0.f;
            if (W.Motion == WormMotion::Walking)
            {
                W.Motion = WormMotion::Idle;
            }
            return;
        }

        const float Move = static_cast<float>((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1 : 0) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1 : 0));
        W.MoveInput = Game.Charging ? 0.f : Move;
        if (Move != 0.f)
        {
            State.Camera.Manual = false;
        }

        // Aim follows the mouse; the worm turns to face its aim unless it is walking
        W.AimAngle = std::atan2(MouseWorld.y - W.Position.y, MouseWorld.x - W.Position.x);
        const bool OnFoot = W.Motion == WormMotion::Idle || W.Motion == WormMotion::Walking;
        // Wedged worms are idle without ground under them and still need to act
        const bool Supported = W.Grounded || (W.Motion == WormMotion::Idle && Vector2Length(W.Velocity) < 8.f);
        if (W.Motion == WormMotion::Walking && W.MoveInput != 0.f)
        {
            W.FacingDir = W.MoveInput;
        }
        else if (OnFoot || W.Motion == WormMotion::Roping)
        {
            W.FacingDir = std::cos(W.AimAngle) >= 0.f ? 1.f : -1.f;
        }

        if (W.Motion == WormMotion::Roping)
        {
            // Climb and descend the rope
            const float Climb = static_cast<float>((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? 1 : 0) - (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1 : 0));
            if (Climb != 0.f && B2_IS_NON_NULL(Game.Rope))
            {
                const float Length = std::clamp(b2DistanceJoint_GetUpperLimit(Game.Rope) + Climb * 4.f * DeltaTime, 0.8f, RopeMaxLength / PixelsPerMeter);
                b2DistanceJoint_SetLimits(Game.Rope, 0.f, Length);
                b2DistanceJoint_SetRestLength(Game.Rope, Length);
            }

            if (IsKeyPressed(KEY_SPACE))
            {
                DetachRope(Game);
                W.Motion = WormMotion::Airborne;
                const b2Vec2 V = b2Body_GetLinearVelocity(W.Body);
                b2Body_SetLinearVelocity(W.Body, {V.x, V.y - 3.f});
            }
        }
        else if (OnFoot && Supported && !Game.Charging)
        {
            if (IsKeyPressed(KEY_SPACE))
            {
                b2Body_SetGravityScale(W.Body, 1.f);
                b2Body_SetLinearVelocity(W.Body, {W.FacingDir * 3.f, -7.f});
                W.Motion = WormMotion::Airborne;
                W.FallSpeed = 0.f;
            }
            else if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL))
            {
                b2Body_SetGravityScale(W.Body, 1.f);
                b2Body_SetLinearVelocity(W.Body, {-W.FacingDir * 1.2f, -9.5f});
                W.Motion = WormMotion::Airborne;
                W.FallSpeed = 0.f;
            }
            else
            {
                W.Motion = W.MoveInput != 0.f ? WormMotion::Walking : WormMotion::Idle;
            }
        }

        if (!CanAttack)
        {
            return;
        }

        for (int Key = KEY_ONE; Key <= KEY_FIVE; ++Key)
        {
            if (IsKeyPressed(Key))
            {
                Game.FuseSeconds = Key - KEY_ONE + 1;
            }
        }

        if (HandleWeaponPanel(State) || State.WaitForMouseRelease || !CanUseWeapon(Game, Game.Weapon))
        {
            return;
        }

        const WeaponInfo& Info = GetWeaponInfo(Game.Weapon);
        const bool Steady = OnFoot && Supported;

        switch (Info.Mode)
        {
            case AimMode::Charge:
            {
                if (!Steady)
                {
                    Game.Charging = false;
                    Game.Charge = 0.f;
                    break;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    Game.Charging = true;
                    Game.Charge = 0.f;
                }
                if (Game.Charging)
                {
                    Game.Charge = std::min(1.f, Game.Charge + ChargeRatePerSec * DeltaTime);
                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || Game.Charge >= 1.f)
                    {
                        FireCharged(State, W);
                    }
                }
                break;
            }
            case AimMode::Instant:
            {
                if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    break;
                }
                if (Game.Weapon == WeaponKind::NinjaRope && W.Motion != WormMotion::Knocked)
                {
                    FireRope(State, W);
                }
                else if (Game.Weapon == WeaponKind::Shotgun && Steady)
                {
                    FireShotgun(State, W);
                }
                break;
            }
            case AimMode::Target:
            {
                if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !Steady)
                {
                    break;
                }
                if (Game.Weapon == WeaponKind::Airstrike)
                {
                    FireAirstrike(State, W, MouseWorld);
                }
                else if (Game.Weapon == WeaponKind::Teleport)
                {
                    FireTeleport(State, W, MouseWorld);
                }
                break;
            }
        }
    }

    // ---------- Physics ----------

    bool CheckGrounded(const Match& Game, const Worm& W)
    {
        b2QueryFilter Filter = MakeQueryFilter();
        Filter.maskBits = Category::Terrain | Category::Debris;

        // Three rays so a worm standing on a ledge edge still counts as grounded
        for (const float Offset : {0.f, -Worm::Radius * 0.6f, Worm::Radius * 0.6f})
        {
            const Vector2 Origin = {.x = W.Position.x + Offset, .y = W.Position.y};
            const b2RayResult Hit = b2World_CastRayClosest(Game.World, ToMeters(Origin), ToMeters({.x = 0.f, .y = Worm::Radius + 3.f}), Filter);
            if (Hit.hit && Hit.normal.y < -0.45f)
            {
                return true;
            }
        }
        return false;
    }

    float LerpAngle(const float From, const float To, const float T)
    {
        return From + std::remainder(To - From, 2.f * PI) * T;
    }

    void SnapshotTransforms(Match& Game)
    {
        for (Worm& W : Game.Worms)
        {
            W.PrevPosition = W.Position;
        }
        for (Projectile& P : Game.Projectiles)
        {
            P.PrevPosition = P.Position;
            P.PrevAngle = P.Angle;
        }
        for (Debris& D : Game.DebrisPieces)
        {
            D.PrevPosition = D.Position;
            D.PrevAngle = D.Angle;
        }
        for (Grave& G : Game.Graves)
        {
            G.PrevPosition = G.Position;
            G.PrevAngle = G.Angle;
        }
    }

    // Worms are read in PostStep along with their ground checks
    void ReadBodyTransforms(Match& Game)
    {
        for (Projectile& P : Game.Projectiles)
        {
            P.Position = ToPixels(b2Body_GetPosition(P.Body));
            const b2Vec2 V = b2Body_GetLinearVelocity(P.Body);
            P.Angle = P.Fuse > 0.f ? b2Rot_GetAngle(b2Body_GetRotation(P.Body)) : std::atan2(V.y, V.x);
        }
        for (Debris& D : Game.DebrisPieces)
        {
            D.Position = ToPixels(b2Body_GetPosition(D.Body));
            D.Angle = b2Rot_GetAngle(b2Body_GetRotation(D.Body));
        }
        for (Grave& G : Game.Graves)
        {
            G.Position = ToPixels(b2Body_GetPosition(G.Body));
            G.Angle = b2Rot_GetAngle(b2Body_GetRotation(G.Body));
        }
    }

    void UpdateDrawTransforms(Match& Game, const float Alpha)
    {
        for (Worm& W : Game.Worms)
        {
            W.DrawPosition = Vector2Lerp(W.PrevPosition, W.Position, Alpha);
        }
        for (Projectile& P : Game.Projectiles)
        {
            P.DrawPosition = Vector2Lerp(P.PrevPosition, P.Position, Alpha);
            P.DrawAngle = LerpAngle(P.PrevAngle, P.Angle, Alpha);
        }
        for (Debris& D : Game.DebrisPieces)
        {
            D.DrawPosition = Vector2Lerp(D.PrevPosition, D.Position, Alpha);
            D.DrawAngle = LerpAngle(D.PrevAngle, D.Angle, Alpha);
        }
        for (Grave& G : Game.Graves)
        {
            G.DrawPosition = Vector2Lerp(G.PrevPosition, G.Position, Alpha);
            G.DrawAngle = LerpAngle(G.PrevAngle, G.Angle, Alpha);
        }
    }

    void PreStep(Match& Game)
    {
        for (Worm& W : Game.Worms)
        {
            if (!W.Alive)
            {
                continue;
            }

            const b2Vec2 V = b2Body_GetLinearVelocity(W.Body);
            switch (W.Motion)
            {
                case WormMotion::Idle:
                    if (W.Grounded)
                    {
                        // Pinned: no gravity means no creeping down slopes between turns
                        b2Body_SetGravityScale(W.Body, 0.f);
                        b2Body_SetLinearVelocity(W.Body, {0.f, 0.f});
                    }
                    else
                    {
                        // Wedged in a crevice too steep to count as ground: stay put, but drop if it starts moving
                        b2Body_SetGravityScale(W.Body, 1.f);
                        if (b2Length(V) > 0.5f)
                        {
                            W.Motion = WormMotion::Airborne;
                            W.FallSpeed = 0.f;
                        }
                    }
                    break;
                case WormMotion::Walking:
                    b2Body_SetGravityScale(W.Body, 1.f);
                    // A little downward push keeps the worm hugging the ground on the way down a slope
                    b2Body_SetLinearVelocity(W.Body, {W.MoveInput * WalkSpeed, std::max(V.y, 0.5f)});
                    break;
                case WormMotion::Roping:
                {
                    const float Mass = b2Body_GetMass(W.Body);
                    b2Body_ApplyForceToCenter(W.Body, {W.MoveInput * Mass * 12.f, 0.f}, true);
                    break;
                }
                default:
                    b2Body_SetGravityScale(W.Body, 1.f);
                    break;
            }
        }

        for (const Projectile& P : Game.Projectiles)
        {
            if (P.UsesWind)
            {
                b2Body_ApplyForceToCenter(P.Body, {b2Body_GetMass(P.Body) * Game.Wind, 0.f}, true);
            }
        }
    }

    void PostStep(GameState& State, const float StepTime)
    {
        Match& Game = State.Game;

        for (Worm& W : Game.Worms)
        {
            if (!W.Alive)
            {
                continue;
            }

            W.Position = ToPixels(b2Body_GetPosition(W.Body));
            const b2Vec2 V = b2Body_GetLinearVelocity(W.Body);
            W.Velocity = ToPixels(V);
            W.Grounded = CheckGrounded(Game, W);

            if (W.Motion == WormMotion::Walking && !W.Grounded)
            {
                W.RestTimer += StepTime;
                if (W.RestTimer > 0.1f)
                {
                    W.Motion = WormMotion::Airborne;
                    W.FallSpeed = 0.f;
                }
                continue;
            }

            if (W.Motion != WormMotion::Airborne && W.Motion != WormMotion::Knocked)
            {
                W.RestTimer = 0.f;
                continue;
            }

            W.FallSpeed = std::max(W.FallSpeed, V.y);
            W.SpinAngle += (W.Motion == WormMotion::Knocked ? V.x * 0.6f : 0.f) * StepTime * 10.f;

            // Sustained near-zero speed also counts as landed, so a worm wedged on a steep wall doesn't stall the turn
            W.RestTimer = b2Length(V) < 0.2f ? W.RestTimer + StepTime : 0.f;
            const bool Stuck = W.RestTimer > 0.3f;

            if (W.Motion == WormMotion::Airborne && ((W.Grounded && V.y > -0.5f) || Stuck))
            {
                if (W.FallSpeed > FallDamageSpeed)
                {
                    ApplyDamage(State, W, static_cast<int>((W.FallSpeed - FallDamageSpeed) * 6.f));
                }
                W.FallSpeed = 0.f;
                W.Motion = WormMotion::Idle;
                W.SpinAngle = 0.f;
                W.RestTimer = 0.f;
            }
            else if (W.Motion == WormMotion::Knocked)
            {
                W.SettleTimer = (W.Grounded && b2Length(V) < 0.8f) || Stuck ? W.SettleTimer + StepTime : 0.f;
                if (W.SettleTimer > 0.2f)
                {
                    W.SettleTimer = 0.f;
                    W.RestTimer = 0.f;
                    W.Motion = WormMotion::Idle;
                    W.SpinAngle = 0.f;
                }
            }
        }
    }

    void HandleContactEvents(GameState& State)
    {
        Match& Game = State.Game;
        const b2ContactEvents Events = b2World_GetContactEvents(Game.World);

        for (int i = 0; i < Events.beginCount; ++i)
        {
            const b2ContactBeginTouchEvent& Event = Events.beginEvents[i];
            for (const b2ShapeId Shape : {Event.shapeIdA, Event.shapeIdB})
            {
                const void* Tag = b2Body_GetUserData(b2Shape_GetBody(Shape));
                if (GetTagType(Tag) != EntityType::Projectile)
                {
                    continue;
                }
                if (Projectile* P = FindProjectile(Game, GetTagId(Tag)); P && P->Fuse <= 0.f)
                {
                    P->Detonate = true;
                }
            }
        }

        // Falling debris hurts
        for (int i = 0; i < Events.hitCount; ++i)
        {
            const b2ContactHitEvent& Event = Events.hitEvents[i];
            if (Event.approachSpeed < 5.f)
            {
                continue;
            }
            for (const b2ShapeId Shape : {Event.shapeIdA, Event.shapeIdB})
            {
                const void* Tag = b2Body_GetUserData(b2Shape_GetBody(Shape));
                if (GetTagType(Tag) != EntityType::Worm)
                {
                    continue;
                }
                if (Worm* W = FindWorm(Game, GetTagId(Tag)))
                {
                    ApplyDamage(State, *W, std::min(25, static_cast<int>((Event.approachSpeed - 5.f) * 3.f)));
                    if (W->Motion == WormMotion::Idle || W->Motion == WormMotion::Walking)
                    {
                        Knock(Game, *W, {.x = 0.f, .y = 0.f});
                    }
                }
            }
        }
    }

    void DetonateProjectile(GameState& State, const Projectile& P)
    {
        Match& Game = State.Game;
        switch (P.Kind)
        {
            case ProjectileKind::Rocket: Explode(State, P.Position, RocketBlast); break;
            case ProjectileKind::Grenade: Explode(State, P.Position, GrenadeBlast); break;
            case ProjectileKind::Bomblet: Explode(State, P.Position, BombletBlast); break;
            case ProjectileKind::AirstrikeBomb: Explode(State, P.Position, AirstrikeBlast); break;
            case ProjectileKind::ClusterBomb:
            {
                Explode(State, P.Position, ClusterBlast);
                std::uniform_real_distribution<float> Spread(-0.9f, 0.9f);
                std::uniform_real_distribution<float> Speed(5.f, 8.f);
                for (int i = 0; i < 5; ++i)
                {
                    const float Angle = -PI * 0.5f + Spread(Game.Rng);
                    const float S = Speed(Game.Rng);
                    CreateProjectile(Game, ProjectileKind::Bomblet, Vector2Add(P.Position, {.x = 0.f, .y = -6.f}), {.x = std::cos(Angle) * S, .y = std::sin(Angle) * S}, 0.f);
                }
                Game.FollowProjectile = Game.Projectiles.back().Id;
                break;
            }
        }
    }

    void UpdateProjectiles(GameState& State, const float DeltaTime)
    {
        Match& Game = State.Game;

        // Iterate by index: detonations can spawn new projectiles
        for (size_t i = 0; i < Game.Projectiles.size(); ++i)
        {
            Projectile& P = Game.Projectiles[i];
            P.Position = ToPixels(b2Body_GetPosition(P.Body));
            const b2Vec2 V = b2Body_GetLinearVelocity(P.Body);
            P.Velocity = ToPixels(V);
            P.Angle = P.Fuse > 0.f ? b2Rot_GetAngle(b2Body_GetRotation(P.Body)) : std::atan2(V.y, V.x);

            if (P.Fuse > 0.f)
            {
                P.Fuse -= DeltaTime;
                if (P.Fuse <= 0.f)
                {
                    P.Detonate = true;
                }
            }

            if (P.Kind == ProjectileKind::Rocket || P.Kind == ProjectileKind::AirstrikeBomb)
            {
                // Trail starts at the tail so it never covers the projectile
                const Vector2 Tail = Vector2Subtract(P.DrawPosition, Vector2Scale({.x = std::cos(P.DrawAngle), .y = std::sin(P.DrawAngle)}, 10.f));
                State.Particles.push_back({.Kind = ParticleKind::Smoke, .Position = Tail, .Velocity = {.x = GetRandomFloatValue(-10.f, 10.f), .y = GetRandomFloatValue(-10.f, 10.f)}, .Tint = {.r = 220, .g = 220, .b = 220, .a = 120}, .InitialRadius = 3.f, .Lifetime = 0.5f, .MaxLifetime = 0.5f});
            }
        }

        for (size_t i = 0; i < Game.Projectiles.size(); ++i)
        {
            const Projectile P = Game.Projectiles[i];
            const bool Sunk = P.Position.y > Game.LiquidLevel + 4.f;
            const bool Lost = P.Position.x < -300.f || P.Position.x > static_cast<float>(MapWidth) + 300.f;
            if (!P.Detonate && !Sunk && !Lost)
            {
                continue;
            }

            b2DestroyBody(P.Body);
            Game.Projectiles.erase(Game.Projectiles.begin() + static_cast<std::ptrdiff_t>(i));
            --i;

            if (Sunk)
            {
                SpawnSplash(State, P.Position);
            }
            else if (P.Detonate)
            {
                DetonateProjectile(State, P);
            }
        }
    }

    void UpdateDebris(GameState& State, const float DeltaTime, const bool ForceSettle)
    {
        Match& Game = State.Game;
        for (size_t i = 0; i < Game.DebrisPieces.size(); ++i)
        {
            Debris& D = Game.DebrisPieces[i];
            D.Age += DeltaTime;
            D.Position = ToPixels(b2Body_GetPosition(D.Body));
            D.Angle = b2Rot_GetAngle(b2Body_GetRotation(D.Body));

            const float Speed = b2Length(b2Body_GetLinearVelocity(D.Body)) + std::abs(b2Body_GetAngularVelocity(D.Body)) * 0.5f;
            D.RestTimer = Speed < 0.15f ? D.RestTimer + DeltaTime : 0.f;

            const bool Sunk = D.Position.y - static_cast<float>(std::max(D.Width, D.Height)) * 0.5f > Game.LiquidLevel;
            const bool Settled = D.RestTimer > 0.4f || !b2Body_IsAwake(D.Body) || D.Age > 12.f || ForceSettle;
            if (!Sunk && !Settled)
            {
                continue;
            }

            if (Sunk)
            {
                SpawnSplash(State, D.Position);
            }
            else
            {
                RestampDebris(Game, D);
            }

            DestroyDebris(D);
            Game.DebrisPieces.erase(Game.DebrisPieces.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        }
    }

    void UpdateGravesAndDrowning(GameState& State)
    {
        Match& Game = State.Game;

        for (Worm& W : Game.Worms)
        {
            if (W.Alive && W.Position.y > Game.LiquidLevel + 4.f)
            {
                KillWorm(State, W, true);
            }
        }

        for (size_t i = 0; i < Game.Graves.size(); ++i)
        {
            Grave& G = Game.Graves[i];
            G.Position = ToPixels(b2Body_GetPosition(G.Body));
            G.Angle = b2Rot_GetAngle(b2Body_GetRotation(G.Body));
            if (G.Position.y > Game.LiquidLevel + 10.f)
            {
                b2DestroyBody(G.Body);
                Game.Graves.erase(Game.Graves.begin() + static_cast<std::ptrdiff_t>(i));
                --i;
            }
        }
    }

    // ---------- Camera ----------

    void UpdateCamera(GameState& State, const float DeltaTime)
    {
        Match& Game = State.Game;
        GameCamera& Cam = State.Camera;

        const float Wheel = GetMouseWheelMove();
        if (Wheel != 0.f)
        {
            Cam.Zoom = std::clamp(Cam.Zoom * std::pow(1.12f, Wheel), 0.45f, 2.5f);
        }

        const Camera2D View = GetWorldCamera(State);
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            const Vector2 Delta = GetMouseDelta();
            Cam.Target = Vector2Subtract(Cam.Target, Vector2Scale(Delta, 1.f / View.zoom));
            Cam.Manual = true;
        }

        // Follow the most interesting thing: a live projectile, a flying worm, else the active worm
        bool HasFocus = false;
        if (const Projectile* P = FindProjectile(Game, Game.FollowProjectile))
        {
            Cam.Focus = P->DrawPosition;
            HasFocus = true;
        }
        else if (!Game.Projectiles.empty())
        {
            Game.FollowProjectile = Game.Projectiles.back().Id;
        }

        if (!HasFocus && Game.Phase == TurnPhase::Settling)
        {
            for (const Worm& W : Game.Worms)
            {
                if (W.Alive && W.Motion == WormMotion::Knocked)
                {
                    Cam.Focus = W.DrawPosition;
                    HasFocus = true;
                    break;
                }
            }
        }

        if (!HasFocus)
        {
            if (const Worm* Active = GetActiveWorm(Game))
            {
                Cam.Focus = Active->DrawPosition;
            }
        }

        if (!Cam.Manual)
        {
            const float Follow = 1.f - std::exp(-DeltaTime * 4.f);
            Cam.Target = Vector2Lerp(Cam.Target, Cam.Focus, Follow);
        }

        // Keep the view over the map; extra headroom above for airstrikes and rope swings
        const float HalfW = static_cast<float>(GetScreenWidth()) * 0.5f / View.zoom;
        const float HalfH = static_cast<float>(GetScreenHeight()) * 0.5f / View.zoom;
        const float MinX = HalfW - 100.f;
        const float MaxX = static_cast<float>(MapWidth) + 100.f - HalfW;
        Cam.Target.x = MinX < MaxX ? std::clamp(Cam.Target.x, MinX, MaxX) : static_cast<float>(MapWidth) * 0.5f;
        const float MinY = HalfH - 350.f;
        const float MaxY = Game.LiquidLevel + 140.f - HalfH;
        Cam.Target.y = MinY < MaxY ? std::clamp(Cam.Target.y, MinY, MaxY) : (MinY + MaxY) * 0.5f;
    }
}

const WeaponInfo& GetWeaponInfo(const WeaponKind Kind)
{
    return Weapons[static_cast<size_t>(Kind)];
}

float GetLaunchSpeed(const WeaponKind Weapon, const float Charge)
{
    if (Weapon == WeaponKind::Rocket)
    {
        return Lerp(6.f, 36.f, Charge);
    }
    return Lerp(4.f, 22.f, Charge);
}

float GetUiScale()
{
    return static_cast<float>(GetScreenHeight()) / 1080.f;
}

Rectangle GetWeaponSlotRect(const int Index)
{
    const float Ui = GetUiScale();
    const float Size = 76.f * Ui;
    const float Gap = 8.f * Ui;
    constexpr int Columns = 4;
    const float PanelW = Columns * Size + (Columns - 1) * Gap;
    const float X0 = static_cast<float>(GetScreenWidth()) - PanelW - 28.f * Ui;
    const float Y0 = static_cast<float>(GetScreenHeight()) - 2.f * Size - Gap - 150.f * Ui;
    return {
        .x = X0 + static_cast<float>(Index % Columns) * (Size + Gap),
        .y = Y0 + static_cast<float>(Index / Columns) * (Size + Gap),
        .width = Size,
        .height = Size
    };
}

Rectangle GetMenuButtonRect(const int Index)
{
    const float Ui = GetUiScale();
    const float W = 420.f * Ui;
    const float H = 64.f * Ui;
    const float X = (static_cast<float>(GetScreenWidth()) - W) * 0.5f;
    const float Y = static_cast<float>(GetScreenHeight()) * 0.5f + static_cast<float>(Index) * (H + 18.f * Ui);
    return {.x = X, .y = Y, .width = W, .height = H};
}

const char* GetMenuBiomeName(const int MenuBiome)
{
    return MenuBiome < 0 ? "Random" : GetBiomeTheme(static_cast<BiomeKind>(MenuBiome)).Name;
}

const Worm* GetActiveWorm(const Match& Game)
{
    if (Game.CurrentWorm < 0 || Game.CurrentWorm >= static_cast<int>(Game.Worms.size()))
    {
        return nullptr;
    }
    const Worm& W = Game.Worms[static_cast<size_t>(Game.CurrentWorm)];
    return W.Alive ? &W : nullptr;
}

Camera2D GetWorldCamera(const GameState& State)
{
    return {
        .offset = {.x = static_cast<float>(GetScreenWidth()) * 0.5f, .y = static_cast<float>(GetScreenHeight()) * 0.5f},
        .target = State.Camera.Target,
        .rotation = 0.f,
        .zoom = static_cast<float>(GetScreenHeight()) / ViewWorldHeight * State.Camera.Zoom
    };
}

Rectangle GetVisibleWorld(const Camera2D& View)
{
    const Vector2 TopLeft = GetScreenToWorld2D({.x = 0.f, .y = 0.f}, View);
    const Vector2 BottomRight = GetScreenToWorld2D({.x = static_cast<float>(GetScreenWidth()), .y = static_cast<float>(GetScreenHeight())}, View);
    return {.x = TopLeft.x, .y = TopLeft.y, .width = BottomRight.x - TopLeft.x, .height = BottomRight.y - TopLeft.y};
}

void SpawnFloatingText(std::vector<FloatingText>& Texts, const Vector2 Position, const char* Text, const Color Tint, const int FontSize, const float Lifetime, const bool Shake)
{
    FloatingText T = {.Position = Position, .Text = {}, .Tint = Tint, .FontSize = FontSize, .Lifetime = Lifetime, .MaxLifetime = Lifetime, .Shake = Shake};

    const int WrittenBytes = snprintf(T.Text, sizeof(T.Text), "%s", Text);

    // Ref: https://www.sandordargo.com/blog/2023/10/11/cpp20-intcmp-utilities
    if (WrittenBytes < 0 || std::cmp_greater_equal(WrittenBytes, sizeof(T.Text)))
    {
        TraceLog(LOG_WARNING, "SpawnFloatingText: text '%s' truncated to %d chars", Text, static_cast<int>(sizeof(T.Text) - 1));
    }

    Texts.push_back(T);
}

void DestroyMatch(Match& Game)
{
    for (Debris& D : Game.DebrisPieces)
    {
        if (D.Texture.id != 0)
        {
            UnloadTexture(D.Texture);
        }
    }

    // Destroying the world frees every body, shape, chain and joint in it
    if (B2_IS_NON_NULL(Game.World))
    {
        b2DestroyWorld(Game.World);
    }

    UnloadTerrain(Game.Terrain);
    Game.World = b2_nullWorldId;
    Game.AnchorBody = b2_nullBodyId;
    Game.Rope = b2_nullJointId;
    Game.Worms.clear();
    Game.Projectiles.clear();
    Game.DebrisPieces.clear();
    Game.Graves.clear();
}

void StartMatch(GameState& State, const std::uint32_t Seed, const BiomeKind Biome)
{
    Match& Game = State.Game;
    DestroyMatch(Game);

    Game.Rng.seed(Seed);
    Game.Biome = Biome;
    Game.LiquidLevel = static_cast<float>(MapHeight) - 110.f;
    Game.NextId = 1;
    Game.TurnNumber = 0;
    Game.PhysicsAccumulator = 0.f;
    Game.Time = 0.f;
    Game.FollowProjectile = 0;
    Game.Weapon = WeaponKind::Rocket;
    Game.FuseSeconds = 3;

    b2WorldDef WorldDef = b2DefaultWorldDef();
    WorldDef.gravity = {0.f, GravityMps2};
    WorldDef.hitEventThreshold = 4.f;
    Game.World = b2CreateWorld(&WorldDef);

    b2BodyDef AnchorDef = b2DefaultBodyDef();
    AnchorDef.type = b2_staticBody;
    Game.AnchorBody = b2CreateBody(Game.World, &AnchorDef);

    InitTerrain(Game.Terrain, MapWidth, MapHeight);
    GenerateBiomeTerrain(Game.Terrain, Biome, Seed, Game.LiquidLevel);
    GenerateBackdrop(Game.Background, Seed);
    SyncTerrain(Game.Terrain, Game.World);

    const std::array<const char*, TeamCount> TeamNames = {"Red Rascals", "Blue Bandits"};
    const std::array<Color, TeamCount> TeamColors = {Color{.r = 236, .g = 72, .b = 72, .a = 255}, Color{.r = 72, .g = 150, .b = 246, .a = 255}};
    for (int t = 0; t < TeamCount; ++t)
    {
        Team& T = Game.Teams[static_cast<size_t>(t)];
        snprintf(T.Name, sizeof(T.Name), "%s", TeamNames[static_cast<size_t>(t)]);
        T.Tint = TeamColors[static_cast<size_t>(t)];
        T.NextWorm = 0;
        for (int w = 0; w < WeaponCount; ++w)
        {
            T.Ammo[static_cast<size_t>(w)] = Weapons[static_cast<size_t>(w)].StartAmmo;
        }
    }

    std::array<const char*, WormNames.size()> Names = WormNames;
    std::shuffle(Names.begin(), Names.end(), Game.Rng);

    // Interleave teams across spawn points so neither side gets one corner
    std::vector<Vector2> Spawns = FindSpawnPoints(Game.Terrain, TeamCount * WormsPerTeam, 170.f, 30.f, Game.LiquidLevel, Game.Rng);
    for (int t = 0; t < TeamCount; ++t)
    {
        for (int w = 0; w < WormsPerTeam; ++w)
        {
            const Vector2 Ground = Spawns[static_cast<size_t>(w * TeamCount + t)];
            CreateWorm(Game, {.x = Ground.x, .y = Ground.y - Worm::Radius - 1.f}, t, Names[static_cast<size_t>(t * WormsPerTeam + w)]);
        }
    }

    std::uniform_int_distribution<int> FirstTeam(0, TeamCount - 1);
    Game.CurrentTeam = FirstTeam(Game.Rng);
    BeginTurn(State);

    State.Particles.clear();
    State.Tracers.clear();
    State.FloatingTexts.clear();
    State.ShakeScale = 0.f;
    State.Winner = -1;
    State.Screen = GameScreen::Game;
    State.Camera.Zoom = 1.f;
    State.Camera.Manual = false;
    if (const Worm* Active = GetActiveWorm(Game))
    {
        State.Camera.Target = Active->Position;
    }

    TraceLog(LOG_INFO, "StartMatch: %s biome, seed %u", GetBiomeTheme(Biome).Name, Seed);
}

void UpdateGameplay(GameState& State, const float DeltaTime)
{
    Match& Game = State.Game;
    Game.Time += DeltaTime;

    if (State.WaitForMouseRelease && IsMouseButtonUp(MOUSE_BUTTON_LEFT))
    {
        State.WaitForMouseRelease = false;
    }

    const Vector2 MouseWorld = GetScreenToWorld2D(GetMousePosition(), GetWorldCamera(State));
    if (Worm* Active = GetActiveWormMutable(Game))
    {
        HandleActiveWormInput(State, *Active, MouseWorld, DeltaTime);
    }
    else
    {
        State.WeaponPanelOpen = false;
    }

    // Fixed step keeps the simulation stable and identical regardless of frame rate
    Game.PhysicsAccumulator = std::min(Game.PhysicsAccumulator + DeltaTime, PhysicsStep * 6.f);
    while (Game.PhysicsAccumulator >= PhysicsStep)
    {
        SnapshotTransforms(Game);
        PreStep(Game);
        b2World_Step(Game.World, PhysicsStep, PhysicsSubSteps);
        ReadBodyTransforms(Game);
        HandleContactEvents(State);
        PostStep(State, PhysicsStep);
        Game.PhysicsAccumulator -= PhysicsStep;
    }

    // Render between the last two physics states; otherwise a 60 Hz sim stutters on faster monitors
    UpdateDrawTransforms(Game, Game.PhysicsAccumulator / PhysicsStep);

    UpdateProjectiles(State, DeltaTime);
    UpdateGravesAndDrowning(State);

    switch (Game.Phase)
    {
        case TurnPhase::Aiming:
            Game.TurnTimer -= DeltaTime;
            if (Game.TurnTimer <= 0.f)
            {
                EnterSettling(Game);
            }
            break;
        case TurnPhase::Retreat:
            Game.PhaseTimer -= DeltaTime;
            if (Game.PhaseTimer <= 0.f)
            {
                EnterSettling(Game);
            }
            break;
        case TurnPhase::Settling:
        {
            Game.PhaseTimer += DeltaTime;
            const bool WormsBusy = std::any_of(Game.Worms.begin(), Game.Worms.end(), [](const Worm& W)
            {
                return W.Alive && (W.Motion == WormMotion::Airborne || W.Motion == WormMotion::Knocked || W.Motion == WormMotion::Roping);
            });
            const bool Busy = WormsBusy || !Game.Projectiles.empty() || !Game.DebrisPieces.empty();
            const bool TimedOut = Game.PhaseTimer > SettleMaxDuration;
            if (TimedOut)
            {
                UpdateDebris(State, 0.f, true);
            }
            if ((!Busy && Game.PhaseTimer > SettleMinDuration) || TimedOut)
            {
                FinishSettling(State);
            }
            break;
        }
        case TurnPhase::Deaths:
            Game.PhaseTimer += DeltaTime;
            if (Game.PhaseTimer >= DeathInterval)
            {
                for (Worm& W : Game.Worms)
                {
                    if (W.Alive && W.HP <= 0)
                    {
                        const Vector2 At = W.Position;
                        KillWorm(State, W, false);
                        Explode(State, At, DeathBlast);
                        State.Camera.Focus = At;
                        break;
                    }
                }
                Game.Phase = TurnPhase::Settling;
                Game.PhaseTimer = 0.f;
            }
            break;
    }

    UpdateDebris(State, DeltaTime, false);
    SyncTerrain(Game.Terrain, Game.World);
    UpdateCamera(State, DeltaTime);
}

void UpdateEffects(GameState& State, const float DeltaTime)
{
    for (Particle& P : State.Particles)
    {
        switch (P.Kind)
        {
            case ParticleKind::Dirt:
                P.Velocity.y += 700.f * DeltaTime;
                break;
            case ParticleKind::Smoke:
                P.Velocity = Vector2Scale(P.Velocity, 1.f - 1.5f * DeltaTime);
                P.Velocity.y -= 30.f * DeltaTime;
                P.Velocity.x += State.Game.Wind * 6.f * DeltaTime;
                break;
            case ParticleKind::Spark:
                P.Velocity = Vector2Scale(P.Velocity, 1.f - 3.f * DeltaTime);
                break;
        }
        P.Position = Vector2Add(P.Position, Vector2Scale(P.Velocity, DeltaTime));
        P.Lifetime -= DeltaTime;
    }
    std::erase_if(State.Particles, [](const Particle& P) { return P.Lifetime <= 0.f; });

    for (Tracer& T : State.Tracers)
    {
        T.Lifetime -= DeltaTime;
    }
    std::erase_if(State.Tracers, [](const Tracer& T) { return T.Lifetime <= 0.f; });

    for (FloatingText& T : State.FloatingTexts)
    {
        T.Position.y -= 30.f * DeltaTime;
        T.Lifetime -= DeltaTime;
    }
    std::erase_if(State.FloatingTexts, [](const FloatingText& T) { return T.Lifetime <= 0.f; });

    for (Worm& W : State.Game.Worms)
    {
        W.AnimTime += DeltaTime;
        W.FlashSec = std::max(0.f, W.FlashSec - DeltaTime);
    }

    UpdateBackdrop(State.Game.Background, GetBiomeTheme(State.Game.Biome), State.Game.Wind, DeltaTime, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));

    constexpr float ShakeFalloffPerSec = 1.8f;
    State.ShakeScale = std::max(0.f, State.ShakeScale - ShakeFalloffPerSec * DeltaTime);
}

