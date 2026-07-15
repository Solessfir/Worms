// https://www.raylib.com/cheatsheet/cheatsheet.html

#include "Game.h"
#include "raylib/raylib.h"
#include "raylib/raymath.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <string>

namespace
{
    constexpr float KeyboardPadSpeed = 2000.f;
    constexpr float BallLaunchVelocity = 1000.f;
    constexpr float BallRadius = 18.f;
    constexpr int Lives = 3;

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

    void DrawHeart(const Vector2 Center, const float Size, const Color Tint)
    {
        const float Radius = Size * 0.25f;
        DrawCircleV({.x = Center.x - Radius, .y = Center.y}, Radius, Tint);
        DrawCircleV({.x = Center.x + Radius, .y = Center.y}, Radius, Tint);
        DrawTriangle({.x = Center.x - 2.f * Radius, .y = Center.y}, {.x = Center.x, .y = Center.y + 2.4f * Radius}, {.x = Center.x + 2.f * Radius, .y = Center.y}, Tint);
    }

    float GetBallPadJiggleOffset(const GameState& State)
    {
        constexpr float MaxJigglePx = 20.f;
        constexpr float JiggleScale = 0.02f;
        return std::clamp(State.Sim.Pad.VelocityX * JiggleScale, -MaxJigglePx, MaxJigglePx);
    }

    void ResetMouseCursor(const GameState& State)
    {
        // Reset cursor to the Pad location. Otherwise, Pad will snap to the Cursor on the next Tick
        // SetMousePosition takes raw window coords, so transform from virtual space
        const ViewFit Fit = GetViewFit();
        const float CursorX = State.Field.X + State.Sim.Pad.Position.x + State.Sim.Pad.Width * 0.5f;
        SetMousePosition(static_cast<int>(CursorX * Fit.Scale), static_cast<int>((State.Field.Y + State.Sim.Pad.Position.y) * Fit.Scale));
    }

    Rectangle GetMenuItemRect(const GameState& State, const int Index, const int Count)
    {
        const float ItemW = State.Field.Width * 0.3f;
        const float ItemH = 52.f;
        const float Gap = State.Field.Width * 0.06f;
        const float RowW = ItemW * static_cast<float>(Count) + Gap * static_cast<float>(Count - 1);
        const float StartX = (State.Field.Width - RowW) * 0.5f;
        const float Y = State.Field.Height * 0.5f - ItemH * 0.5f;
        return {.x = StartX + static_cast<float>(Index) * (ItemW + Gap), .y = Y, .width = ItemW, .height = ItemH};
    }

    std::string GetLevelPath(const GameState& State)
    {
        #if RELEASE
        return TextFormat("%sassets/levels/level_%d.txt", GetApplicationDirectory(), State.CurrentLevel);
        #else
        return TextFormat("assets/levels/level_%d.txt", State.CurrentLevel);
        #endif
    }

    void LaunchBall(GameState& State)
    {
        std::vector<Ball*> IdleBalls;
        for (Ball& Ball : State.Sim.Balls)
        {
            // Spawned Multiballs can have Caught = false, but attached to the Pad (not moving)
            const float BallSpeedSquared = Ball.Velocity.x * Ball.Velocity.x + Ball.Velocity.y * Ball.Velocity.y;
            if (Ball.Caught || BallSpeedSquared < 1.f)
            {
                IdleBalls.push_back(&Ball);
            }
        }

        if (IdleBalls.empty())
        {
            return;
        }

        // If Multiball: Spread Balls so they don't all move the same direction
        constexpr float HorizontalSpread = 150.f;
        for (size_t i = 0; i < IdleBalls.size(); ++i)
        {
            Ball& Ball = *IdleBalls[i];
            Ball.Caught = false;
            const float Offset = static_cast<float>(i) - static_cast<float>(IdleBalls.size() - 1) * 0.5f;
            Ball.Velocity = {.x = Offset * HorizontalSpread, .y = -BallLaunchVelocity};
        }

        State.Sim.BallLaunched = true;
    }

    void SpawnBallTrail(GameState& State)
    {
        constexpr float TrailLifetime = 0.15f;
        constexpr float TrailAlpha = 0.025f;

        for (const Ball& Ball : State.Sim.Balls)
        {
            if (Ball.Caught)
            {
                continue;
            }

            State.Particles.push_back({
                .Position = Ball.Position,
                .Velocity = {},
                .Tint = ColorAlpha(RAYWHITE, TrailAlpha),
                .InitialRadius = Ball.Radius,
                .Lifetime = TrailLifetime,
                .MaxLifetime = TrailLifetime
            });
        }
    }

