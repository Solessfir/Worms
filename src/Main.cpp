// https://www.raylib.com/cheatsheet/cheatsheet.html

#include "Game.h"
#include "raylib/raylib.h"
#include "raylib/raymath.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <string>

namespace
{
    constexpr float BlastRadius = 90.f;
    constexpr float BlastMaxDamage = 60.f;
    constexpr float SettleDuration = 0.6f;
    constexpr float TrajectoryTimeStep = 0.03f;
    constexpr int TrajectoryPoints = 40;

    constexpr float FieldAspect = 4.f / 3.f;
    float VirtualHeight = 900.f;                    // overwritten from the monitor in main()
    float FieldWidth = VirtualHeight * FieldAspect; // playfield must always fit on screen

    struct ViewFit
    {
        float Scale;      // virtual units (window pixels)
        float ViewWidth;  // virtual units visible horizontally
        float ViewHeight; // virtual units visible vertically (900 unless the window is narrower than the Field)
    };

    ViewFit GetViewFit()
    {
        const float WindowWidth = static_cast<float>(GetScreenWidth());
        const float WindowHeight = static_cast<float>(GetScreenHeight());

        // Fit by height; if the window is too narrow for the playfield, fit by width instead
        const float Scale = std::min(WindowHeight / VirtualHeight, WindowWidth / FieldWidth);
        return {.Scale = Scale, .ViewWidth = WindowWidth / Scale, .ViewHeight = WindowHeight / Scale};
    }

    // Audio
    Music MenuMusic;
    Music GameplayMusic;
    Music VictoryMusic;
    Sound ExplosionSound;

