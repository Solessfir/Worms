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
    Music GameOverMusic;
    Music VictoryMusic;
    Sound PadHitSound;
    Sound BrickAndWallHitSounds[3];
    Sound PowerupSound;
    Sound LaserSound;

    const Sound& GetRandomHitSound()
    {
        static_assert(std::size(BrickAndWallHitSounds) > 0, "No hit sounds!");
        constexpr int LastIndex = static_cast<int>(std::size(BrickAndWallHitSounds)) - 1;
        return BrickAndWallHitSounds[GetRandomValue(0, LastIndex)];
    }

    bool ConfirmPressed()
    {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            || IsKeyPressed(KEY_SPACE)
            || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    }

    std::string GetLevelPath(const GameState& State)
    {
        (void)State;
        #if RELEASE
        return TextFormat("%sassets/levels/level_1.txt", GetApplicationDirectory());
        #else
        return "assets/levels/level_1.txt";
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
        LoadLevelFromPath(State.Sim, Path.c_str(), State.Field.Width);

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
    }

    void RestartMatch(GameState& State)
    {
        StartMatch(State);
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
        if (State.Screen == GameScreen::MainMenu)
        {
            if (IsButtonClicked(State, GetMenuButtonRect(State, 0, 2)))
            {
                StopMusicStream(MenuMusic);
                PlayMusicStream(GameplayMusic);
                StartMatch(State);
            }
            else if (IsButtonClicked(State, GetMenuButtonRect(State, 1, 2)))
            {
                ResetMainMenu(State, GameScreen::Options);
            }
        }
        else // Options
        {
            if (IsButtonClicked(State, GetMenuButtonRect(State, 0, 2)))
            {
                ResetMainMenu(State, GameScreen::MainMenu);
            }
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
                SpawnParticles(State.Particles, Sim.Worms[i].Position, RAYWHITE, 24);
            }
        }

        if (Sim.Phase == TurnPhase::Aiming)
        {
            Worm& Active = Sim.Worms[Sim.CurrentPlayer];
            const Vector2 MousePos = GetMousePosition();

            Vector2 FireVelocity{};
            if (UpdateWormAim(Active, MousePos, DeltaTime, FireVelocity))
            {
                Sim.Shot.Position = Active.Position;
                Sim.Shot.Velocity = FireVelocity;
                Sim.Shot.Active = true;
                Sim.Phase = TurnPhase::Firing;
            }
        }
        else if (Sim.Phase == TurnPhase::Firing)
        {
            UpdateProjectile(Sim.Shot, DeltaTime);

            bool Exploded = false;
            Vector2 ExplodeAt{};

            if (Sim.Shot.Active)
            {
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
                    ExplodeAt = Sim.Shot.Position;
                    Sim.Shot.Active = false;

                    const ExplosionResult Result = ApplyExplosionDamage(Sim, ExplodeAt, BlastRadius, static_cast<int>(BlastMaxDamage));
                    SpawnParticles(State.Particles, ExplodeAt, ORANGE, 40);
                    State.ShakeScale = std::min(1.f, State.ShakeScale + 0.7f);

                    for (const int WormIndex : Result.HitWormIndices)
                    {
                        const Worm& W = Sim.Worms[WormIndex];
                        const float Distance = Vector2Distance(ExplodeAt, W.Position);
                        const float DamageScale = std::clamp(1.f - std::max(0.f, Distance - Worm::Radius) / BlastRadius, 0.f, 1.f);
                        const int Damage = static_cast<int>(BlastMaxDamage * DamageScale);
                        SpawnFloatingText(State.FloatingTexts, {.x = W.Position.x, .y = W.Position.y - 40.f}, TextFormat("-%d", Damage), RED, 32, 1.2f, true);
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
            PlayMusicStream(MenuMusic);
            ResetMainMenu(State, GameScreen::MainMenu);
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

        if (State.Screen == GameScreen::MainMenu || State.Screen == GameScreen::Options)
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
        UpdateMusicStream(GameOverMusic);
        UpdateMusicStream(VictoryMusic);

        // Tick Screen Shake falloff
        constexpr float ShakeFalloffPerSec = 1.8f;
        State.ShakeScale = std::max(0.f, State.ShakeScale - ShakeFalloffPerSec * DeltaTime);
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

            // Draw Laser Projectiles
            for (const Laser& Laser : State.Sim.Lasers)
            {
                DrawRectangleRec({.x = State.Field.X + Laser.Position.x - 2.f, .y = State.Field.Y + Laser.Position.y - 8.f, .width = 4.f, .height = 14.f}, ORANGE);
            }

            // Draw Balls
            for (const Ball& Ball : State.Sim.Balls)
            {
                DrawCircleV({.x = State.Field.X + Ball.Position.x, .y = State.Field.Y + Ball.Position.y}, Ball.Radius, RAYWHITE);
            }

            // Draw Pad
            const Vector2 PaddlePosition = {.x = State.Field.X + State.Sim.Pad.Position.x, .y = State.Field.Y + State.Sim.Pad.Position.y};
            const Vector2 PaddleSize = {.x = State.Sim.Pad.Width, .y = State.Sim.Pad.Height};
            constexpr Color PadColor = {.r = 80, .g = 140, .b = 255, .a = 255};

            // Flash Pad on Ball hit
            const float FlashAmount = std::clamp(State.Sim.Pad.FlashSec / Paddle::FlashDuration, 0.f, 1.f);
            const Color DrawColor = ColorLerp(PadColor, WHITE, FlashAmount);

            DrawRectangleRounded({.x = PaddlePosition.x, .y = PaddlePosition.y, .width = PaddleSize.x, .height = PaddleSize.y}, 0.5f, 8, DrawColor);

            // Draw Protection Powerup
            if (State.Sim.ProtectionActive)
            {
                constexpr float BarHeight = 5.f;
                DrawRectangleRec({.x = State.Field.X, .y = State.Field.Y + State.Field.Height - BarHeight, .width = State.Field.Width, .height = BarHeight}, PINK);
            }

            // Draw Bricks
            const float IntroProgress = 1.f - State.LevelIntroSec / GameState::LevelIntroDuration;
            for (const Brick& Brick : State.Sim.Bricks)
            {
                // Skip fully-dead bricks
                if (!Brick.Alive && Brick.DeathSec <= 0.f)
                {
                    continue;
                }

                constexpr float Padding = 2.f;
                const float FullW = Brick.Width - Padding * 2.f;
                const float FullH = Brick.Height - Padding * 2.f;
                const float CenterX = State.Field.X + Brick.Position.x + Brick.Width * 0.5f;
                const float CenterY = State.Field.Y + Brick.Position.y + Brick.Height * 0.5f;

                float BrickScale;
                Color BrickTint = Brick.Tint;

                if (Brick.Alive)
                {
                    // Brick Reveal Animation on every new Level. Reveal raws top to bottom
                    const float RevealThreshold = Brick.Position.y / State.Field.Height;
                    constexpr float RevealSpeed = 4.f;
                    BrickScale = std::clamp((IntroProgress - RevealThreshold) * RevealSpeed, 0.f, 1.f);
                    if (BrickScale <= 0.f)
                    {
                        continue;
                    }
                }
                else // Play Brick Death anim
                {
                    constexpr float DeathMaxScale = 1.4f;
                    const float Progress = 1.f - Brick.DeathSec / Brick::DeathDuration;
                    BrickScale = Lerp(1.f, DeathMaxScale, Progress);
                    BrickTint = ColorAlpha(BrickTint, 1.f - Progress);
                }

                const float DrawW = FullW * BrickScale;
                const float DrawH = FullH * BrickScale;
                DrawRectangleRounded({.x = CenterX - DrawW * 0.5f, .y = CenterY - DrawH * 0.5f, .width = DrawW, .height = DrawH}, 0.5f, 8, BrickTint);
            }

            // Draw Powerup drops
            for (const Drop& Drop : State.Drops)
            {
                const PowerupDef& Def = GetPowerupDefinition(Drop.Type);
                const float DrawX = State.Field.X + Drop.Position.x;
                const float DrawY = State.Field.Y + Drop.Position.y;
                DrawRectangleRounded({.x = DrawX, .y = DrawY, .width = Drop.Width, .height = Drop.Height}, 0.5f, 8, Def.Tint);

                constexpr int SymbolFontSize = 32;
                const char Symbol[2] = {Def.Symbol, '\0'};
                const int SymbolWidth = MeasureText(Symbol, SymbolFontSize);
                DrawText(Symbol, static_cast<int>(DrawX + (Drop.Width - static_cast<float>(SymbolWidth)) * 0.5f), static_cast<int>(DrawY + (Drop.Height - SymbolFontSize) * 0.5f), SymbolFontSize, BLACK);
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
            if (State.Screen == GameScreen::MainMenu || State.Screen == GameScreen::Options)
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

                constexpr Color LightBlue = PadColor;
                constexpr Color DimGray = {.r = 80, .g = 80, .b = 80, .a = 255};
                constexpr Color DimGold = {.r = 200, .g = 160, .b = 60, .a = 255};

                DrawCentered("BREAKOUT", 128, 0.15f, RAYWHITE);
                DrawCentered(State.Screen == GameScreen::MainMenu ? "MAIN MENU" : "OPTIONS", 64, 0.27f, DimGold);

                if (State.Screen == GameScreen::MainMenu)
                {
                    DrawMenuItem(GetMenuItemRect(State, 0, 2), "NEW GAME", LightBlue);
                    DrawMenuItem(GetMenuItemRect(State, 1, 2), "OPTIONS", LightBlue);
                }
                else
                {
                    DrawMenuItem(GetMenuItemRect(State, 0, 2), "KEYBOARD", State.MouseControl ? DimGray : LightBlue);
                    DrawMenuItem(GetMenuItemRect(State, 1, 2), "MOUSE", State.MouseControl ? LightBlue : DimGray);
                }
            }

            // FPS
            {
                constexpr int FpsFontSize = 24;
                const char* FpsText = TextFormat("%d FPS", GetFPS());
                const int TextWidth = MeasureText(FpsText, FpsFontSize);
                constexpr Color DimGray = {.r = 120, .g = 120, .b = 120, .a = 255};
                DrawText(FpsText, PillarCenterX - TextWidth / 2, 14, FpsFontSize, DimGray);
            }

            if (State.Screen == GameScreen::Game || State.Screen == GameScreen::GameOver || State.Screen == GameScreen::Victory)
            {
                // Level
                {
                    constexpr int LevelFontSize = 24;
                    const char* LevelText = TextFormat("LVL %d", State.CurrentLevel);
                    const int TextWidth = MeasureText(LevelText, LevelFontSize);
                    constexpr Color DimGold = {.r = 200, .g = 160, .b = 60, .a = 255};
                    DrawText(LevelText, PillarCenterX - TextWidth / 2, 48, LevelFontSize, DimGold);
                }

                // Score
                {
                    constexpr int ScoreFontSize = 38;
                    const char* ScoreText = TextFormat("%d", State.Sim.Score);
                    const int TextWidth = MeasureText(ScoreText, ScoreFontSize);
                    DrawText(ScoreText, PillarCenterX - TextWidth / 2, 80, ScoreFontSize, RAYWHITE);
                }

                // Health
                {
                    constexpr float HeartSize = 28.f;
                    constexpr float HeartSpacing = HeartSize * 1.4f;
                    constexpr float RowSpacing = HeartSize * 1.4f;
                    constexpr float BaseY = 134.f;
                    constexpr int PerRow = 5;
                    for (int i = 0; i < State.Sim.Lives; ++i)
                    {
                        const int Row = i / PerRow;
                        const int Col = i % PerRow;
                        const int HeartsThisRow = std::min(PerRow, State.Sim.Lives - Row * PerRow);
                        const float RowWidth = static_cast<float>(HeartsThisRow) * HeartSpacing;
                        const float StartX = static_cast<float>(PillarCenterX) - RowWidth * 0.5f + HeartSpacing * 0.5f;
                        DrawHeart({.x = StartX + static_cast<float>(Col) * HeartSpacing, .y = BaseY + static_cast<float>(Row) * RowSpacing}, HeartSize, RED);
                    }
                }

                // Right Side: Active Powerup Effects
                {
                    const int RightPillarCenterX = static_cast<int>((State.Field.X + State.Field.Width + static_cast<float>(ScreenWidth)) * 0.5f);
                    constexpr float SlotSize = 44.f;
                    constexpr float SlotSpacing = 62.f;
                    constexpr float BarHeight = 6.f;
                    constexpr float StartY = 24.f;
                    constexpr int SymbolFontSize = 32;

                    for (int i = 0; i < static_cast<int>(State.ActiveEffects.size()); ++i)
                    {
                        const ActiveEffect& Effect = State.ActiveEffects[i];
                        const PowerupDef& Def = GetPowerupDefinition(Effect.Type);

                        const float SlotY = StartY + static_cast<float>(i) * SlotSpacing;
                        const float SlotX = static_cast<float>(RightPillarCenterX) - SlotSize * 0.5f;

                        DrawRectangleRounded({.x = SlotX, .y = SlotY, .width = SlotSize, .height = SlotSize}, 0.5f, 8, Def.Tint);

                        const char Symbol[2] = {Def.Symbol, '\0'};
                        const int SymbolWidth = MeasureText(Symbol, SymbolFontSize);
                        DrawText(Symbol, RightPillarCenterX - SymbolWidth / 2, static_cast<int>(SlotY + (SlotSize - SymbolFontSize) * 0.5f), SymbolFontSize, BLACK);

                        const float FillRatio = std::clamp(Effect.TimeLeft / Def.Duration, 0.f, 1.f);
                        DrawRectangleRec({.x = SlotX, .y = SlotY + SlotSize + 4.f, .width = SlotSize, .height = BarHeight}, DARKGRAY);
                        DrawRectangleRec({.x = SlotX, .y = SlotY + SlotSize + 4.f, .width = SlotSize * FillRatio, .height = BarHeight}, Def.Tint);
                    }
                }
            }

            // Game Over overlay
            if (State.Screen == GameScreen::GameOver)
            {
                DrawRectangle(0, 0, ScreenWidth, ScreenHeight, {.r = 0, .g = 0, .b = 0, .a = 160});

                const char* OverText = "GAME OVER";
                constexpr int OverSize = 96;
                DrawText(OverText, ScreenWidth / 2 - MeasureText(OverText, OverSize) / 2, ScreenHeight / 2 - 170, OverSize, RAYWHITE);

                const char* LevelNumberText = TextFormat("Level %d", State.CurrentLevel);
                constexpr int LevelLineSize = 36;
                constexpr Color DimGold = {.r = 200, .g = 160, .b = 60, .a = 255};
                DrawText(LevelNumberText, ScreenWidth / 2 - MeasureText(LevelNumberText, LevelLineSize) / 2, ScreenHeight / 2 - 40, LevelLineSize, DimGold);

                const char* ScoreText = TextFormat("Score: %d", State.Sim.Score);
                constexpr int ScoreTextSize = 44;
                DrawText(ScoreText, ScreenWidth / 2 - MeasureText(ScoreText, ScoreTextSize) / 2, ScreenHeight / 2 + 16, ScoreTextSize, RAYWHITE);

                const char* BestScoreText = TextFormat("Best: %d", State.HighestScore);
                constexpr int BestScoreTextSize = 36;
                DrawText(BestScoreText, ScreenWidth / 2 - MeasureText(BestScoreText, BestScoreTextSize) / 2, ScreenHeight / 2 + 72, BestScoreTextSize, DimGold);

                const char* PromptText = "Confirm to Restart or Esc to Exit";
                constexpr int PromptTextSize = 32;
                constexpr Color DimGray = {.r = 180, .g = 180, .b = 180, .a = 255};
                DrawText(PromptText, ScreenWidth / 2 - MeasureText(PromptText, PromptTextSize) / 2, ScreenHeight / 2 + 128, PromptTextSize, DimGray);
            }

            // Victory overlay
            if (State.Screen == GameScreen::Victory)
            {
                DrawRectangle(0, 0, ScreenWidth, ScreenHeight, {.r = 0, .g = 0, .b = 0, .a = 160});

                const char* WinText = "YOU WIN!";
                constexpr int WinSize = 96;
                DrawText(WinText, ScreenWidth / 2 - MeasureText(WinText, WinSize) / 2, ScreenHeight / 2 - 170, WinSize, GOLD);

                const char* ScoreText = TextFormat("Score: %d", State.Sim.Score);
                constexpr int ScoreTextSize = 44;
                DrawText(ScoreText, ScreenWidth / 2 - MeasureText(ScoreText, ScoreTextSize) / 2, ScreenHeight / 2 - 30, ScoreTextSize, RAYWHITE);

                const char* BestScoreText = TextFormat("Best: %d", State.HighestScore);
                constexpr int BestScoreTextSize = 36;
                constexpr Color DimGold = {.r = 200, .g = 160, .b = 60, .a = 255};
                DrawText(BestScoreText, ScreenWidth / 2 - MeasureText(BestScoreText, BestScoreTextSize) / 2, ScreenHeight / 2 + 26, BestScoreTextSize, DimGold);

                const char* PromptText = "Confirm to Play again or Esc to Exit";
                constexpr int PromptTextSize = 32;
                constexpr Color DimGray = {.r = 180, .g = 180, .b = 180, .a = 255};
                DrawText(PromptText, ScreenWidth / 2 - MeasureText(PromptText, PromptTextSize) / 2, ScreenHeight / 2 + 82, PromptTextSize, DimGray);
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
    GameOverMusic = LoadMusicStream("assets/audio/gameover.mp3");
    VictoryMusic = LoadMusicStream("assets/audio/victory.mp3");
    #else
    MenuMusic = LoadMusicStream(TextFormat("%sassets/audio/menu.mp3", GetApplicationDirectory()));
    GameplayMusic = LoadMusicStream(TextFormat("%sassets/audio/soundtrack.mp3", GetApplicationDirectory()));
    GameOverMusic = LoadMusicStream(TextFormat("%sassets/audio/gameover.mp3", GetApplicationDirectory()));
    VictoryMusic = LoadMusicStream(TextFormat("%sassets/audio/victory.mp3", GetApplicationDirectory()));
    #endif

    SetMusicVolume(MenuMusic, 0.35f);
    SetMusicVolume(GameplayMusic, 0.35f);
    SetMusicVolume(GameOverMusic, 0.35f);
    SetMusicVolume(VictoryMusic, 0.35f);

    PlayMusicStream(MenuMusic);

    // SFX
    PadHitSound = LoadSound("assets/audio/hit1.mp3");
    BrickAndWallHitSounds[0] = LoadSound("assets/audio/hit2.mp3");
    BrickAndWallHitSounds[1] = LoadSound("assets/audio/hit3.mp3");
    BrickAndWallHitSounds[2] = LoadSound("assets/audio/hit4.mp3");
    PowerupSound = LoadSound("assets/audio/powerup.mp3");
    LaserSound = LoadSound("assets/audio/laser.mp3");

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
        if (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))
        {
            // Gamepad should close the game too
            // For Keyboard raylib::InitWindow sets: CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
            break;
        }

        UpdateFrame(State, GetFrameTime());
        RenderFrame(State);
    }

    UnloadSound(PadHitSound);
    UnloadSound(BrickAndWallHitSounds[0]);
    UnloadSound(BrickAndWallHitSounds[1]);
    UnloadSound(BrickAndWallHitSounds[2]);
    UnloadSound(PowerupSound);
    UnloadSound(LaserSound);
    UnloadMusicStream(MenuMusic);
    UnloadMusicStream(GameplayMusic);
    UnloadMusicStream(GameOverMusic);
    UnloadMusicStream(VictoryMusic);
    CloseAudioDevice();
    CloseWindow();
}