    void SpawnPowerupDrop(GameState& State, const Vector2 Center)
    {
        // This could be the last Brick alive (BricksDestroyed already has this kill)
        // No drop then - level clears and the Powerup would be unreachable
        if (State.Sim.BricksDestroyed >= State.Sim.TotalInteractiveBricks)
        {
            return;
        }

        // Early game is super boring, so we increase drop chance, and decrease it closer to the End game
        constexpr int DropChanceEarly = 40;
        constexpr int DropChanceLate = 8;

        // Force a Drop if none for a long time, or hold back if Drops come too fast
        constexpr double DropMinDuration = 0.5;
        constexpr double DropMaxDuration = 5.0;
        const double SinceLastDrop = GetTime() - State.Sim.LastDropTime;

        constexpr float DropWidth = 42.f;
        constexpr float DropHeight = 42.f;

        float Progress = 0.f;
        if (State.Sim.TotalInteractiveBricks > 0)
        {
            Progress = static_cast<float>(State.Sim.BricksDestroyed) / static_cast<float>(State.Sim.TotalInteractiveBricks);
        }

        int DropChancePercent = static_cast<int>(DropChanceEarly + (DropChanceLate - DropChanceEarly) * Progress);

        if (SinceLastDrop > DropMaxDuration)
        {
            DropChancePercent = 100;
        }
        else if (SinceLastDrop <= DropMinDuration)
        {
            DropChancePercent = 0;
        }

        if (GetRandomValue(0, 99) < DropChancePercent)
        {
            State.Sim.LastDropTime = GetTime();
            const PowerupType Type = static_cast<PowerupType>(GetRandomValue(0, static_cast<int>(PowerupType::Count) - 1));
            State.Drops.push_back({
                .Position = {.x = Center.x - DropWidth * 0.5f, .y = Center.y - DropHeight * 0.5f},
                .Width = DropWidth,
                .Height = DropHeight,
                .Type = Type
            });
        }
    }

    void StartLevel(GameState& State)
    {
        State.Sim.Balls.clear();
        State.Sim.Balls.push_back(MakeBall({.x = State.Field.Width * 0.5f, .y = State.Field.Height * 0.5f}, {}, BallRadius));
        State.Sim.BricksDestroyed = 0;
        State.Sim.LastDropTime = GetTime();
        State.Sim.BallSpeedMultiplier = 1.f;
        State.Sim.BallLaunched = false;
        State.Sim.LaserActive = false;
        State.Sim.LaserCooldownSec = 0.f;
        State.Sim.CatchActive = false;
        State.Sim.ThroughActive = false;
        State.Sim.ProtectionActive = false;
        State.Drops.clear();
        State.Particles.clear();
        State.ActiveEffects.clear();
        State.FloatingTexts.clear();
        State.Sim.Lasers.clear();
        State.ShakeScale = 0.f;
        State.LevelIntroSec = GameState::LevelIntroDuration;
        State.Screen = GameScreen::Game;

        // Center Pad and Mouse Cursor
        State.Sim.Pad.Position.x = (State.Field.Width - State.Sim.Pad.Width) * 0.5f;
        ResetMouseCursor(State);

        const std::string Path = GetLevelPath(State);
        LoadLevelFromPath(State.Sim, Path.c_str(), State.Field.Width);

        // Show Level Name every new Level
        const Vector2 BannerPos = {.x = State.Field.Width * 0.5f, .y = State.Field.Height * 0.45f};
        SpawnFloatingText(State.FloatingTexts, BannerPos, TextFormat("LEVEL %d", State.CurrentLevel), GOLD, 128, GameState::LevelIntroDuration, false);
    }

    void RestartGame(GameState& State)
    {
        State.CurrentLevel = 1;
        State.Sim.Lives = Lives;
        State.Sim.Score = 0;
        State.Sim.Combo = 0;
        StartLevel(State);
    }

    void LoadNextLevel(GameState& State)
    {
        ++State.CurrentLevel;
        const std::string Path = GetLevelPath(State);

        if (!FileExists(Path.c_str()))
        {
            State.Screen = GameScreen::Victory;
            StopMusicStream(GameplayMusic);
            PlayMusicStream(VictoryMusic);
            return;
        }

        StartLevel(State);
    }