    bool ConfirmPressed()
    {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE);
    }

    std::string GetLevelPath(const GameState& State)
    {
        (void)State;
        constexpr int LevelCount = 3;
        static int NextLevelIndex = 1;
        const int LevelIndex = NextLevelIndex;
        NextLevelIndex = (NextLevelIndex % LevelCount) + 1;
        #if RELEASE
        return TextFormat("%sassets/levels/level_%d.txt", GetApplicationDirectory(), LevelIndex);
        #else
        return TextFormat("assets/levels/level_%d.txt", LevelIndex);
        #endif
    }

    void StartMatch(GameState& State)
    {
        State.Particles.clear();
        State.FloatingTexts.clear();
        State.ShakeScale = 0.f;
        State.Winner = -1;
        State.Sim.CurrentPlayer = 0;
        State.Sim.Phase = TurnPhase::Aiming;
        State.Sim.SettleTimer = 0.f;
        State.Sim.Shot.Active = false;

        const std::string Path = GetLevelPath(State);
        LoadLevelFromPath(State.Sim, Path.c_str(), State.Field.Width, State.Field.Height);

        const float SpawnY = State.Field.Height * 0.5f;
        State.Sim.Worms[0] = {
            .Position = {.x = State.Field.Width * 0.2f, .y = SpawnY},
            .Velocity = {},
            .HP = Worm::MaxHP,
            .Name = "Worm One",
            .FacingDir = 1.f,
            .OnGround = false,
            .AimAngle = 0.f,
            .Charging = false,
            .ChargePower = 0.f,
            .Alive = true,
            .FlashSec = 0.f
        };
        State.Sim.Worms[1] = {
            .Position = {.x = State.Field.Width * 0.8f, .y = SpawnY},
            .Velocity = {},
            .HP = Worm::MaxHP,
            .Name = "Worm Two",
            .FacingDir = -1.f,
            .OnGround = false,
            .AimAngle = 0.f,
            .Charging = false,
            .ChargePower = 0.f,
            .Alive = true,
            .FlashSec = 0.f
        };

        State.Screen = GameScreen::Game;
        State.WaitForMouseRelease = true;
    }

    void ResetMainMenu(GameState& State, const GameScreen Screen)
    {
        State.Screen = Screen;
    }

    Rectangle GetMenuButtonRect(const GameState& State, const int Index, const int Count)
    {
        const float ItemW = State.Field.Width * 0.3f;
        const float ItemH = 52.f;
        const float Gap = State.Field.Width * 0.06f;
        const float RowW = ItemW * static_cast<float>(Count) + Gap * static_cast<float>(Count - 1);
        const float StartX = (State.Field.Width - RowW) * 0.5f;
        const float Y = State.Field.Height * 0.5f - ItemH * 0.5f;
        return {.x = StartX + static_cast<float>(Index) * (ItemW + Gap), .y = Y, .width = ItemW, .height = ItemH};
    }

    bool IsButtonClicked(const GameState& State, const Rectangle& Rect)
    {
        const Vector2 Local = {.x = static_cast<float>(GetMouseX()) - State.Field.X, .y = static_cast<float>(GetMouseY()) - State.Field.Y};
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(Local, Rect);
    }

    void UpdateMenu(GameState& State)
    {
        if (IsButtonClicked(State, GetMenuButtonRect(State, 0, 1)))
        {
            StopMusicStream(MenuMusic);
            PlayMusicStream(GameplayMusic);
            StartMatch(State);
        }
    }

    void UpdateGameplay(GameState& State, const float DeltaTime)
    {
        SimState& Sim = State.Sim;

        for (int i = 0; i < 2; ++i)
        {
            const bool AcceptInput = (Sim.Phase == TurnPhase::Aiming) && (i == Sim.CurrentPlayer);
            UpdateWorm(Sim.Worms[i], Sim.Terrain, DeltaTime, State.Field.Width, State.Field.Height, AcceptInput);

            if (CheckWormBelowField(Sim.Worms[i], State.Field.Height))
            {
                const Color WormColor = i == 0 ? SKYBLUE : PINK;
                SpawnParticles(State.Particles, Sim.Worms[i].Position, WormColor, 24);
            }
        }

        if (Sim.Phase == TurnPhase::Aiming && (!Sim.Worms[0].Alive || !Sim.Worms[1].Alive))
        {
            Sim.Phase = TurnPhase::Settling;
            Sim.SettleTimer = SettleDuration;
        }

        if (State.WaitForMouseRelease && IsMouseButtonUp(MOUSE_BUTTON_LEFT))
        {
            State.WaitForMouseRelease = false;
        }

        if (Sim.Phase == TurnPhase::Aiming && !State.WaitForMouseRelease)
        {
            Worm& Active = Sim.Worms[Sim.CurrentPlayer];
            const Vector2 MousePos = GetMousePosition();

            Vector2 FireVelocity{};
            if (UpdateWormAim(Active, MousePos, DeltaTime, FireVelocity))
            {
                constexpr float SpawnOffset = Worm::Radius + Projectile::Radius + 2.f;
                Sim.Shot.Position = {
                    .x = Active.Position.x + std::cosf(Active.AimAngle) * SpawnOffset,
                    .y = Active.Position.y + std::sinf(Active.AimAngle) * SpawnOffset
                };
                Sim.Shot.Velocity = FireVelocity;
                Sim.Shot.Active = true;
                Sim.Phase = TurnPhase::Firing;
            }
        }
        else if (Sim.Phase == TurnPhase::Firing)
        {
            UpdateProjectile(Sim.Shot, DeltaTime);

            if (Sim.Shot.Active)
            {
                constexpr float TrailLifetime = 0.15f;
                constexpr float TrailAlpha = 0.35f;
                State.Particles.push_back({
                    .Position = Sim.Shot.Position,
                    .Velocity = {},
                    .Tint = ColorAlpha(ORANGE, TrailAlpha),
                    .InitialRadius = Projectile::Radius,
                    .Lifetime = TrailLifetime,
                    .MaxLifetime = TrailLifetime
                });

                bool Exploded = false;
                for (const TerrainBlock& Block : Sim.Terrain)
                {
                    if (!Block.Alive)
                    {
                        continue;
                    }

                    const Rectangle BlockRect = {.x = Block.Position.x, .y = Block.Position.y, .width = Block.Width, .height = Block.Height};
                    if (CheckCollisionCircleRec(Sim.Shot.Position, Projectile::Radius, BlockRect))
                    {
                        Exploded = true;
                        break;
                    }
                }

                if (!Exploded)
                {
                    for (const Worm& W : Sim.Worms)
                    {
                        if (!W.Alive)
                        {
                            continue;
                        }
                        if (CheckCollisionCircles(Sim.Shot.Position, Projectile::Radius, W.Position, Worm::Radius))
                        {
                            Exploded = true;
                            break;
                        }
                    }
                }

                if (!Exploded && (Sim.Shot.Position.x < -50.f || Sim.Shot.Position.x > State.Field.Width + 50.f || Sim.Shot.Position.y > State.Field.Height + 50.f))
                {
                    Sim.Shot.Active = false;
                    Sim.Phase = TurnPhase::Settling;
                    Sim.SettleTimer = SettleDuration;
                }

                if (Exploded)
                {
                    const Vector2 ExplodeAt = Sim.Shot.Position;
                    Sim.Shot.Active = false;

                    const ExplosionResult Result = ApplyExplosionDamage(Sim, ExplodeAt, BlastRadius, static_cast<int>(BlastMaxDamage));
                    PlaySfx(ExplosionSound);
                    SpawnParticles(State.Particles, ExplodeAt, ORANGE, 40);
                    State.ShakeScale = std::min(1.f, State.ShakeScale + 0.7f);

                    for (const int WormIndex : Result.HitWormIndices)
                    {
                        const Worm& W = Sim.Worms[WormIndex];
                        const float Distance = Vector2Distance(ExplodeAt, W.Position);
                        const float DamageScale = std::clamp(1.f - std::max(0.f, Distance - Worm::Radius) / BlastRadius, 0.f, 1.f);
                        const int Damage = static_cast<int>(BlastMaxDamage * DamageScale);
                        SpawnFloatingText(State.FloatingTexts, {.x = W.Position.x, .y = W.Position.y - 40.f}, TextFormat("-%d", Damage), RED, 32, 1.2f, true);

                        if (!W.Alive)
                        {
                            const Color WormColor = WormIndex == 0 ? SKYBLUE : PINK;
                            SpawnParticles(State.Particles, W.Position, WormColor, 32);
                        }
                    }

                    Sim.Phase = TurnPhase::Settling;
                    Sim.SettleTimer = SettleDuration;
                }
            }
        }
        else // Settling
        {
            Sim.SettleTimer -= DeltaTime;
            if (Sim.SettleTimer <= 0.f)
            {
                if (!Sim.Worms[0].Alive)
                {
                    State.Winner = 1;
                }
                else if (!Sim.Worms[1].Alive)
                {
                    State.Winner = 0;
                }

                if (State.Winner != -1)
                {
                    State.Screen = GameScreen::Victory;
                    StopMusicStream(GameplayMusic);
                    PlayMusicStream(VictoryMusic);
                }
                else
                {
                    Sim.CurrentPlayer = 1 - Sim.CurrentPlayer;
                    Sim.Phase = TurnPhase::Aiming;
                }
            }
        }

        // Tick dying terrain blocks
        for (TerrainBlock& Block : Sim.Terrain)
        {
            if (!Block.Alive && Block.DeathSec > 0.f)
            {
                Block.DeathSec -= DeltaTime;
            }
        }
    }

    void HandleEndScreenInput(GameState& State)
    {
        if (!ConfirmPressed())
        {
            return;
        }

        if (State.Screen == GameScreen::Victory)
        {
            StopMusicStream(VictoryMusic);
            PlayMusicStream(GameplayMusic);
            StartMatch(State);
        }
    }

    void UpdateEntities(GameState& State, const float DeltaTime)
    {
        UpdateParticles(State.Particles, DeltaTime);
        UpdateFloatingTexts(State.FloatingTexts, DeltaTime);
        UpdateBackgroundParticles(State.BackgroundParticles, DeltaTime, State.Field.Width, State.Field.Height);
    }

    void UpdateFrame(GameState& State, const float DeltaTime)
    {
        // Window can resize any time: re-fit the view every Tick.
        // Field.X/Y center the playfield inside the visible virtual area
        const ViewFit Fit = GetViewFit();
        State.Field.X = (Fit.ViewWidth - State.Field.Width) * 0.5f;
        State.Field.Y = (Fit.ViewHeight - State.Field.Height) * 0.5f;

        // Map window mouse coords into virtual space.
        // After this, GetMouseX/GetMousePosition return virtual coords everywhere
        SetMouseScale(1.f / Fit.Scale, 1.f / Fit.Scale);

        if (State.Screen == GameScreen::MainMenu)
        {
            UpdateMenu(State);
        }
        else if (State.Screen == GameScreen::Game)
        {
            UpdateGameplay(State, DeltaTime);
        }
        else if (State.Screen == GameScreen::Victory)
        {
            HandleEndScreenInput(State);
        }

        UpdateEntities(State, DeltaTime);

        UpdateMusicStream(MenuMusic);
        UpdateMusicStream(GameplayMusic);
        UpdateMusicStream(VictoryMusic);

        // Tick Screen Shake falloff
        constexpr float ShakeFalloffPerSec = 1.8f;
        State.ShakeScale = std::max(0.f, State.ShakeScale - ShakeFalloffPerSec * DeltaTime);
    }

    void DrawWorm(const Playfield& Field, const SimState& Sim, const int Index)
    {
        const Worm& W = Sim.Worms[Index];
        if (!W.Alive)
        {
            return;
        }

        const Vector2 DrawPos = {.x = Field.X + W.Position.x, .y = Field.Y + W.Position.y};

        const float FlashAmount = std::clamp(W.FlashSec / Worm::FlashDuration, 0.f, 1.f);
        const Color BaseColor = Index == 0 ? SKYBLUE : PINK;
        const Color WormColor = ColorLerp(BaseColor, WHITE, FlashAmount);

        // Body: squat pill shape instead of a bare circle
        const float BodyW = Worm::Radius * 2.3f;
        const float BodyH = Worm::Radius * 1.6f;
        const Rectangle BodyRect = {.x = DrawPos.x - BodyW * 0.5f, .y = DrawPos.y - BodyH * 0.5f, .width = BodyW, .height = BodyH};
        DrawRectangleRounded(BodyRect, 0.9f, 10, WormColor);

        // Segment shading lines to break up the pill silhouette
        const Color SegmentShade = ColorAlpha(BLACK, 0.18f);
        DrawLineEx({.x = BodyRect.x + BodyW * 0.33f, .y = BodyRect.y + 4.f}, {.x = BodyRect.x + BodyW * 0.33f, .y = BodyRect.y + BodyH - 4.f}, 2.f, SegmentShade);
        DrawLineEx({.x = BodyRect.x + BodyW * 0.66f, .y = BodyRect.y + 4.f}, {.x = BodyRect.x + BodyW * 0.66f, .y = BodyRect.y + BodyH - 4.f}, 2.f, SegmentShade);

        // Eyes look toward the aim direction while active, otherwise toward facing direction
        const bool IsActiveAimer = (Index == Sim.CurrentPlayer) && (Sim.Phase == TurnPhase::Aiming);
        const float LookAngle = IsActiveAimer ? W.AimAngle : (W.FacingDir > 0.f ? 0.f : PI);
        const float EyeOffsetX = W.FacingDir * Worm::Radius * 0.5f;
        const Vector2 EyeCenter1 = {.x = DrawPos.x + EyeOffsetX - 6.f, .y = DrawPos.y - Worm::Radius * 0.35f};
        const Vector2 EyeCenter2 = {.x = DrawPos.x + EyeOffsetX + 6.f, .y = DrawPos.y - Worm::Radius * 0.35f};
        const Vector2 PupilOffset = {.x = std::cosf(LookAngle) * 2.2f, .y = std::sinf(LookAngle) * 2.2f};

        DrawCircleV(EyeCenter1, 5.f, RAYWHITE);
        DrawCircleV(EyeCenter2, 5.f, RAYWHITE);
        DrawCircleV({.x = EyeCenter1.x + PupilOffset.x, .y = EyeCenter1.y + PupilOffset.y}, 2.3f, BLACK);
        DrawCircleV({.x = EyeCenter2.x + PupilOffset.x, .y = EyeCenter2.y + PupilOffset.y}, 2.3f, BLACK);

        if (IsActiveAimer)
        {
            const Vector2 AimEnd = {.x = DrawPos.x + std::cosf(W.AimAngle) * (Worm::Radius + 16.f), .y = DrawPos.y + std::sinf(W.AimAngle) * (Worm::Radius + 16.f)};
            DrawLineEx(DrawPos, AimEnd, 3.f, RAYWHITE);

            if (W.Charging)
            {
                constexpr float BarWidth = 44.f;
                constexpr float BarHeight = 6.f;
                const Vector2 BarPos = {.x = DrawPos.x - BarWidth * 0.5f, .y = DrawPos.y - Worm::Radius - 40.f};
                DrawRectangleRec({.x = BarPos.x, .y = BarPos.y, .width = BarWidth, .height = BarHeight}, DARKGRAY);
                DrawRectangleRec({.x = BarPos.x, .y = BarPos.y, .width = BarWidth * W.ChargePower, .height = BarHeight}, ORANGE);
            }
        }

        // Name + HP bar
        constexpr int NameFontSize = 16;
        const int NameWidth = MeasureText(W.Name, NameFontSize);
        const int NameX = static_cast<int>(DrawPos.x) - NameWidth / 2;
        const int NameY = static_cast<int>(DrawPos.y - Worm::Radius - 34.f);
        DrawText(W.Name, NameX, NameY, NameFontSize, RAYWHITE);

        constexpr float HpBarWidth = 50.f;
        constexpr float HpBarHeight = 6.f;
        const float HpFrac = static_cast<float>(W.HP) / static_cast<float>(Worm::MaxHP);
        const Color HpColor = ColorLerp(RED, GREEN, HpFrac);
        const Vector2 HpBarPos = {.x = DrawPos.x - HpBarWidth * 0.5f, .y = DrawPos.y - Worm::Radius - 16.f};
        DrawRectangleRec({.x = HpBarPos.x, .y = HpBarPos.y, .width = HpBarWidth, .height = HpBarHeight}, DARKGRAY);
        DrawRectangleRec({.x = HpBarPos.x, .y = HpBarPos.y, .width = HpBarWidth * HpFrac, .height = HpBarHeight}, HpColor);
    }

    void RenderFrame(const GameState& State)
    {
        // Everything below draws in virtual units, so "screen" = visible virtual area.
        const ViewFit Fit = GetViewFit();
        const int ScreenWidth = static_cast<int>(Fit.ViewWidth);
        const int ScreenHeight = static_cast<int>(Fit.ViewHeight);

        // Screen shake: Offset = MaxOffset * Shake ^ 2 * RandomOffset
        // Ref: https://www.reddit.com/r/gamedev/comments/8r2c29/need_help_implementing_screen_shake_for_a_game_im/
        // Ref: Math for Game Programmers: Juicing Your Cameras With Math https://www.youtube.com/watch?v=tu-Qe66AvtY
        constexpr float ShakeMaxOffset = 18.f;
        const float ShakeSquared = State.ShakeScale * State.ShakeScale;
        const Vector2 ShakeOffset =
        {
            .x = ShakeMaxOffset * ShakeSquared * GetRandomFloatValue(-1.f, 1.f),
            .y = ShakeMaxOffset * ShakeSquared * GetRandomFloatValue(-1.f, 1.f)
        };

        // Virtual. Window transform (no shake): HUD, overlays, background
        const Camera2D ViewCamera =
        {
            .offset = {},
            .target = {},
            .rotation = 0.f,
            .zoom = Fit.Scale
        };

        // Same transform + shake on top: world/gameplay layer
        const Camera2D ShakeCamera =
        {
            .offset = {.x = ShakeOffset.x * Fit.Scale, .y = ShakeOffset.y * Fit.Scale},
            .target = {},
            .rotation = 0.f,
            .zoom = Fit.Scale
        };

        BeginDrawing();
        {
            // Draw Background
            ClearBackground({.r = 12, .g = 12, .b = 12, .a = 255});

            // Draw Background Particles
            // Ref: https://www.raylib.com/examples/core/loader.html?name=core_scissor_test
            // Scissor is not camera-aware - it takes raw window coords, so transform the Field rect manually
            BeginScissorMode(static_cast<int>(State.Field.X * Fit.Scale), static_cast<int>(State.Field.Y * Fit.Scale), static_cast<int>(State.Field.Width * Fit.Scale), static_cast<int>(State.Field.Height * Fit.Scale));
            BeginMode2D(ViewCamera);
            for (const BackgroundParticle& Particle : State.BackgroundParticles)
            {
                const Color C = {.r = 200, .g = 200, .b = 200, .a = static_cast<unsigned char>(Particle.Alpha)};
                DrawCircleV({.x = State.Field.X + Particle.Position.x, .y = State.Field.Y + Particle.Position.y}, Particle.Radius, C);
            }
            EndMode2D();
            EndScissorMode();

            // Screen shake start
            // Ref: https://www.raylib.com/examples/core/loader.html?name=core_2d_camera
            BeginMode2D(ShakeCamera);

            constexpr Color DarkGray = {.r = 40, .g = 40, .b = 40, .a = 255};
            DrawRectangleLinesEx({.x = State.Field.X, .y = State.Field.Y, .width = State.Field.Width, .height = State.Field.Height}, 2.f, DarkGray);

            // Draw Terrain Blocks
            for (const TerrainBlock& Block : State.Sim.Terrain)
            {
                if (!Block.Alive && Block.DeathSec <= 0.f)
                {
                    continue;
                }

                constexpr float Padding = 2.f;
                const float FullW = Block.Width - Padding * 2.f;
                const float FullH = Block.Height - Padding * 2.f;
                const float CenterX = State.Field.X + Block.Position.x + Block.Width * 0.5f;
                const float CenterY = State.Field.Y + Block.Position.y + Block.Height * 0.5f;

                float BlockScale = 1.f;
                Color BlockTint = Block.Tint;

                if (!Block.Alive)
                {
                    constexpr float DeathMaxScale = 1.4f;
                    const float Progress = 1.f - Block.DeathSec / TerrainBlock::DeathDuration;
                    BlockScale = Lerp(1.f, DeathMaxScale, Progress);
                    BlockTint = ColorAlpha(BlockTint, 1.f - Progress);
                }

                const float DrawW = FullW * BlockScale;
                const float DrawH = FullH * BlockScale;
                DrawRectangleRounded({.x = CenterX - DrawW * 0.5f, .y = CenterY - DrawH * 0.5f, .width = DrawW, .height = DrawH}, 0.3f, 8, BlockTint);
            }

            // Draw aim trajectory preview for the active Worm
            if (State.Sim.Phase == TurnPhase::Aiming)
            {
                const Worm& Active = State.Sim.Worms[State.Sim.CurrentPlayer];
                if (Active.Charging)
                {
                    const float Speed = Lerp(300.f, 1100.f, Active.ChargePower);
                    const Vector2 PreviewVelocity = {.x = std::cosf(Active.AimAngle) * Speed, .y = std::sinf(Active.AimAngle) * Speed};
                    const std::vector<Vector2> Points = SimulateTrajectory(Active.Position, PreviewVelocity, TrajectoryTimeStep, TrajectoryPoints);

                    for (const Vector2& Point : Points)
                    {
                        DrawCircleV({.x = State.Field.X + Point.x, .y = State.Field.Y + Point.y}, 2.5f, ColorAlpha(RAYWHITE, 0.5f));
                    }
                }
            }

            // Draw Projectile
            if (State.Sim.Shot.Active)
            {
                DrawCircleV({.x = State.Field.X + State.Sim.Shot.Position.x, .y = State.Field.Y + State.Sim.Shot.Position.y}, Projectile::Radius, ORANGE);
            }

            // Draw Worms
            for (int i = 0; i < 2; ++i)
            {
                DrawWorm(State.Field, State.Sim, i);
            }

            // Draw Hit Particles
            for (const Particle& Particle : State.Particles)
            {
                const float Lifetime = Particle.Lifetime / Particle.MaxLifetime;
                Color C = Particle.Tint;
                C.a = static_cast<unsigned char>(static_cast<float>(Particle.Tint.a) * Lifetime);
                DrawCircleV({.x = State.Field.X + Particle.Position.x, .y = State.Field.Y + Particle.Position.y}, Particle.InitialRadius * Lifetime, C);
            }

            // Screen shake end
            // End before Text, so Screen Shake is not affecting it
            EndMode2D();

            // Text/HUD/overlays still need the virtual camera, just without shake
            BeginMode2D(ViewCamera);

            // Floating Text
            for (const FloatingText& FloatingText : State.FloatingTexts)
            {
                const float Frac = FloatingText.Lifetime / FloatingText.MaxLifetime;
                const Color TextColor = ColorAlpha(FloatingText.Tint, Frac);
                const Color OutlineColor = ColorAlpha(BLACK, Frac);

                float ShakeX = 0;
                float ShakeY = 0;

                if (FloatingText.Shake)
                {
                    const float Shake = 4.f * Frac;
                    ShakeX = GetRandomFloatValue(-1.f, 1.f) * Shake;
                    ShakeY = GetRandomFloatValue(-1.f, 1.f) * Shake;
                }

                const int FontSize = FloatingText.FontSize;
                const int TextW = MeasureText(FloatingText.Text, FontSize);
                const int X = static_cast<int>(State.Field.X + FloatingText.Position.x - static_cast<float>(TextW) * 0.5f + ShakeX);
                const int Y = static_cast<int>(State.Field.Y + FloatingText.Position.y + ShakeY);

                // Ref: https://www.reddit.com/r/raylib/comments/12b1pgk/draw_outlined_text/
                constexpr int Outline = 1;
                DrawText(FloatingText.Text, X - Outline, Y, FontSize, OutlineColor);
                DrawText(FloatingText.Text, X + Outline, Y, FontSize, OutlineColor);
                DrawText(FloatingText.Text, X, Y - Outline, FontSize, OutlineColor);
                DrawText(FloatingText.Text, X, Y + Outline, FontSize, OutlineColor);
                DrawText(FloatingText.Text, X, Y, FontSize, TextColor);
            }

            const int PillarCenterX = static_cast<int>(State.Field.X * 0.5f);

            // ---------- Menu / Text ----------

            // Menu overlay
            if (State.Screen == GameScreen::MainMenu)
            {
                auto DrawMenuItem = [&](const Rectangle& Rect, const char* Label, const Color ItemColor)
                {
                    DrawRectangleRounded({.x = State.Field.X + Rect.x, .y = State.Field.Y + Rect.y, .width = Rect.width, .height = Rect.height}, 0.5f, 8, ItemColor);
                    constexpr int FontSize = 36;
                    const int TextW = MeasureText(Label, FontSize);
                    DrawText(Label, static_cast<int>(State.Field.X + Rect.x + (Rect.width - static_cast<float>(TextW)) * 0.5f), static_cast<int>(State.Field.Y + Rect.y + (Rect.height - FontSize) * 0.5f), FontSize, BLACK);
                };

                auto DrawCentered = [&](const char* Text, const int Size, const float FieldYFrac, const Color C)
                {
                    DrawText(Text, static_cast<int>(State.Field.X + (State.Field.Width - static_cast<float>(MeasureText(Text, Size))) * 0.5f), static_cast<int>(State.Field.Y + State.Field.Height * FieldYFrac), Size, C);
                };

                constexpr Color LightBlue = {.r = 80, .g = 140, .b = 255, .a = 255};
                constexpr Color DimGold = {.r = 200, .g = 160, .b = 60, .a = 255};

                DrawCentered("WORMS", 128, 0.15f, RAYWHITE);
                DrawCentered("MAIN MENU", 64, 0.27f, DimGold);
                DrawMenuItem(GetMenuButtonRect(State, 0, 1), "NEW GAME", LightBlue);
            }

            // FPS
            {
                constexpr int FpsFontSize = 24;
                const char* FpsText = TextFormat("%d FPS", GetFPS());
                const int TextWidth = MeasureText(FpsText, FpsFontSize);
                constexpr Color DimGray = {.r = 120, .g = 120, .b = 120, .a = 255};
                DrawText(FpsText, PillarCenterX - TextWidth / 2, 14, FpsFontSize, DimGray);
            }

            if (State.Screen == GameScreen::Game)
            {
                constexpr int TurnFontSize = 32;
                const Worm& Active = State.Sim.Worms[State.Sim.CurrentPlayer];
                const char* TurnText = TextFormat("%s's Turn", Active.Name);
                const int TextWidth = MeasureText(TurnText, TurnFontSize);
                const Color TurnColor = State.Sim.CurrentPlayer == 0 ? SKYBLUE : PINK;
                DrawText(TurnText, PillarCenterX - TextWidth / 2, 48, TurnFontSize, TurnColor);
            }

            // Victory overlay
            if (State.Screen == GameScreen::Victory)
            {
                DrawRectangle(0, 0, ScreenWidth, ScreenHeight, {.r = 0, .g = 0, .b = 0, .a = 160});

                const char* WinnerName = State.Sim.Worms[State.Winner].Name;
                const char* WinText = TextFormat("%s WINS!", WinnerName);
                constexpr int WinSize = 96;
                DrawText(WinText, ScreenWidth / 2 - MeasureText(WinText, WinSize) / 2, ScreenHeight / 2 - 60, WinSize, GOLD);

                const char* PromptText = "Confirm to play again";
                constexpr int PromptTextSize = 32;
                constexpr Color DimGray = {.r = 180, .g = 180, .b = 180, .a = 255};
                DrawText(PromptText, ScreenWidth / 2 - MeasureText(PromptText, PromptTextSize) / 2, ScreenHeight / 2 + 60, PromptTextSize, DimGray);
            }
            EndMode2D();
        }
        EndDrawing();
    }
}

