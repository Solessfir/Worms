#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "Game.h"

TEST_CASE("Ball moves by velocity * delta")
{
    constexpr float PosX = 100.f;
    constexpr float PosY = 50.f;
    constexpr float OneFrame = 1.f / 60.f;

    // Spawn Ball and move somewhere (right+down in this case)
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 0.f, .y = 0.f}, {.x = PosX, .y = PosY}, 10.f));

    // Move 1 frame at 60 FPS
    UpdateBall(Sim, OneFrame);

    CHECK(Sim.Balls[0].Position.x == doctest::Approx(PosX * OneFrame));
    CHECK(Sim.Balls[0].Position.y == doctest::Approx(PosY * OneFrame));
}

TEST_CASE("Ball bounces off left wall")
{
    constexpr float Radius = 10.f;
    constexpr float Width = 800.f;
    constexpr float Height = 600.f;

    // Ball overlapping the left wall (x = 5 < Radius), moving left
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 5.f, .y = 100.f}, {.x = -100.f, .y = 0.f}, Radius));

    std::vector<Vector2> DeadBalls;
    UpdateBallWallCollision(Sim, Width, Height, DeadBalls);

    // We expect that Ball will not penetrate the wall (snap to the left edge)
    CHECK(Sim.Balls[0].Position.x == doctest::Approx(Radius));

    // And velocity will be flipped
    CHECK(Sim.Balls[0].Velocity.x > 0.f);
}

TEST_CASE("Ball bounces off right wall")
{
    constexpr float Radius = 10.f;
    constexpr float Width = 800.f;
    constexpr float Height = 600.f;

    // Ball overlapping the right wall (x = 795, edge reaches 805 > Width), moving right
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 795.f, .y = 100.f}, {.x = 100.f, .y = 0.f}, Radius));

    std::vector<Vector2> DeadBalls;
    UpdateBallWallCollision(Sim, Width, Height, DeadBalls);

    // We expect that Ball will not penetrate the wall (snap to the right edge)
    CHECK(Sim.Balls[0].Position.x == doctest::Approx(Width - Radius));

    // And velocity will be flipped
    CHECK(Sim.Balls[0].Velocity.x < 0.f);
}

TEST_CASE("Ball falls off bottom - erased and reported dead")
{
    constexpr float Radius = 10.f;
    constexpr float Width = 800.f;
    constexpr float Height = 600.f;

    // Launch the Ball already below the floor, moving down
    SimState Sim{};
    Sim.BallLaunched = true;
    Sim.Lives = 1;
    Sim.Balls.push_back(MakeBall({.x = 400.f, .y = Height + 5.f}, {.x = 0.f, .y = 100.f}, Radius));

    std::vector<Vector2> DeadBalls;
    UpdateBallWallCollision(Sim, Width, Height, DeadBalls);

    // We expect that UpdateBallWallCollision will kill the Ball
    CHECK(Sim.Balls.empty());

    // Add this dead Ball to the DeadBalls
    CHECK(DeadBalls.size() == 1);

    // Substruct 1 Life
    CHECK(Sim.Lives == 0);

    // And Ball no more flying
    CHECK(Sim.BallLaunched == false);
}

TEST_CASE("Ball bounces off paddle")
{
    constexpr float Radius = 10.f;
    constexpr float PadX = 350.f;
    constexpr float PadTop = 550.f;
    constexpr float PadWidth = 100.f;

    // Ball above paddle center, moving down
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = PadX + PadWidth * 0.5f, .y = 545.f}, {.x = 50.f, .y = 100.f}, Radius));

    Sim.Pad.Position = {.x = PadX, .y = PadTop};
    Sim.Pad.Width = PadWidth;
    Sim.Pad.Height = 15.f;

    UpdateBallPadCollision(Sim);

    // We expect that Ball position will be snapped to the Pad top
    CHECK(Sim.Balls[0].Position.y == doctest::Approx(PadTop - Radius));

    // And Ball velocity flipped
    CHECK(Sim.Balls[0].Velocity.y < 0.f);
}

TEST_CASE("Ball misses paddle - no bounce")
{
    constexpr float Radius = 10.f;
    constexpr float VelY = 100.f;

    // Ball far from the paddle (top-left of the field)
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 100.f, .y = 100.f}, {.x = 50.f, .y = VelY}, Radius));

    Sim.Pad.Position = {.x = 350.f, .y = 550.f};
    Sim.Pad.Width = 100.f;
    Sim.Pad.Height = 15.f;

    UpdateBallPadCollision(Sim);

    // Ball fall through, velocity unchanged
    CHECK(Sim.Balls[0].Velocity.y == doctest::Approx(VelY));
}

TEST_CASE("Ball destroys single-hit brick")
{
    constexpr float Radius = 10.f;
    constexpr int Points = 5;

    // Ball moving down into a 1-hit brick it overlaps
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 100.f, .y = 100.f}, {.x = 0.f, .y = 50.f}, Radius));

    Brick B{};
    B.Position = {.x = 90.f, .y = 95.f};
    B.Width = 40.f;
    B.Height = 15.f;
    B.PointValue = Points;
    B.HitsLeft = 1;
    B.Alive = true;
    Sim.Bricks.push_back(B);

    std::vector<int> HitBricks;
    UpdateBallBrickCollision(Sim, HitBricks);

    // Hit reported
    CHECK(HitBricks.size() == 1);

    // Brick dies
    CHECK(Sim.Bricks[0].Alive == false);

    // Ball combo incremented
    CHECK(Sim.Combo == 1);

    // Score incremented
    CHECK(Sim.Score == Points * Sim.Combo);

    // Ball velocity flipped
    CHECK(Sim.Balls[0].Velocity.y < 0.f);
}

TEST_CASE("Ball does not affect dead brick")
{
    constexpr float Radius = 10.f;
    constexpr float VelY = 50.f;

    // Same geometry as above, but the brick is already dead
    SimState Sim{};
    Sim.Balls.push_back(MakeBall({.x = 100.f, .y = 100.f}, {.x = 0.f, .y = VelY}, Radius));

    Brick B{};
    B.Position = {.x = 90.f, .y = 95.f};
    B.Width = 40.f;
    B.Height = 15.f;
    B.PointValue = 5;
    B.HitsLeft = 0;
    B.Alive = false; // dead bricks are skipped
    Sim.Bricks.push_back(B);

    std::vector<int> HitBricks;
    UpdateBallBrickCollision(Sim, HitBricks);

    // Dead brick ignored
    CHECK(HitBricks.empty());

    // Score not incremented
    CHECK(Sim.Score == 0);

    // Ball velocity unchanged
    CHECK(Sim.Balls[0].Velocity.y == doctest::Approx(VelY));
}
