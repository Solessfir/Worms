// https://www.raylib.com/cheatsheet/cheatsheet.html

#include "Game.h"
#include "raylib/raylib.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace
{
    Music MenuMusic;
    Music GameplayMusic;
    Music VictoryMusic;
    bool QuitRequested = false;

    const char* AssetPath(const char* Relative)
    {
        #if RELEASE
        return TextFormat("%s%s", GetApplicationDirectory(), Relative);
        #else
        return Relative;
        #endif
    }

    std::uint32_t MakeSeed()
    {
        return std::random_device{}();
    }

    BiomeKind PickBiome(const int MenuBiome, const std::uint32_t Seed)
    {
        return MenuBiome < 0 ? static_cast<BiomeKind>(Seed % static_cast<std::uint32_t>(BiomeKind::Count)) : static_cast<BiomeKind>(MenuBiome);
    }

    // The menu sits on top of a live map that slowly pans, so picking a biome previews it
    void ShowMenu(GameState& State)
    {
        const std::uint32_t Seed = MakeSeed();
        StartMatch(State, Seed, PickBiome(State.MenuBiome, Seed));
        State.Screen = GameScreen::MainMenu;
        State.Camera.Zoom = 0.75f;
    }

    void UpdateMenu(GameState& State, const float DeltaTime)
    {
        State.Game.Time += DeltaTime;
        State.Camera.Target = {
            .x = static_cast<float>(MapWidth) * 0.5f + std::sin(State.Game.Time * 0.06f) * static_cast<float>(MapWidth) * 0.32f,
            .y = State.Game.LiquidLevel - 420.f
        };

        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            return;
        }

        if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(0)))
        {
            StopMusicStream(MenuMusic);
            PlayMusicStream(GameplayMusic);
            const std::uint32_t Seed = MakeSeed();
            StartMatch(State, Seed, PickBiome(State.MenuBiome, Seed));
        }
        else if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(1)))
        {
            State.MenuBiome = State.MenuBiome + 1 >= static_cast<int>(BiomeKind::Count) ? -1 : State.MenuBiome + 1;
            ShowMenu(State);
        }
        else if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(2)))
        {
            QuitRequested = true;
        }
    }

    void ReturnToMenu(GameState& State)
    {
        StopMusicStream(GameplayMusic);
        StopMusicStream(VictoryMusic);
        PlayMusicStream(MenuMusic);
        State.Paused = false;
        ShowMenu(State);
    }

    // Returns true while the game is paused, so the caller skips the simulation
    bool UpdatePause(GameState& State)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            State.Paused = !State.Paused;
            State.WeaponPanelOpen = false;
            State.Game.Charging = false;
            State.Game.Charge = 0.f;
            // The click that resumes must not also fire a weapon
            State.WaitForMouseRelease = true;
        }

        if (!State.Paused)
        {
            return false;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(0)))
            {
                State.Paused = false;
                State.WaitForMouseRelease = true;
            }
            else if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(1)))
            {
                ReturnToMenu(State);
            }
            else if (CheckCollisionPointRec(GetMousePosition(), GetMenuButtonRect(2)))
            {
                QuitRequested = true;
            }
        }
        return true;
    }

    void UpdateVictory(GameState& State, const float DeltaTime)
    {
        State.Game.Time += DeltaTime;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
        {
            ReturnToMenu(State);
        }
    }
}

int main()
{
    // ---------- Window Setup ----------
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    #if RELEASE
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    #endif

    InitWindow(1600, 900, "Worms");
    SetWindowMinSize(640, 360);
    // Esc opens the pause menu instead of closing the window
    SetExitKey(KEY_NULL);

    const int Monitor = GetCurrentMonitor();
    SetTargetFPS(GetMonitorRefreshRate(Monitor));

    #if RELEASE
    // Raylib Fullscreen and Alt+Tab are broken on Windows
    // https://github.com/raysan5/raylib/issues/3865
    // +1 px to not trigger Windows "fullscreen optimization"
    SetWindowSize(GetMonitorWidth(Monitor) + 1, GetMonitorHeight(Monitor) + 1);
    SetWindowPosition(0, 0);
    #endif

    // The game draws its own cursor in screen space
    HideCursor();

    // ---------- Audio Setup ----------
    InitAudioDevice();
    MenuMusic = LoadMusicStream(AssetPath("assets/audio/menu.mp3"));
    GameplayMusic = LoadMusicStream(AssetPath("assets/audio/soundtrack.mp3"));
    VictoryMusic = LoadMusicStream(AssetPath("assets/audio/victory.mp3"));
    const Sound ExplosionSound = LoadSound(AssetPath("assets/audio/explosion.mp3"));

    SetMusicVolume(MenuMusic, 0.35f);
    SetMusicVolume(GameplayMusic, 0.35f);
    SetMusicVolume(VictoryMusic, 0.35f);
    SetSoundVolume(ExplosionSound, 0.5f);
    PlayMusicStream(MenuMusic);

    // ---------- Game Setup ----------
    GameState State;
    State.Sounds.Explosion = ExplosionSound;
    ShowMenu(State);

    // ---------- Main Loop ----------
    while (!WindowShouldClose() && !QuitRequested)
    {
        // Clamp so a window drag or breakpoint doesn't fling everything across the map
        const float DeltaTime = std::min(GetFrameTime(), 0.1f);

        switch (State.Screen)
        {
            case GameScreen::MainMenu: UpdateMenu(State, DeltaTime); break;
            case GameScreen::Game:
                if (UpdatePause(State))
                {
                    break;
                }
                UpdateGameplay(State, DeltaTime);
                if (State.Screen == GameScreen::Victory)
                {
                    StopMusicStream(GameplayMusic);
                    PlayMusicStream(VictoryMusic);
                }
                break;
            case GameScreen::Victory: UpdateVictory(State, DeltaTime); break;
        }

        if (!State.Paused)
        {
            UpdateEffects(State, DeltaTime);
        }
        UpdateMusicStream(MenuMusic);
        UpdateMusicStream(GameplayMusic);
        UpdateMusicStream(VictoryMusic);

        RenderFrame(State);
    }

    DestroyMatch(State.Game);
    UnloadMusicStream(MenuMusic);
    UnloadMusicStream(GameplayMusic);
    UnloadMusicStream(VictoryMusic);
    UnloadSound(ExplosionSound);
    CloseAudioDevice();
    CloseWindow();
}