int main()
{
    // ---------- Window Setup ----------
    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    #if RELEASE
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    #endif

    // SetConfigFlags must be called before this func
    // Initial window size only - the view re-fits to the actual window/monitor every Tick
    InitWindow(1600, 900, "Worms");
    SetWindowMinSize(400, 225);

    const int Monitor = GetCurrentMonitor();
    const int RefreshRate = GetMonitorRefreshRate(Monitor);
    SetTargetFPS(RefreshRate);

    // Anchor the virtual design space to this Monitor, so item sizes match the fullscreen look they were tuned for (fallback if the query fails)
    if (GetMonitorHeight(Monitor) > 0)
    {
        VirtualHeight = static_cast<float>(GetMonitorHeight(Monitor));
        FieldWidth = VirtualHeight * FieldAspect;
    }

    #if RELEASE
    // Raylib Fullscreen and Alt+Tab are broken on Windows
    // https://github.com/raysan5/raylib/issues/3865
    // +1 px to not trigger Windows "fullscreen optimization"
    SetWindowSize(GetMonitorWidth(Monitor) + 1, GetMonitorHeight(Monitor) + 1);
    SetWindowPosition(0, 0);
    HideCursor();
    #endif

    // ---------- Audio Setup ----------
    InitAudioDevice();

    // Music
    #if DEBUG
    MenuMusic = LoadMusicStream("assets/audio/menu.mp3");
    GameplayMusic = LoadMusicStream("assets/audio/soundtrack.mp3");
    VictoryMusic = LoadMusicStream("assets/audio/victory.mp3");
    ExplosionSound = LoadSound("assets/audio/explosion.mp3");
    #else
    MenuMusic = LoadMusicStream(TextFormat("%sassets/audio/menu.mp3", GetApplicationDirectory()));
    GameplayMusic = LoadMusicStream(TextFormat("%sassets/audio/soundtrack.mp3", GetApplicationDirectory()));
    VictoryMusic = LoadMusicStream(TextFormat("%sassets/audio/victory.mp3", GetApplicationDirectory()));
    ExplosionSound = LoadSound(TextFormat("%sassets/audio/explosion.mp3", GetApplicationDirectory()));
    #endif

    SetMusicVolume(MenuMusic, 0.35f);
    SetMusicVolume(GameplayMusic, 0.35f);
    SetMusicVolume(VictoryMusic, 0.35f);
    SetSoundVolume(ExplosionSound, 0.5f);

    PlayMusicStream(MenuMusic);

    // ---------- Game Setup ----------
    GameState State{};
    State.Screen = GameScreen::MainMenu;

    // Field size lives in virtual design units - window size never affects gameplay proportions.
    // Field.X/Y (centering inside the window) are re-derived every Tick in UpdateFrame,
    // but set them now too so pre-loop setup (cursor centering) starts correct
    State.Field.Height = VirtualHeight;
    State.Field.Width = FieldWidth;
    const ViewFit InitialFit = GetViewFit();
    State.Field.X = (InitialFit.ViewWidth - State.Field.Width) * 0.5f;
    State.Field.Y = (InitialFit.ViewHeight - State.Field.Height) * 0.5f;

    State.Winner = -1;
    ResetMainMenu(State, GameScreen::MainMenu);
    InitBackgroundParticles(State.BackgroundParticles, 150, State.Field.Width, State.Field.Height);

    // ---------- Main Loop ----------
    while (!WindowShouldClose())
    {
        UpdateFrame(State, GetFrameTime());
        RenderFrame(State);
    }

    UnloadMusicStream(MenuMusic);
    UnloadMusicStream(GameplayMusic);
    UnloadMusicStream(VictoryMusic);
    UnloadSound(ExplosionSound);
    CloseAudioDevice();
    CloseWindow();
}
