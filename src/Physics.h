#pragma once

#include <cstdint>
#include "box2d/box2d.h"
#include "raylib/raylib.h"

// World is authored in pixels (terrain mask resolution). Box2D is tuned for objects 0.1-10 m, so a worm (18 px) ends up ~0.75 m
constexpr float PixelsPerMeter = 24.f;

inline b2Vec2 ToMeters(const Vector2 Pixels)
{
    return {Pixels.x / PixelsPerMeter, Pixels.y / PixelsPerMeter};
}

inline Vector2 ToPixels(const b2Vec2 Meters)
{
    return {.x = Meters.x * PixelsPerMeter, .y = Meters.y * PixelsPerMeter};
}

namespace Category
{
    constexpr std::uint64_t Terrain = 1u << 0;
    constexpr std::uint64_t Debris = 1u << 1;
    constexpr std::uint64_t Worm = 1u << 2;
    constexpr std::uint64_t Projectile = 1u << 3;
    constexpr std::uint64_t Grave = 1u << 4;
}

// Body user data packs the entity type and a stable id, since entity vectors reallocate and raw pointers would dangle
enum class EntityType : std::uint8_t
{
    None,
    Worm,
    Projectile,
    Debris,
    Grave
};

inline void* MakeBodyTag(const EntityType Type, const std::uint32_t Id)
{
    return reinterpret_cast<void*>((static_cast<std::uintptr_t>(Type) << 32) | Id);
}

inline EntityType GetTagType(const void* Tag)
{
    return static_cast<EntityType>(reinterpret_cast<std::uintptr_t>(Tag) >> 32);
}

inline std::uint32_t GetTagId(const void* Tag)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Tag) & 0xFFFFFFFFu);
}