    void ResetMainMenu(GameState& State, const GameScreen Screen)
    {
        const float BaseWidth = GetBasePadWidth(State);
        State.Sim.Pad.Width = BaseWidth;
        State.Sim.Pad.Position.y = State.Field.Height - 28.f;
        State.Sim.Pad.Position.x = std::clamp(State.Sim.Pad.Position.x, 0.f, State.Field.Width - BaseWidth);
        State.Sim.Balls.clear();
        State.Sim.Balls.push_back(MakeBall({.x = State.Sim.Pad.Position.x + BaseWidth * 0.5f, .y = State.Sim.Pad.Position.y - BallRadius}, {}, BallRadius));
        State.Sim.BallLaunched = false;
        State.Sim.BallSpeedMultiplier = 1.f;
        State.Sim.BricksDestroyed = 0;
        State.Sim.TotalInteractiveBricks = 0;
        State.LevelIntroSec = 0.f;
        State.Screen = Screen;
    }

    bool BounceOffRect(GameState& State, const Rectangle& Rect)
    {
        bool Hit = false;
        for (Ball& Ball : State.Sim.Balls)
        {
            Hit |= TryResolveBallRectCollision(Ball, Rect);
        }
        return Hit;
    }

    void UpdateMenu(GameState& State, const float DeltaTime)
    {
        if (State.Sim.BallLaunched)
        {
            constexpr int SubSteps = 3;
            const float SubDt = DeltaTime / static_cast<float>(SubSteps);
            static std::vector<Vector2> DeadBalls;

            for (int Step = 0; Step < SubSteps; ++Step)
            {
                UpdateBall(State.Sim, SubDt);

                if (UpdateBallWallCollision(State.Sim, State.Field.Width, State.Field.Height, DeadBalls))
                {
                    PlaySfx(GetRandomHitSound());
                }

                if (State.Sim.Balls.empty())
                {
                    // We can't lose in Main Menu - reset the Ball
                    ResetMainMenu(State, State.Screen);
                    break;
                }

                if (UpdateBallPadCollision(State.Sim))
                {
                    PlaySfx(PadHitSound);
                }

                if (State.Screen == GameScreen::MainMenu)
                {
                    if (BounceOffRect(State, GetMenuItemRect(State, 0, 2)))
                    {
                        PlaySfx(GetRandomHitSound());
                        SpawnParticles(State.Particles, State.Sim.Balls[0].Position, SKYBLUE, 32);
                        StopMusicStream(MenuMusic);
                        PlayMusicStream(GameplayMusic);
                        RestartGame(State); // Start the game
                        break;
                    }

                    if (BounceOffRect(State, GetMenuItemRect(State, 1, 2)))
                    {
                        PlaySfx(GetRandomHitSound());
                        SpawnParticles(State.Particles, State.Sim.Balls[0].Position, SKYBLUE, 32);
                        ResetMainMenu(State, GameScreen::Options);
                        break;
                    }
                }
                else // Options
                {
                    // Keyboard control
                    if (BounceOffRect(State, GetMenuItemRect(State, 0, 2)))
                    {
                        PlaySfx(GetRandomHitSound());
                        SpawnParticles(State.Particles, State.Sim.Balls[0].Position, SKYBLUE, 32);
                        State.MouseControl = false;
                        ResetMainMenu(State, GameScreen::MainMenu);
                        break;
                    }

                    // Mouse control
                    if (BounceOffRect(State, GetMenuItemRect(State, 1, 2)))
                    {
                        PlaySfx(GetRandomHitSound());
                        SpawnParticles(State.Particles, State.Sim.Balls[0].Position, SKYBLUE, 32);
                        State.MouseControl = true;
                        ResetMouseCursor(State);
                        ResetMainMenu(State, GameScreen::MainMenu);
                        break;
                    }
                }
            }

            if (State.Screen == GameScreen::MainMenu || State.Screen == GameScreen::Options)
            {
                UpdateBallSpeed(State.Sim, BallLaunchVelocity);
                UpdatePad(State.Sim, DeltaTime, State.Field.Width, State.Field.X, KeyboardPadSpeed, State.MouseControl);
                SpawnBallTrail(State);
            }
        }
        else
        {
            Ball& Ball = State.Sim.Balls[0];
            Ball.Position.x = State.Sim.Pad.Position.x + State.Sim.Pad.Width * 0.5f - GetBallPadJiggleOffset(State);
            Ball.Position.y = State.Sim.Pad.Position.y - Ball.Radius - 1.f;
            UpdatePad(State.Sim, DeltaTime, State.Field.Width, State.Field.X, KeyboardPadSpeed, State.MouseControl);
        }

        if (ConfirmPressed())
        {
            LaunchBall(State);
        }
    }

