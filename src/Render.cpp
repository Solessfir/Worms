#include "Game.h"
#include "raylib/raymath.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
    constexpr Color Skin = {.r = 246, .g = 166, .b = 172, .a = 255};
    constexpr Color SkinShade = {.r = 214, .g = 122, .b = 138, .a = 255};
    constexpr Color Outline = {.r = 58, .g = 28, .b = 40, .a = 255};
    constexpr Color HudPanel = {.r = 16, .g = 18, .b = 26, .a = 200};

    void DrawTextOutlined(const char* Text, const int X, const int Y, const int Size, const Color Tint, const Color Edge = BLACK)
    {
        const int O = std::max(1, Size / 14);
        DrawText(Text, X - O, Y, Size, Edge);
        DrawText(Text, X + O, Y, Size, Edge);
        DrawText(Text, X, Y - O, Size, Edge);
        DrawText(Text, X, Y + O, Size, Edge);
        DrawText(Text, X, Y, Size, Tint);
    }

    void DrawTextCentered(const char* Text, const float CenterX, const float Y, const int Size, const Color Tint)
    {
        DrawTextOutlined(Text, static_cast<int>(CenterX) - MeasureText(Text, Size) / 2, static_cast<int>(Y), Size, Tint);
    }

    // Rotated rectangle anchored at Pivot, extending from Back to Front along Angle
    void DrawBar(const Vector2 Pivot, const float Angle, const float Back, const float Front, const float Thickness, const Color Tint)
    {
        const Rectangle Rect = {.x = Pivot.x, .y = Pivot.y, .width = Front - Back, .height = Thickness};
        DrawRectanglePro(Rect, {.x = -Back, .y = Thickness * 0.5f}, Angle * RAD2DEG, Tint);
    }

    Vector2 Along(const Vector2 Pivot, const float Angle, const float Distance)
    {
        return {.x = Pivot.x + std::cos(Angle) * Distance, .y = Pivot.y + std::sin(Angle) * Distance};
    }

    // ---------- Weapons ----------

    // Where the worm holds its weapon; the charge wedge grows from here so it lines up with the barrel
    Vector2 GetWeaponPivot(const Worm& W)
    {
        return {.x = W.DrawPosition.x + W.FacingDir * 4.f, .y = W.DrawPosition.y - 3.f};
    }

    void DrawWeaponArt(const WeaponKind Kind, const Vector2 Pivot, const float Angle, const float Scale)
    {
        const float S = Scale;
        switch (Kind)
        {
            case WeaponKind::Rocket:
            {
                DrawBar(Pivot, Angle, -8.f * S, 16.f * S, 7.f * S, {.r = 70, .g = 84, .b = 52, .a = 255});
                DrawBar(Pivot, Angle, -8.f * S, 16.f * S, 3.f * S, {.r = 104, .g = 122, .b = 72, .a = 255});
                DrawBar(Pivot, Angle, 12.f * S, 17.f * S, 8.f * S, {.r = 40, .g = 44, .b = 36, .a = 255});
                DrawBar(Pivot, Angle, -2.f * S, 1.f * S, 8.f * S, {.r = 210, .g = 60, .b = 50, .a = 255});
                break;
            }
            case WeaponKind::Grenade:
            case WeaponKind::ClusterBomb:
            {
                const Vector2 Hand = Along(Pivot, Angle, 6.f * S);
                const Color Body = Kind == WeaponKind::Grenade ? Color{.r = 60, .g = 110, .b = 52, .a = 255} : Color{.r = 196, .g = 54, .b = 46, .a = 255};
                DrawCircleV(Hand, 5.f * S, Outline);
                DrawCircleV(Hand, 4.2f * S, Body);
                DrawCircleV({.x = Hand.x - 1.5f * S, .y = Hand.y - 1.5f * S}, 1.4f * S, ColorAlpha(WHITE, 0.5f));
                DrawRectangleV({.x = Hand.x - 1.5f * S, .y = Hand.y - 7.f * S}, {.x = 3.f * S, .y = 3.f * S}, GRAY);
                if (Kind == WeaponKind::ClusterBomb)
                {
                    DrawCircleV({.x = Hand.x + 1.5f * S, .y = Hand.y + 1.f * S}, 1.f * S, YELLOW);
                }
                break;
            }
            case WeaponKind::Airstrike:
            {
                const Vector2 Hand = Along(Pivot, Angle, 6.f * S);
                DrawRectangleRec({.x = Hand.x - 4.f * S, .y = Hand.y - 5.f * S, .width = 8.f * S, .height = 10.f * S}, {.r = 70, .g = 74, .b = 82, .a = 255});
                DrawRectangleRec({.x = Hand.x - 2.5f * S, .y = Hand.y - 3.f * S, .width = 5.f * S, .height = 3.f * S}, {.r = 120, .g = 230, .b = 140, .a = 255});
                DrawLineEx({.x = Hand.x + 2.f * S, .y = Hand.y - 5.f * S}, {.x = Hand.x + 4.f * S, .y = Hand.y - 13.f * S}, 1.5f * S, DARKGRAY);
                DrawCircleV({.x = Hand.x + 4.f * S, .y = Hand.y - 13.f * S}, 1.6f * S, RED);
                break;
            }
            case WeaponKind::NinjaRope:
            {
                const Vector2 Hand = Along(Pivot, Angle, 6.f * S);
                DrawRing(Hand, 2.f * S, 5.f * S, 0.f, 360.f, 16, {.r = 150, .g = 104, .b = 60, .a = 255});
                DrawRing(Hand, 2.6f * S, 3.4f * S, 0.f, 360.f, 16, {.r = 110, .g = 74, .b = 40, .a = 255});
                DrawBar(Pivot, Angle, 9.f * S, 14.f * S, 3.f * S, LIGHTGRAY);
                break;
            }
            case WeaponKind::Teleport:
            {
                const Vector2 Hand = Along(Pivot, Angle, 7.f * S);
                DrawCircleV(Hand, 5.5f * S, ColorAlpha({.r = 120, .g = 220, .b = 255, .a = 255}, 0.35f));
                DrawRectangleRec({.x = Hand.x - 3.f * S, .y = Hand.y - 4.f * S, .width = 6.f * S, .height = 8.f * S}, {.r = 200, .g = 204, .b = 214, .a = 255});
                DrawCircleV(Hand, 2.f * S, {.r = 120, .g = 220, .b = 255, .a = 255});
                break;
            }
            case WeaponKind::Shotgun:
            {
                DrawBar(Pivot, Angle, -6.f * S, 2.f * S, 5.f * S, {.r = 120, .g = 76, .b = 40, .a = 255});
                DrawBar(Pivot, Angle, 0.f, 20.f * S, 3.f * S, {.r = 90, .g = 94, .b = 104, .a = 255});
                DrawBar(Pivot, Angle, 4.f * S, 10.f * S, 4.f * S, {.r = 140, .g = 92, .b = 50, .a = 255});
                break;
            }
            default: break;
        }
    }

    // ---------- Worm ----------

    void DrawWormCharacter(const Match& Game, const Worm& W, const bool IsActive)
    {
        const Team& T = Game.Teams[static_cast<size_t>(W.Team)];
        const float F = W.FacingDir;
        const Vector2 P = W.DrawPosition;
        const float Flash = W.FlashSec / Worm::FlashDuration;
        const Color Body = ColorLerp(Skin, WHITE, Flash);
        const Color Belly = ColorLerp(ColorLerp(Skin, WHITE, 0.35f), WHITE, Flash);

        if (W.Motion == WormMotion::Knocked)
        {
            // Tumbling: curled into a ball
            DrawCircleV(P, Worm::Radius + 1.2f, Outline);
            DrawCircleV(P, Worm::Radius, Body);
            DrawRing(P, 3.f, 5.f, W.SpinAngle * RAD2DEG, W.SpinAngle * RAD2DEG + 260.f, 12, SkinShade);
            const Vector2 Eye = Along(P, W.SpinAngle, 5.f);
            DrawCircleV(Eye, 2.6f, WHITE);
            DrawCircleV(Eye, 1.2f, BLACK);
            return;
        }

        // Body: a chain of overlapping circles from the tail on the ground up to the head.
        // Walking ripples the chain, idle breathes.
        const bool Walking = W.Motion == WormMotion::Walking && W.MoveInput != 0.f;
        constexpr int Segments = 6;
        const Vector2 Base[Segments] = {{.x = -10.f, .y = 6.f}, {.x = -6.5f, .y = 5.5f}, {.x = -2.5f, .y = 3.5f}, {.x = 0.5f, .y = -0.5f}, {.x = 1.8f, .y = -5.5f}, {.x = 2.4f, .y = -11.5f}};
        const float Radii[Segments] = {3.2f, 4.4f, 5.6f, 6.4f, 6.9f, 7.4f};

        Vector2 Points[Segments];
        for (int i = 0; i < Segments; ++i)
        {
            const float Phase = W.AnimTime * (Walking ? 14.f : 2.4f) - static_cast<float>(i) * 0.9f;
            const float Bob = Walking ? std::sin(Phase) * 1.4f * (1.f - static_cast<float>(i) / Segments) : std::sin(Phase) * 0.5f * static_cast<float>(i) / Segments;
            Points[i] = {.x = P.x + Base[i].x * F, .y = P.y + Base[i].y + Bob};
        }

        for (int i = 0; i < Segments; ++i)
        {
            DrawCircleV(Points[i], Radii[i] + 1.3f, Outline);
        }
        for (int i = 0; i < Segments; ++i)
        {
            DrawCircleV(Points[i], Radii[i], Body);
            // Belly on the front side
            DrawCircleV({.x = Points[i].x + F * Radii[i] * 0.35f, .y = Points[i].y + Radii[i] * 0.15f}, Radii[i] * 0.55f, Belly);
        }
        // Back shading reads as volume
        for (int i = 1; i < Segments - 1; ++i)
        {
            DrawCircleV({.x = Points[i].x - F * Radii[i] * 0.45f, .y = Points[i].y}, Radii[i] * 0.4f, ColorAlpha(SkinShade, 0.6f));
        }

        const Vector2 Head = Points[Segments - 1];

        // Team bandana with tails fluttering behind
        const float Flutter = std::sin(W.AnimTime * 9.f) * 1.5f;
        DrawLineEx({.x = Head.x - F * 6.f, .y = Head.y - 3.f}, {.x = Head.x - F * 13.f, .y = Head.y - 1.f + Flutter}, 2.4f, T.Tint);
        DrawLineEx({.x = Head.x - F * 6.f, .y = Head.y - 3.f}, {.x = Head.x - F * 12.f, .y = Head.y + 3.f - Flutter}, 2.f, ColorLerp(T.Tint, BLACK, 0.2f));
        DrawRectanglePro({.x = Head.x, .y = Head.y - 4.2f, .width = 15.f, .height = 3.4f}, {.x = 7.5f, .y = 1.7f}, -F * 8.f, T.Tint);

        // Eyes track the aim while it's this worm's turn
        const float Look = IsActive ? W.AimAngle : (F > 0.f ? 0.f : PI);
        const Vector2 PupilOffset = {.x = std::cos(Look) * 1.3f, .y = std::sin(Look) * 1.3f};
        const bool Blink = std::fmod(W.AnimTime + static_cast<float>(W.Id) * 0.7f, 3.7f) < 0.12f;
        for (const float Side : {-1.f, 1.f})
        {
            const Vector2 Eye = {.x = Head.x + F * 2.6f + Side * 2.7f, .y = Head.y - 0.5f};
            DrawCircleV(Eye, 3.3f, Outline);
            DrawCircleV(Eye, 2.8f, WHITE);
            if (Blink)
            {
                DrawRectangleV({.x = Eye.x - 3.f, .y = Eye.y - 3.f}, {.x = 6.f, .y = 3.5f}, Body);
                DrawLineEx({.x = Eye.x - 2.8f, .y = Eye.y + 0.5f}, {.x = Eye.x + 2.8f, .y = Eye.y + 0.5f}, 1.f, Outline);
            }
            else
            {
                DrawCircleV(Vector2Add(Eye, PupilOffset), 1.4f, BLACK);
            }
        }

        // Smile; worried when low on health
        const Vector2 Mouth = {.x = Head.x + F * 3.6f, .y = Head.y + 4.f};
        if (W.HP > 30)
        {
            DrawRing(Mouth, 1.6f, 2.4f, 20.f, 160.f, 8, Outline);
        }
        else
        {
            DrawRing({.x = Mouth.x, .y = Mouth.y + 2.f}, 1.6f, 2.4f, 200.f, 340.f, 8, Outline);
        }

        const bool HoldsWeapon = IsActive && Game.Phase == TurnPhase::Aiming && (W.Motion == WormMotion::Idle || W.Motion == WormMotion::Roping);
        if (HoldsWeapon)
        {
            const Vector2 Pivot = GetWeaponPivot(W);
            DrawWeaponArt(Game.Weapon, Pivot, W.AimAngle, 1.f);
        }
    }

    void DrawGrave(const Grave& G)
    {
        const float Degrees = G.DrawAngle * RAD2DEG;
        DrawRectanglePro({.x = G.DrawPosition.x, .y = G.DrawPosition.y, .width = 13.f, .height = 17.f}, {.x = 6.5f, .y = 8.5f}, Degrees, Outline);
        DrawRectanglePro({.x = G.DrawPosition.x, .y = G.DrawPosition.y, .width = 11.f, .height = 15.f}, {.x = 5.5f, .y = 7.5f}, Degrees, {.r = 150, .g = 150, .b = 158, .a = 255});
        DrawRectanglePro({.x = G.DrawPosition.x, .y = G.DrawPosition.y, .width = 2.f, .height = 9.f}, {.x = 1.f, .y = 6.f}, Degrees, G.Tint);
        DrawRectanglePro({.x = G.DrawPosition.x, .y = G.DrawPosition.y, .width = 7.f, .height = 2.f}, {.x = 3.5f, .y = 4.f}, Degrees, G.Tint);
    }

    void DrawProjectile(const Projectile& P)
    {
        switch (P.Kind)
        {
            case ProjectileKind::Rocket:
            case ProjectileKind::AirstrikeBomb:
            {
                const Color Shell = P.Kind == ProjectileKind::Rocket ? Color{.r = 96, .g = 110, .b = 70, .a = 255} : Color{.r = 70, .g = 72, .b = 80, .a = 255};
                DrawBar(P.DrawPosition, P.DrawAngle, -8.f, 7.f, 6.f, Outline);
                DrawBar(P.DrawPosition, P.DrawAngle, -7.f, 6.f, 4.5f, Shell);
                DrawBar(P.DrawPosition, P.DrawAngle, 4.f, 8.f, 4.f, {.r = 214, .g = 64, .b = 50, .a = 255});
                DrawBar(P.DrawPosition, P.DrawAngle, -9.f, -5.f, 9.f, ColorLerp(Shell, BLACK, 0.3f));
                if (P.Kind == ProjectileKind::Rocket)
                {
                    DrawCircleV(Along(P.DrawPosition, P.DrawAngle, -11.f), 3.f + GetRandomFloatValue(0.f, 1.5f), {.r = 255, .g = 180, .b = 60, .a = 220});
                }
                break;
            }
            case ProjectileKind::Grenade:
            case ProjectileKind::ClusterBomb:
            {
                DrawWeaponArt(P.Kind == ProjectileKind::Grenade ? WeaponKind::Grenade : WeaponKind::ClusterBomb, Along(P.DrawPosition, P.DrawAngle, -6.f), P.DrawAngle, 1.f);
                DrawTextCentered(TextFormat("%d", static_cast<int>(std::ceil(P.Fuse))), P.DrawPosition.x, P.DrawPosition.y - 22.f, 10, WHITE);
                break;
            }
            case ProjectileKind::Bomblet:
            {
                DrawCircleV(P.DrawPosition, 4.5f, ColorAlpha(ORANGE, 0.4f));
                DrawCircleV(P.DrawPosition, 3.5f, Outline);
                DrawCircleV(P.DrawPosition, 2.6f, {.r = 196, .g = 54, .b = 46, .a = 255});
                break;
            }
        }
    }

    void DrawParticles(const std::vector<Particle>& Particles, const ParticleKind Kind)
    {
        for (const Particle& P : Particles)
        {
            if (P.Kind != Kind)
            {
                continue;
            }
            const float Life = P.Lifetime / P.MaxLifetime;
            switch (Kind)
            {
                case ParticleKind::Smoke:
                    DrawCircleV(P.Position, P.InitialRadius * (2.6f - 1.6f * Life), ColorAlpha(P.Tint, Life * static_cast<float>(P.Tint.a) / 255.f));
                    break;
                case ParticleKind::Spark:
                    DrawCircleV(P.Position, P.InitialRadius * Life, ColorAlpha(P.Tint, Life));
                    break;
                case ParticleKind::Dirt:
                    DrawRectangleV({.x = P.Position.x - P.InitialRadius * 0.5f, .y = P.Position.y - P.InitialRadius * 0.5f}, {.x = P.InitialRadius, .y = P.InitialRadius}, ColorAlpha(P.Tint, std::min(1.f, Life * 2.f)));
                    break;
            }
        }
    }

    // ---------- World overlays ----------

    // raylib culls triangles by winding; the wedge flips winding with the aim direction, so submit both
    void DrawQuadBothSides(const Vector2 A, const Vector2 B, const Vector2 C, const Vector2 D, const Color Tint)
    {
        DrawTriangle(A, B, C, Tint);
        DrawTriangle(A, C, B, Tint);
        DrawTriangle(A, C, D, Tint);
        DrawTriangle(A, D, C, Tint);
    }

    // Tapered wedge along the aim: a faint full-power outline, filled yellow to red up to the current charge
    void DrawChargeWedge(const Vector2 Origin, const float Angle, const float Charge, const float Time)
    {
        constexpr float Start = 20.f; // just past the longest barrel (rocket launcher ends at 17)
        constexpr float Length = 78.f;
        constexpr float MinHalf = 1.5f;
        constexpr float MaxHalf = 10.f;
        constexpr int Segments = 28;

        const Vector2 Dir = {.x = std::cos(Angle), .y = std::sin(Angle)};
        const Vector2 Side = {.x = -Dir.y, .y = Dir.x};
        auto Edge = [&](const float T, const float Sign, const float Grow)
        {
            const float Half = Lerp(MinHalf, MaxHalf, T) + Grow;
            const Vector2 Center = Vector2Add(Origin, Vector2Scale(Dir, Start + T * Length));
            return Vector2Add(Center, Vector2Scale(Side, Half * Sign));
        };

        // Ghost of the full wedge so the fill reads as a fraction
        DrawQuadBothSides(Edge(0.f, -1.f, 1.5f), Edge(0.f, 1.f, 1.5f), Edge(1.f, 1.f, 1.5f), Edge(1.f, -1.f, 1.5f), ColorAlpha(BLACK, 0.35f));
        DrawQuadBothSides(Edge(0.f, -1.f, 0.f), Edge(0.f, 1.f, 0.f), Edge(1.f, 1.f, 0.f), Edge(1.f, -1.f, 0.f), ColorAlpha(WHITE, 0.12f));

        const Color Low = {.r = 255, .g = 226, .b = 70, .a = 255};
        const Color Mid = {.r = 255, .g = 140, .b = 30, .a = 255};
        const Color High = {.r = 236, .g = 40, .b = 30, .a = 255};
        auto Ramp = [&](const float T)
        {
            return T < 0.5f ? ColorLerp(Low, Mid, T * 2.f) : ColorLerp(Mid, High, (T - 0.5f) * 2.f);
        };

        const int Filled = static_cast<int>(std::ceil(Charge * Segments));
        for (int i = 0; i < Filled; ++i)
        {
            const float T0 = static_cast<float>(i) / Segments;
            const float T1 = std::min(Charge, static_cast<float>(i + 1) / Segments);
            DrawQuadBothSides(Edge(T0, -1.f, 0.f), Edge(T0, 1.f, 0.f), Edge(T1, 1.f, 0.f), Edge(T1, -1.f, 0.f), ColorAlpha(Ramp(T1), 0.7f));
        }

        // Bright leading edge, pulsing once nearly full
        const float Pulse = Charge > 0.9f ? 0.5f + 0.5f * std::sin(Time * 30.f) : 0.f;
        DrawLineEx(Edge(Charge, -1.f, 1.f), Edge(Charge, 1.f, 1.f), 2.f + Pulse * 1.5f, ColorAlpha(ColorLerp(WHITE, Ramp(Charge), 0.3f), 0.85f));

        // Quarter ticks make power levels repeatable between turns
        for (const float Tick : {0.25f, 0.5f, 0.75f})
        {
            DrawLineEx(Edge(Tick, -1.f, 2.f), Edge(Tick, 1.f, 2.f), 1.f, ColorAlpha(WHITE, Tick <= Charge ? 0.55f : 0.3f));
        }
    }

    void DrawAimOverlay(const GameState& State, const Vector2 MouseWorld)
    {
        const Match& Game = State.Game;
        const Worm* Active = GetActiveWorm(Game);
        if (!Active || Game.Phase != TurnPhase::Aiming || State.Screen != GameScreen::Game)
        {
            return;
        }

        const Color TeamTint = Game.Teams[static_cast<size_t>(Active->Team)].Tint;
        const WeaponInfo& Info = GetWeaponInfo(Game.Weapon);

        if (B2_IS_NON_NULL(Game.Rope))
        {
            DrawCircleV(Game.RopeAnchor, 3.f, LIGHTGRAY);
            return;
        }

        if (Info.Mode == AimMode::Target)
        {
            if (Game.Weapon == WeaponKind::Teleport)
            {
                const bool Valid = IsCircleFree(Game.Terrain, MouseWorld, Worm::Radius + 2.f) && MouseWorld.y < Game.LiquidLevel - 30.f;
                DrawRing(MouseWorld, Worm::Radius - 1.f, Worm::Radius + 1.f, 0.f, 360.f, 24, Valid ? Color{.r = 120, .g = 230, .b = 255, .a = 255} : RED);
            }
            else
            {
                const float Bounce = std::sin(Game.Time * 6.f) * 4.f;
                DrawTriangle({.x = MouseWorld.x - 8.f, .y = MouseWorld.y - 26.f + Bounce}, {.x = MouseWorld.x, .y = MouseWorld.y - 12.f + Bounce}, {.x = MouseWorld.x + 8.f, .y = MouseWorld.y - 26.f + Bounce}, TeamTint);
                DrawRing(MouseWorld, 9.f, 11.f, 0.f, 360.f, 24, TeamTint);
            }
            return;
        }

        // Crosshair at a fixed distance along the aim, like a sight
        const Vector2 Sight = Along(Active->DrawPosition, Active->AimAngle, 52.f);
        DrawRing(Sight, 5.f, 7.f, 0.f, 360.f, 20, TeamTint);
        DrawCircleV(Sight, 1.5f, TeamTint);

        if (Game.Weapon == WeaponKind::NinjaRope)
        {
            for (float D = 20.f; D < 420.f; D += 14.f)
            {
                DrawCircleV(Along(Active->DrawPosition, Active->AimAngle, D), 1.2f, ColorAlpha(WHITE, 0.35f));
            }
        }

        if (Game.Charging)
        {
            DrawChargeWedge(GetWeaponPivot(*Active), Active->AimAngle, Game.Charge, Game.Time);
        }
    }

    void DrawWorld(const GameState& State, const Camera2D& View)
    {
        const Match& Game = State.Game;
        const BiomeTheme& Theme = GetBiomeTheme(Game.Biome);
        const Rectangle Visible = GetVisibleWorld(View);

        DrawLiquid(Theme, Game.LiquidLevel, Game.Time, Visible, false);

        DrawTexture(Game.Terrain.Texture, 0, 0, WHITE);

        for (const Debris& D : Game.DebrisPieces)
        {
            const float W = static_cast<float>(D.Width);
            const float H = static_cast<float>(D.Height);
            DrawTexturePro(D.Texture, {.x = 0.f, .y = 0.f, .width = W, .height = H}, {.x = D.DrawPosition.x, .y = D.DrawPosition.y, .width = W, .height = H}, {.x = W * 0.5f, .y = H * 0.5f}, D.DrawAngle * RAD2DEG, WHITE);
        }

        for (const Grave& G : Game.Graves)
        {
            DrawGrave(G);
        }

        if (B2_IS_NON_NULL(Game.Rope))
        {
            if (const Worm* Active = GetActiveWorm(Game))
            {
                DrawLineEx(Active->DrawPosition, Game.RopeAnchor, 2.f, {.r = 150, .g = 104, .b = 60, .a = 255});
            }
        }

        // Smoke sits behind worms and projectiles so trails and blasts never hide them
        DrawParticles(State.Particles, ParticleKind::Smoke);

        const Worm* Active = GetActiveWorm(Game);
        for (const Worm& W : Game.Worms)
        {
            if (W.Alive)
            {
                DrawWormCharacter(Game, W, &W == Active && State.Screen == GameScreen::Game);
            }
        }

        for (const Projectile& P : Game.Projectiles)
        {
            DrawProjectile(P);
        }

        for (const Tracer& T : State.Tracers)
        {
            DrawLineEx(T.From, T.To, 2.f, ColorAlpha({.r = 255, .g = 240, .b = 180, .a = 255}, T.Lifetime / 0.2f));
        }

        DrawParticles(State.Particles, ParticleKind::Dirt);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawParticles(State.Particles, ParticleKind::Spark);
        EndBlendMode();

        DrawAimOverlay(State, GetScreenToWorld2D(GetMousePosition(), View));

        DrawLiquid(Theme, Game.LiquidLevel, Game.Time, Visible, true);
    }

    // ---------- Screen overlays ----------

    void DrawWormLabels(const GameState& State, const Camera2D& View)
    {
        const Match& Game = State.Game;
        const float Ui = GetUiScale();
        const int NameSize = static_cast<int>(18.f * Ui);
        const int HpSize = static_cast<int>(18.f * Ui);
        const Worm* Active = GetActiveWorm(Game);

        for (const Worm& W : Game.Worms)
        {
            // The active worm's label sits right where you aim upward; it's noise mid-shot
            const bool ChargingShot = &W == Active && Game.Charging;
            if (!W.Alive || ChargingShot)
            {
                continue;
            }

            const Color Tint = Game.Teams[static_cast<size_t>(W.Team)].Tint;
            const Vector2 Anchor = GetWorldToScreen2D({.x = W.DrawPosition.x, .y = W.DrawPosition.y - 22.f}, View);

            const char* Hp = TextFormat("%d", W.HP);
            const float HpW = static_cast<float>(MeasureText(Hp, HpSize)) + 10.f * Ui;
            const Rectangle HpBox = {.x = Anchor.x - HpW * 0.5f, .y = Anchor.y - static_cast<float>(HpSize) - 6.f * Ui, .width = HpW, .height = static_cast<float>(HpSize) + 4.f * Ui};
            DrawRectangleRounded(HpBox, 0.35f, 6, HudPanel);
            DrawText(Hp, static_cast<int>(HpBox.x + 5.f * Ui), static_cast<int>(HpBox.y + 2.f * Ui), HpSize, Tint);

            DrawTextCentered(W.Name, Anchor.x, HpBox.y - static_cast<float>(NameSize) - 2.f * Ui, NameSize, Tint);

            if (&W == Active && Game.Phase == TurnPhase::Aiming && State.Screen == GameScreen::Game)
            {
                const float Bounce = std::sin(Game.Time * 6.f) * 5.f * Ui;
                const float Top = HpBox.y - static_cast<float>(NameSize) - 34.f * Ui + Bounce;
                DrawTriangle({.x = Anchor.x - 10.f * Ui, .y = Top}, {.x = Anchor.x, .y = Top + 14.f * Ui}, {.x = Anchor.x + 10.f * Ui, .y = Top}, Tint);
            }
        }
    }

    void DrawFloatingTexts(const GameState& State, const Camera2D& View)
    {
        const float Ui = GetUiScale();
        for (const FloatingText& T : State.FloatingTexts)
        {
            const float Frac = T.Lifetime / T.MaxLifetime;
            Vector2 Screen = GetWorldToScreen2D(T.Position, View);
            if (T.Shake)
            {
                Screen.x += GetRandomFloatValue(-1.f, 1.f) * 4.f * Frac;
                Screen.y += GetRandomFloatValue(-1.f, 1.f) * 4.f * Frac;
            }
            const int Size = static_cast<int>(static_cast<float>(T.FontSize) * Ui);
            DrawTextOutlined(T.Text, static_cast<int>(Screen.x) - MeasureText(T.Text, Size) / 2, static_cast<int>(Screen.y), Size, ColorAlpha(T.Tint, Frac), ColorAlpha(BLACK, Frac));
        }
    }

    void DrawHud(const GameState& State)
    {
        const Match& Game = State.Game;
        const float Ui = GetUiScale();
        const float ScreenW = static_cast<float>(GetScreenWidth());
        const float ScreenH = static_cast<float>(GetScreenHeight());
        const Team& Current = Game.Teams[static_cast<size_t>(Game.CurrentTeam)];
        const Worm* Active = GetActiveWorm(Game);

        // Turn banner
        if (Active)
        {
            const int Size = static_cast<int>(30.f * Ui);
            DrawTextCentered(TextFormat("%s - %s", Current.Name, Active->Name), ScreenW * 0.5f, 18.f * Ui, Size, Current.Tint);

            const WeaponInfo& Info = GetWeaponInfo(Game.Weapon);
            const bool Fused = Game.Weapon == WeaponKind::Grenade || Game.Weapon == WeaponKind::ClusterBomb;
            const int Ammo = Current.Ammo[static_cast<size_t>(Game.Weapon)];
            const char* WeaponText = Fused ? TextFormat("%s  (fuse %ds)", Info.Name, Game.FuseSeconds) : (Ammo > 0 ? TextFormat("%s  x%d", Info.Name, Ammo) : Info.Name);
            DrawTextCentered(WeaponText, ScreenW * 0.5f, 18.f * Ui + static_cast<float>(Size) + 6.f * Ui, static_cast<int>(20.f * Ui), RAYWHITE);
        }

        // Turn timer
        {
            const Rectangle Box = {.x = 24.f * Ui, .y = ScreenH - 96.f * Ui, .width = 92.f * Ui, .height = 72.f * Ui};
            DrawRectangleRounded(Box, 0.25f, 8, HudPanel);
            DrawRectangleRoundedLinesEx(Box, 0.25f, 8, 3.f * Ui, Current.Tint);

            const char* Text = "--";
            Color Tint = RAYWHITE;
            if (Game.Phase == TurnPhase::Aiming)
            {
                const int Seconds = static_cast<int>(std::ceil(std::max(0.f, Game.TurnTimer)));
                Text = TextFormat("%d", Seconds);
                Tint = Seconds <= 10 && std::fmod(Game.Time, 1.f) < 0.5f ? RED : RAYWHITE;
            }
            else if (Game.Phase == TurnPhase::Retreat)
            {
                Text = TextFormat("%d", static_cast<int>(std::ceil(Game.PhaseTimer)));
                Tint = YELLOW;
            }
            const int Size = static_cast<int>(44.f * Ui);
            DrawText(Text, static_cast<int>(Box.x + (Box.width - static_cast<float>(MeasureText(Text, Size))) * 0.5f), static_cast<int>(Box.y + (Box.height - static_cast<float>(Size)) * 0.5f), Size, Tint);
        }

        // Team health bars
        {
            const float BarMaxW = 320.f * Ui;
            const float BarH = 16.f * Ui;
            const int NameSize = static_cast<int>(18.f * Ui);
            for (int t = 0; t < TeamCount; ++t)
            {
                int Total = 0;
                for (const Worm& W : Game.Worms)
                {
                    if (W.Team == t && W.Alive)
                    {
                        Total += W.HP;
                    }
                }
                const Team& T = Game.Teams[static_cast<size_t>(t)];
                const float Y = ScreenH - 78.f * Ui + static_cast<float>(t) * (BarH + 14.f * Ui);
                const float X = ScreenW * 0.5f - BarMaxW * 0.5f + 70.f * Ui;
                DrawTextOutlined(T.Name, static_cast<int>(X - static_cast<float>(MeasureText(T.Name, NameSize)) - 12.f * Ui), static_cast<int>(Y), NameSize, T.Tint);
                DrawRectangleRounded({.x = X - 2.f * Ui, .y = Y - 2.f * Ui, .width = BarMaxW + 4.f * Ui, .height = BarH + 4.f * Ui}, 0.4f, 6, HudPanel);
                const float Frac = static_cast<float>(Total) / static_cast<float>(Worm::MaxHP * WormsPerTeam);
                DrawRectangleRounded({.x = X, .y = Y, .width = BarMaxW * Frac, .height = BarH}, 0.4f, 6, T.Tint);
            }
        }

        // Wind gauge
        {
            const float W = 240.f * Ui;
            const float H = 22.f * Ui;
            const Rectangle Box = {.x = ScreenW - W - 28.f * Ui, .y = ScreenH - 60.f * Ui, .width = W, .height = H};
            DrawRectangleRounded({.x = Box.x - 4.f * Ui, .y = Box.y - 4.f * Ui, .width = W + 8.f * Ui, .height = H + 8.f * Ui}, 0.4f, 6, HudPanel);
            const float Center = Box.x + W * 0.5f;
            const float Frac = Game.Wind / 5.f;
            const float FillW = std::abs(Frac) * W * 0.5f;
            const Color WindTint = Frac >= 0.f ? Color{.r = 90, .g = 200, .b = 255, .a = 255} : Color{.r = 255, .g = 150, .b = 90, .a = 255};
            DrawRectangleRec({.x = Frac >= 0.f ? Center : Center - FillW, .y = Box.y, .width = FillW, .height = H}, WindTint);

            // Chevrons scroll with the wind
            const float Scroll = std::fmod(Game.Time * 40.f * std::abs(Frac), 16.f * Ui);
            for (float X = 0.f; X < FillW - 6.f * Ui; X += 16.f * Ui)
            {
                const float Cx = Frac >= 0.f ? Center + std::fmod(X + Scroll, FillW) : Center - std::fmod(X + Scroll, FillW);
                const float Dir = Frac >= 0.f ? 1.f : -1.f;
                DrawTriangle({.x = Cx + 5.f * Ui * Dir, .y = Box.y + H * 0.5f}, Dir > 0.f ? Vector2{.x = Cx, .y = Box.y + 4.f * Ui} : Vector2{.x = Cx, .y = Box.y + H - 4.f * Ui}, Dir > 0.f ? Vector2{.x = Cx, .y = Box.y + H - 4.f * Ui} : Vector2{.x = Cx, .y = Box.y + 4.f * Ui}, ColorAlpha(WHITE, 0.6f));
            }
            DrawLineEx({.x = Center, .y = Box.y - 2.f * Ui}, {.x = Center, .y = Box.y + H + 2.f * Ui}, 2.f * Ui, RAYWHITE);
            DrawTextOutlined("WIND", static_cast<int>(Box.x), static_cast<int>(Box.y - 24.f * Ui), static_cast<int>(18.f * Ui), RAYWHITE);
        }

        // Weapon panel
        if (State.WeaponPanelOpen)
        {
            const Rectangle First = GetWeaponSlotRect(0);
            const Rectangle Last = GetWeaponSlotRect(WeaponCount - 1);
            const Rectangle Panel = {.x = First.x - 12.f * Ui, .y = First.y - 40.f * Ui, .width = GetWeaponSlotRect(3).x + First.width - First.x + 24.f * Ui, .height = Last.y + Last.height - First.y + 52.f * Ui};
            DrawRectangleRounded(Panel, 0.08f, 8, HudPanel);
            DrawTextOutlined("WEAPONS", static_cast<int>(Panel.x + 12.f * Ui), static_cast<int>(Panel.y + 10.f * Ui), static_cast<int>(20.f * Ui), RAYWHITE);

            for (int i = 0; i < WeaponCount; ++i)
            {
                const Rectangle Slot = GetWeaponSlotRect(i);
                const WeaponKind Kind = static_cast<WeaponKind>(i);
                const int Ammo = Current.Ammo[static_cast<size_t>(i)];
                const bool Hovered = CheckCollisionPointRec(GetMousePosition(), Slot);
                const bool Selected = Kind == Game.Weapon;

                DrawRectangleRounded(Slot, 0.15f, 6, Selected ? ColorAlpha(Current.Tint, 0.45f) : (Hovered ? Color{.r = 70, .g = 76, .b = 96, .a = 230} : Color{.r = 40, .g = 44, .b = 58, .a = 230}));
                DrawWeaponArt(Kind, {.x = Slot.x + Slot.width * 0.38f, .y = Slot.y + Slot.height * 0.55f}, -0.35f, 1.9f * Ui);
                if (Ammo >= 0)
                {
                    DrawTextOutlined(TextFormat("%d", Ammo), static_cast<int>(Slot.x + Slot.width - 18.f * Ui), static_cast<int>(Slot.y + 4.f * Ui), static_cast<int>(16.f * Ui), Ammo > 0 ? RAYWHITE : RED);
                }
                if (Ammo == 0)
                {
                    DrawRectangleRounded(Slot, 0.15f, 6, ColorAlpha(BLACK, 0.55f));
                }
                if (Hovered)
                {
                    const char* Name = GetWeaponInfo(Kind).Name;
                    DrawTextCentered(Name, Panel.x + Panel.width * 0.5f, Panel.y + Panel.height + 8.f * Ui, static_cast<int>(20.f * Ui), RAYWHITE);
                }
            }
        }

        // Controls hint
        {
            const char* Hint = "A/D walk   Space jump   Ctrl backflip   LMB fire   RMB weapons   Q/E cycle   1-5 fuse   W/S rope   Wheel zoom   MMB pan   Esc pause";
            const int Size = static_cast<int>(14.f * Ui);
            DrawText(Hint, static_cast<int>((ScreenW - static_cast<float>(MeasureText(Hint, Size))) * 0.5f), static_cast<int>(ScreenH - 18.f * Ui), Size, ColorAlpha(RAYWHITE, 0.55f));
        }
    }

    void DrawButtonColumn(const std::initializer_list<const char*> Labels)
    {
        const float Ui = GetUiScale();
        constexpr Color Fills[3] = {{.r = 236, .g = 72, .b = 72, .a = 255}, {.r = 72, .g = 150, .b = 246, .a = 255}, {.r = 90, .g = 96, .b = 112, .a = 255}};
        int Index = 0;
        for (const char* Label : Labels)
        {
            const Rectangle Button = GetMenuButtonRect(Index);
            const bool Hovered = CheckCollisionPointRec(GetMousePosition(), Button);
            const Color Fill = Fills[std::min(Index, 2)];
            DrawRectangleRounded(Button, 0.4f, 8, Hovered ? ColorLerp(Fill, WHITE, 0.25f) : Fill);
            DrawRectangleRoundedLinesEx(Button, 0.4f, 8, 3.f * Ui, ColorAlpha(BLACK, 0.4f));
            const int Size = static_cast<int>(32.f * Ui);
            DrawText(Label, static_cast<int>(Button.x + (Button.width - static_cast<float>(MeasureText(Label, Size))) * 0.5f), static_cast<int>(Button.y + (Button.height - static_cast<float>(Size)) * 0.5f), Size, WHITE);
            ++Index;
        }
    }

    void DrawPause()
    {
        const float Ui = GetUiScale();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.55f));
        DrawTextCentered("PAUSED", static_cast<float>(GetScreenWidth()) * 0.5f, GetMenuButtonRect(0).y - 140.f * Ui, static_cast<int>(96.f * Ui), RAYWHITE);
        DrawButtonColumn({"RESUME", "MAIN MENU", "QUIT"});
    }

    void DrawMenu(const GameState& State)
    {
        const float Ui = GetUiScale();
        const float ScreenW = static_cast<float>(GetScreenWidth());
        const float ScreenH = static_cast<float>(GetScreenHeight());
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.35f));

        const float Bob = std::sin(static_cast<float>(GetTime()) * 2.f) * 6.f * Ui;
        DrawTextCentered("WORMS", ScreenW * 0.5f, ScreenH * 0.18f + Bob, static_cast<int>(150.f * Ui), {.r = 246, .g = 166, .b = 172, .a = 255});
        DrawTextCentered("artillery with physics and a lot of dirt", ScreenW * 0.5f, ScreenH * 0.18f + 160.f * Ui, static_cast<int>(26.f * Ui), RAYWHITE);

        DrawButtonColumn({"NEW GAME", TextFormat("BIOME: %s", GetMenuBiomeName(State.MenuBiome)), "QUIT"});
        DrawTextCentered("2 teams, 4 worms each, hot-seat", ScreenW * 0.5f, GetMenuButtonRect(2).y + 100.f * Ui, static_cast<int>(20.f * Ui), ColorAlpha(RAYWHITE, 0.8f));
    }

    void DrawVictory(const GameState& State)
    {
        const float Ui = GetUiScale();
        const float ScreenW = static_cast<float>(GetScreenWidth());
        const float ScreenH = static_cast<float>(GetScreenHeight());
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.5f));

        if (State.Winner >= 0)
        {
            const Team& T = State.Game.Teams[static_cast<size_t>(State.Winner)];
            DrawTextCentered(TextFormat("%s WIN!", T.Name), ScreenW * 0.5f, ScreenH * 0.4f, static_cast<int>(96.f * Ui), T.Tint);
        }
        else
        {
            DrawTextCentered("DRAW!", ScreenW * 0.5f, ScreenH * 0.4f, static_cast<int>(96.f * Ui), GOLD);
        }
        DrawTextCentered("Click to return to the menu", ScreenW * 0.5f, ScreenH * 0.4f + 120.f * Ui, static_cast<int>(30.f * Ui), RAYWHITE);
    }

    void DrawCursor(const GameState& State)
    {
        const Vector2 Mouse = GetMousePosition();
        const float Ui = GetUiScale();
        const bool InGame = State.Screen == GameScreen::Game && !State.WeaponPanelOpen && !State.Paused;
        const Color Tint = InGame ? State.Game.Teams[static_cast<size_t>(State.Game.CurrentTeam)].Tint : WHITE;
        if (InGame)
        {
            DrawRing(Mouse, 7.f * Ui, 9.f * Ui, 0.f, 360.f, 20, ColorAlpha(BLACK, 0.5f));
            DrawRing(Mouse, 7.5f * Ui, 8.5f * Ui, 0.f, 360.f, 20, Tint);
            DrawCircleV(Mouse, 1.5f * Ui, Tint);
        }
        else
        {
            DrawTriangle(Mouse, {.x = Mouse.x, .y = Mouse.y + 20.f * Ui}, {.x = Mouse.x + 13.f * Ui, .y = Mouse.y + 14.f * Ui}, BLACK);
            DrawTriangle({.x = Mouse.x + 1.5f * Ui, .y = Mouse.y + 3.f * Ui}, {.x = Mouse.x + 1.5f * Ui, .y = Mouse.y + 16.f * Ui}, {.x = Mouse.x + 10.f * Ui, .y = Mouse.y + 12.5f * Ui}, Tint);
        }
    }
}