    void UpdateGameplay(GameState& State, const float DeltaTime)
    {
        if (State.Sim.BallLaunched)
        {
            Paddle& Pad = State.Sim.Pad;
            constexpr int SubSteps = 3;
            const float SubDt = DeltaTime / static_cast<float>(SubSteps);

            static std::vector<int> BrickHits;
            static std::vector<Vector2> DeadBalls;

            for (int i = 0; i < SubSteps; ++i)
            {
                UpdateBall(State.Sim, SubDt);

                const bool WasShield = State.Sim.ProtectionActive;
                if (UpdateBallWallCollision(State.Sim, State.Field.Width, State.Field.Height, DeadBalls))
                {
                    PlaySfx(GetRandomHitSound());

                    if (WasShield && !State.Sim.ProtectionActive)
                    {
                        for (ActiveEffect& Effect : State.ActiveEffects)
                        {
                            if (Effect.Type == PowerupType::Protection)
                            {
                                Effect.TimeLeft = 0.f;
                                break;
                            }
                        }
                    }
                }

                for (const Vector2& Pos : DeadBalls)
                {
                    SpawnParticles(State.Particles, Pos, RAYWHITE, 32);
                    State.ShakeScale = std::min(1.f, State.ShakeScale + 0.6f);
                }

                const bool CatchWasActive = State.Sim.CatchActive;
                if (UpdateBallPadCollision(State.Sim))
                {
                    PlaySfx(PadHitSound);

                    // Reset Catch Powerup
                    if (CatchWasActive && !State.Sim.CatchActive)
                    {
                        for (ActiveEffect& Effect : State.ActiveEffects)
                        {
                            if (Effect.Type == PowerupType::Catch)
                            {
                                Effect.TimeLeft = 0.f;
                                break;
                            }
                        }
                    }
                }

                UpdateBallBrickCollision(State.Sim, BrickHits);

                for (const int HitIndex : BrickHits)
                {
                    PlaySfx(GetRandomHitSound());

                    const Brick& HitBrick = State.Sim.Bricks[HitIndex];
                    const Vector2 Center = {.x = HitBrick.Position.x + HitBrick.Width * 0.5f, .y = HitBrick.Position.y + HitBrick.Height * 0.5f};
                    SpawnParticles(State.Particles, Center, HitBrick.Tint, HitBrick.Alive ? 4 : 32);

                    if (!HitBrick.Alive)
                    {
                        SpawnPowerupDrop(State, Center);
                        State.ShakeScale = std::min(1.f, State.ShakeScale + 0.25f);
                    }
                }

                // Show Combo Text
                const int Combo = State.Sim.Combo;
                if (!BrickHits.empty() && (Combo == 5 || (Combo > 0 && Combo % 10 == 0)))
                {
                    Color TextColor = RAYWHITE;
                    int FontSize = 42;
                    float Lifetime = 1.5f;
                    const char* Label = "COMBO";

                    if (Combo >= 100)
                    {
                        TextColor = RED;
                        FontSize = 58;
                        Lifetime = 2.2f;
                        Label = "UNSTOPPABLE";
                    }
                    else if (Combo >= 50)
                    {
                        TextColor = ORANGE;
                        FontSize = 52;
                        Lifetime = 2.0f;
                        Label = "INSANE";
                    }
                    else if (Combo >= 10)
                    {
                        TextColor = YELLOW;
                        FontSize = 48;
                        Lifetime = 1.7f;
                        Label = "NICE";
                    }

                    const Brick& Brick = State.Sim.Bricks[BrickHits.back()];
                    const Vector2 Position = {.x = Brick.Position.x + Brick.Width * 0.5f, .y = Brick.Position.y + Brick.Height * 0.5f};
                    SpawnFloatingText(State.FloatingTexts, Position, TextFormat("x%d %s!", Combo, Label), TextColor, FontSize, Lifetime, true);
                }
            }

            UpdateBallSpeed(State.Sim, BallLaunchVelocity);
            UpdatePad(State.Sim, DeltaTime, State.Field.Width, State.Field.X, KeyboardPadSpeed, State.MouseControl);

            // Tick Pad flash effect
            if (Pad.FlashSec > 0.f)
            {
                Pad.FlashSec -= DeltaTime;
            }

            // Catch Powerup: Attach caught Balls to the Pad
            const float PadCenterX = Pad.Position.x + Pad.Width * 0.5f;
            for (Ball& Ball : State.Sim.Balls)
            {
                if (Ball.Caught)
                {
                    const float MinX = Pad.Position.x + Ball.Radius;
                    const float MaxX = Pad.Position.x + Pad.Width - Ball.Radius;
                    Ball.Position.x = std::clamp(PadCenterX + Ball.CaughtOffsetX, MinX, MaxX);
                    Ball.Position.y = Pad.Position.y - Ball.Radius;
                }
            }

            SpawnBallTrail(State);
        }
        else // idle state
        {
            if (State.Sim.Balls.empty())
            {
                // Respawn after death
                State.Sim.Balls.push_back(MakeBall({}, {}, BallRadius));
            }

            UpdatePad(State.Sim, DeltaTime, State.Field.Width, State.Field.X, KeyboardPadSpeed, State.MouseControl);

            // Catch Powerup: Attach all Balls to the Pad
            const Paddle& Pad = State.Sim.Pad;
            const float PadCenterX = Pad.Position.x + Pad.Width * 0.5f;

            for (Ball& Ball : State.Sim.Balls)
            {
                Ball.Position.x = PadCenterX - GetBallPadJiggleOffset(State);
                Ball.Position.y = Pad.Position.y - Ball.Radius;
            }
        }

        #if DEBUG
        if (IsKeyPressed(KEY_N))
        {
            LoadNextLevel(State);
        }
        #endif

        if (State.LevelIntroSec > 0.f)
        {
            State.LevelIntroSec -= DeltaTime;
        }

        if (ConfirmPressed())
        {
            LaunchBall(State);
        }

        // Store Highest Score to display on the Game Over screen
        State.HighestScore = std::max(State.HighestScore, State.Sim.Score);

        // Level cleared / Game over
        if (State.Sim.TotalInteractiveBricks > 0 && State.Sim.BricksDestroyed >= State.Sim.TotalInteractiveBricks)
        {
            LoadNextLevel(State);
        }
        else if (State.Sim.Lives <= 0)
        {
            State.Screen = GameScreen::GameOver;
            StopMusicStream(GameplayMusic);
            PlayMusicStream(GameOverMusic);
        }
    }

    void HandleEndScreenInput(GameState& State)
    {
        if (!ConfirmPressed())
        {
            return;
        }

        if (State.Screen == GameScreen::GameOver)
        {
            StopMusicStream(GameOverMusic);
            PlayMusicStream(GameplayMusic);
            RestartGame(State);
            return;
        }

        if (State.Screen == GameScreen::Victory)
        {
            StopMusicStream(VictoryMusic);
            PlayMusicStream(GameplayMusic);
            RestartGame(State);
        }
    }

    void UpdateEntities(GameState& State, const float DeltaTime)
    {
        UpdateParticles(State.Particles, DeltaTime);
        UpdateFloatingTexts(State.FloatingTexts, DeltaTime);
        UpdateBackgroundParticles(State.BackgroundParticles, DeltaTime, State.Field.Width, State.Field.Height);

        // Tick dying Bricks
        for (Brick& Brick : State.Sim.Bricks)
        {
            if (!Brick.Alive && Brick.DeathSec > 0.f)
            {
                Brick.DeathSec -= DeltaTime;
            }
        }

        if (State.Screen != GameScreen::Game)
        {
            // Game is not is progress - do not update anything
            return;
        }

        // Tick falling Drops
        static std::vector<PowerupType> CaughtPowerups;
        static std::vector<Drop> MissedDrops;
        UpdateDrops(State.Drops, State.Sim.Pad, DeltaTime, State.Field.Height, CaughtPowerups, MissedDrops);

        // Tick Powerups
        for (const PowerupType Type : CaughtPowerups)
        {
            const PowerupDef& Def = GetPowerupDefinition(Type);
            const Vector2 PaddleTop = {.x = State.Sim.Pad.Position.x + State.Sim.Pad.Width * 0.5f, .y = State.Sim.Pad.Position.y};
            SpawnParticles(State.Particles, PaddleTop, Def.Tint, 14);
            PlaySfx(PowerupSound);

            if (Def.Text[0] != '\0')
            {
                SpawnFloatingText(State.FloatingTexts, {.x = PaddleTop.x, .y = PaddleTop.y - 50.f}, Def.Text, Def.Tint, 42, 2.f, false);
            }

            if (Def.Duration > 0.f)
            {
                const auto FoundEffect = std::ranges::find_if(State.ActiveEffects, [Type](const ActiveEffect& E)
                {
                    return E.Type == Type;
                });

                if (FoundEffect != State.ActiveEffects.end())
                {
                    // Same effect durations stack
                    FoundEffect->TimeLeft += Def.Duration;
                }
                else
                {
                    if (Def.Apply)
                    {
                        Def.Apply(State);
                    }

                    State.ActiveEffects.push_back({.Type = Type, .TimeLeft = Def.Duration});
                }
            }
            else // Instant effects
            {
                if (Def.Apply)
                {
                    Def.Apply(State);
                }
            }
        }

        for (const Drop& MissedDrop : MissedDrops)
        {
            const Vector2 Center = {.x = MissedDrop.Position.x + MissedDrop.Width * 0.5f, .y = MissedDrop.Position.y + MissedDrop.Height * 0.5f};
            SpawnParticles(State.Particles, Center, GetPowerupDefinition(MissedDrop.Type).Tint, 32);
        }

        // Tick Active Effects (picked up Powerups)
        UpdateActiveEffects(State, DeltaTime);

        // Laser
        State.Sim.LaserCooldownSec -= DeltaTime;
        if (State.Sim.LaserActive && State.Sim.LaserCooldownSec <= 0.f)
        {
            State.Sim.LaserCooldownSec = Laser::FireCooldown;
            const Paddle& Pad = State.Sim.Pad;
            State.Sim.Lasers.push_back({.Position = {.x = Pad.Position.x + 8.f, .y = Pad.Position.y}});
            State.Sim.Lasers.push_back({.Position = {.x = Pad.Position.x + Pad.Width - 8.f, .y = Pad.Position.y}});

            SetSoundPitch(LaserSound, GetRandomFloatValue(0.85f, 1.15f));
            PlaySfx(LaserSound);
        }

        static std::vector<int> LaserHits;
        UpdateLasers(State.Sim, DeltaTime, LaserHits);
        for (const int HitIndex : LaserHits)
        {
            const Brick& Brick = State.Sim.Bricks[HitIndex];
            const Vector2 Center = {.x = Brick.Position.x + Brick.Width * 0.5f, .y = Brick.Position.y + Brick.Height * 0.5f};
            SpawnParticles(State.Particles, Center, Brick.Tint, Brick.Alive ? 4 : 32);

            if (!Brick.Alive)
            {
                SpawnPowerupDrop(State, Center);
            }
        }
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
            UpdateMenu(State, DeltaTime);
        }