void RenderFrame(const GameState& State)
{
    const Match& Game = State.Game;
    const BiomeTheme& Theme = GetBiomeTheme(Game.Biome);

    // Screen shake: Offset = MaxOffset * Shake ^ 2 * RandomOffset
    // Ref: Math for Game Programmers: Juicing Your Cameras With Math https://www.youtube.com/watch?v=tu-Qe66AvtY
    constexpr float ShakeMaxOffset = 18.f;
    const float ShakeSquared = State.ShakeScale * State.ShakeScale;
    Camera2D View = GetWorldCamera(State);
    View.offset.x += ShakeMaxOffset * ShakeSquared * GetRandomFloatValue(-1.f, 1.f) * GetUiScale();
    View.offset.y += ShakeMaxOffset * ShakeSquared * GetRandomFloatValue(-1.f, 1.f) * GetUiScale();

    BeginDrawing();
    {
        ClearBackground(Theme.SkyBottom);
        DrawSkyAndRidges(Game.Background, Theme, View, static_cast<float>(MapHeight));

        BeginMode2D(View);
        DrawWorld(State, View);
        EndMode2D();

        DrawAmbient(Game.Background, Theme);
        DrawWormLabels(State, View);
        DrawFloatingTexts(State, View);

        if (State.Screen == GameScreen::Game)
        {
            DrawHud(State);
            if (State.Paused)
            {
                DrawPause();
            }
        }
        else if (State.Screen == GameScreen::MainMenu)
        {
            DrawMenu(State);
        }
        else if (State.Screen == GameScreen::Victory)
        {
            DrawVictory(State);
        }

        DrawText(TextFormat("%d FPS", GetFPS()), 10, 8, static_cast<int>(16.f * GetUiScale()), ColorAlpha(RAYWHITE, 0.5f));
        DrawCursor(State);
    }
    EndDrawing();
}