        // UpdateMenu above can set GameScreen::Game
        if (State.Screen == GameScreen::Game)
        {
            UpdateGameplay(State, DeltaTime);
        }
        else if (State.Screen == GameScreen::GameOver || State.Screen == GameScreen::Victory)
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
    State.MouseControl = true;

    // Field size lives in virtual design units - window size never affects gameplay proportions.
    // Field.X/Y (centering inside the window) are re-derived every Tick in UpdateFrame,
    // but set them now too so pre-loop setup (cursor centering) starts correct
    State.Field.Height = VirtualHeight;
    State.Field.Width = FieldWidth;
    const ViewFit InitialFit = GetViewFit();
    State.Field.X = (InitialFit.ViewWidth - State.Field.Width) * 0.5f;
    State.Field.Y = (InitialFit.ViewHeight - State.Field.Height) * 0.5f;

    const float PadWidth = GetBasePadWidth(State);
    constexpr float PadHeight = 22.f;
    constexpr float PadBottomOffset = 4.f;

    // Spawn Pad in the Center
    State.Sim.Pad =
    {
        .Position = {.x = State.Field.Width * 0.5f - PadWidth * 0.5f, .y = State.Field.Height - PadHeight + PadBottomOffset},
        .VelocityX = 0.f,
        .Width = PadWidth,
        .Height = PadHeight,
        .FlashSec = 0.f
    };

    // By default, we use Mouse control, so we need to center Cursor to the Pad, so Pad is not snapping to the current random Cursor position
    ResetMouseCursor(State);

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
