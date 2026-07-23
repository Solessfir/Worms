# Worms: Multiplayer & Multithreading

Answers two interview-style questions against the actual codebase in this repo (`src/Game.h`, `src/Game.cpp`, `src/Main.cpp`).

---

## 1. Converting to multiplayer

### Where the current architecture stands

The split already helps: `Game.cpp` is a pure simulation layer (no raylib input calls except `IsKeyDown`/`IsMouseButtonDown` inside `UpdateWorm`/`UpdateWormAim`), and `Main.cpp` owns input, orchestration, and rendering. That's the fault line to cut along.

The blocker: simulation functions read input directly instead of taking it as data.

```cpp
// src/Game.cpp - UpdateWorm reads raw input mid-simulation
if (AcceptInput)
{
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    {
        W.Position.x -= WormWalkSpeed * DeltaTime;
        W.FacingDir = -1.f;
    }
    ...
    if (IsKeyPressed(KEY_SPACE) && W.OnGround)
    {
        W.Velocity.y = WormJumpVelocity;
    }
}
```

A remote client has no `IsKeyDown` for the other player's keyboard. Step one is always: **decouple "what does the player want to do" from "what does the simulation do with it."**

### Step 1 - Introduce an input struct

```cpp
// New: PlayerInput, built once per frame from local device state
struct PlayerInput
{
    bool MoveLeft;
    bool MoveRight;
    bool JumpPressed;
    Vector2 AimTarget;   // world-space mouse position
    bool FireHeld;
    bool FireReleased;
};

// Main.cpp: gather local input (unchanged devices, new shape)
PlayerInput GatherLocalInput()
{
    return {
        .MoveLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT),
        .MoveRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
        .JumpPressed = IsKeyPressed(KEY_SPACE),
        .AimTarget = GetMousePosition(),
        .FireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .FireReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
    };
}
```

```cpp
// Game.cpp: UpdateWorm takes the struct instead of polling raylib
void UpdateWorm(Worm& W, const std::vector<TerrainBlock>& Terrain,
                 const float DeltaTime, const float PlayfieldWidth,
                 const PlayerInput& Input, const bool AcceptInput)
{
    if (AcceptInput)
    {
        if (Input.MoveLeft)  { W.Position.x -= WormWalkSpeed * DeltaTime; W.FacingDir = -1.f; }
        if (Input.MoveRight) { W.Position.x += WormWalkSpeed * DeltaTime; W.FacingDir = 1.f; }
        if (Input.JumpPressed && W.OnGround) { W.Velocity.y = WormJumpVelocity; W.OnGround = false; }
    }
    ...
}
```

Now `UpdateWorm`/`UpdateWormAim` are pure functions of `(state, input, dt) -> state`. That's the prerequisite for literally every networking model, not just one.

### Step 2 - Pick a model: authoritative server, not lockstep

This game is a good fit for **server-authoritative, low-frequency state sync**, not deterministic lockstep:

- Turns are slow (aim → charge → fire → settle) so bandwidth is trivial - a few packets per turn, not per-frame.
- Lockstep requires bit-identical float math on both peers (`std::cosf`, `Vector2Distance`, gravity integration) across potentially different compilers/CPUs - a notorious source of desync bugs. Not worth the risk for a turn-based game with no real-time reflex component.
- Server-authoritative also closes the obvious cheat vector: a modified client claiming `ChargePower = 1.0` on every shot, or moving during the opponent's turn.

Concretely:

```cpp
// Server owns the only "real" GameState. It runs UpdateGameplay exactly as
// today, but AcceptInput now comes from whichever connection matches
// Sim.CurrentPlayer, not local keyboard/mouse.
void ServerTick(GameState& State, const PlayerInput& InputFromCurrentPlayerConn, float DeltaTime)
{
    UpdateGameplay(State, InputFromCurrentPlayerConn, DeltaTime); // same function, new input source
    Broadcast(SerializeState(State)); // to both clients
}

// Client: no local simulation at all during the opponent's turn - just
// interpolate/render the last state snapshot. During your own turn, you can
// simulate the aim/charge locally for responsiveness, but firing is a
// request, not a fact, until the server echoes back the resulting SimState.
void ClientSendInput(const PlayerInput& Input)
{
    if (IsLocalPlayersTurn) SendToServer(Input);
}
```

### What changes, concretely, file by file

- `Game.h` / `Game.cpp`: add `PlayerInput`, thread it through `UpdateWorm`, `UpdateWormAim`, `UpdateGameplay`. No physics/collision logic changes - it's already deterministic given the same inputs.
- `Main.cpp`: splits into two binaries or a `--server`/`--client` mode. Client keeps `RenderFrame` untouched; it just stops calling `UpdateGameplay` directly and instead applies `SimState` snapshots received over the wire.
- New: a thin `NetworkManager` (e.g. wrapping ENet or plain UDP) for connect/send/receive, and a `Serialize(SimState)`/`Deserialize` pair. `SimState` is already a flat, small POD-ish struct (2 worms, a `vector<TerrainBlock>`, one projectile) - cheap to serialize wholesale rather than diffing, at this scale.
- Level loading (`LoadLevelFromPath`) needs a **level hash/id exchanged at match start** so both sides provably loaded the same terrain - can't trust the client picked the same random/sequential level index independently.

### Challenges to call out explicitly

1. **Determinism vs. sync strategy.** Solved above by going authoritative-server instead of lockstep - worth stating *why* that tradeoff was made, since it's the first thing a reviewer will probe.
2. **Latency hiding.** Turn-based mostly hides latency, but the *charge-and-release* mechanic is the one real-time-sensitive part (`W.ChargePower` accrues while `FireHeld`). A naive "send input, wait for server echo" round-trips every frame during charging. Fix: let the client simulate charge locally (cosmetic only), and only trust the server's recorded release time/power for the actual shot resolution.
3. **Turn ownership / cheat validation.** Server must reject any `PlayerInput` that isn't from `Sim.CurrentPlayer`'s connection, and clamp `ChargePower`/`AimAngle` server-side rather than trusting client-reported values.
4. **Disconnect/reconnect.** `SimState` is small enough to snapshot and resend wholesale on reconnect - no need for a replay/event log, just "here's the current world, continue."
5. **The existing `WaitForMouseRelease` menu-click-bleed fix** (`State.WaitForMouseRelease`) is local-input-only state; it has to live on the client's input-gathering side, not leak into the networked `PlayerInput`.

---

## 2. What can be multithreaded, and why

Honest starting point: **at this game's current scale (2 worms, ~dozens of terrain blocks, a few dozen particles), almost nothing here is worth multithreading.** The right answer to "what can be multithreaded" is also "what's actually slow" - and right now nothing here would show up on a profiler next to `BeginDrawing`/`EndDrawing`. That said, here's what's structurally parallelizable and when it'd start to matter:

### a) Particle updates - parallelizable, not yet worth it

```cpp
// src/Game.cpp - today: sequential, each Particle is fully independent
void UpdateParticles(std::vector<Particle>& Particles, const float DeltaTime)
{
    for (Particle& Particle : Particles)
    {
        Particle.Position.x += Particle.Velocity.x * DeltaTime;
        Particle.Position.y += Particle.Velocity.y * DeltaTime;
        Particle.Lifetime -= DeltaTime;
    }
    std::erase_if(Particles, [](const Particle& P) { return P.Lifetime <= 0.f; });
}
```

```cpp
// Parallelized: no shared mutable state between iterations, so this is a
// textbook std::execution::par_unseq candidate once Particles.size() is
// large enough to amortize thread dispatch overhead (hundreds+, not the
// dozens spawned per explosion today)
#include <execution>

void UpdateParticles(std::vector<Particle>& Particles, const float DeltaTime)
{
    std::for_each(std::execution::par_unseq, Particles.begin(), Particles.end(),
        [DeltaTime](Particle& P)
        {
            P.Position.x += P.Velocity.x * DeltaTime;
            P.Position.y += P.Velocity.y * DeltaTime;
            P.Lifetime -= DeltaTime;
        });
    std::erase_if(Particles, [](const Particle& P) { return P.Lifetime <= 0.f; }); // still sequential - compaction has ordering dependencies
}
```

Why: each particle only touches its own memory - zero synchronization needed for the update pass itself. Same argument applies to `UpdateBackgroundParticles`. The catch is that `SpawnParticles(..., 40)` on an explosion is a tiny workload; thread dispatch overhead would dominate. This becomes worth it only if the particle budget grows an order of magnitude (e.g. per-pixel terrain debris instead of a handful of blast particles).

### b) Terrain/collision queries - parallelizable, not yet worth it either

```cpp
// src/Game.cpp - ApplyExplosionDamage loops all terrain blocks checking
// circle-vs-rect overlap against the blast center. Independent per block.
for (int i = 0; i < static_cast<int>(Sim.Terrain.size()); ++i)
{
    TerrainBlock& Block = Sim.Terrain[i];
    if (!Block.Alive) continue;
    const Rectangle BlockRect = {.x = Block.Position.x, .y = Block.Position.y, .width = Block.Width, .height = Block.Height};
    if (!CheckCollisionCircleRec(Center, Radius, BlockRect)) continue;
    ...
}
```

Why not yet: current levels have on the order of 100-300 blocks (40-col grids, ~10-15 rows). A sequential scan over a few hundred `Rectangle` checks is sub-microsecond work; spinning up threads costs more than it saves. This becomes relevant only if terrain resolution jumps significantly (e.g. true per-pixel deformable terrain, thousands of cells) - at that point a spatial grid/quadtree to *avoid* checking every block would help more than threading the linear scan would.

### c) Asset loading - the one that's actually worth doing

```cpp
// src/Main.cpp today - LoadMusicStream/LoadSound/LoadLevelFromPath all run
// synchronously on the main thread during startup, causing a load hitch
// before the window is interactive.
MenuMusic = LoadMusicStream("assets/audio/menu.mp3");
GameplayMusic = LoadMusicStream("assets/audio/soundtrack.mp3");
ExplosionSound = LoadSound("assets/audio/explosion.mp3");
```

The improvement here isn't "parallelize the loop" (there's no loop) - it's **moving I/O off the main thread** so the window paints immediately instead of blocking on disk:

```cpp
// File reads (the actual I/O-bound part) can happen on a worker thread;
// GPU/audio-device resource creation cannot - raylib's LoadSound/LoadMusicStream
// touch the OpenAL/GL context, which raylib does not guarantee is thread-safe
// off the thread that owns it. So: read bytes off-thread, create the raylib
// resource on the main thread once the bytes are ready.
std::future<std::vector<uint8_t>> ExplosionBytes = std::async(std::launch::async,
    [] { return ReadFileBytes("assets/audio/explosion.mp3"); });

// ...later, on the main thread, once needed:
ExplosionSound = LoadSoundFromMemory(".mp3", ExplosionBytes.get().data(), ...);
```

Why this one's real: it's genuinely I/O-bound (disk read latency, not CPU), it's on the critical path to "window is responsive," and it doesn't touch any raylib GPU/audio call off the owning thread - unlike (a) and (b), which are CPU-bound and small enough that dispatch overhead eats the win.

### d) If multiplayer lands: networking gets its own thread, unconditionally

Unlike (a)-(c), this one isn't a "maybe" - socket recv/send should never block the render/sim loop. Pattern: a network thread owns the socket, pushes received packets into a thread-safe queue (`std::mutex` + `std::queue`, or a lock-free SPSC ring buffer given the low message rate here), and `UpdateFrame` drains that queue at the top of each tick before running `UpdateGameplay`. This is a hard requirement, not a scale-dependent optimization - `Main.cpp`'s `while (!WindowShouldClose())` loop must never wait on the network.

### Summary judgment

The honest engineering answer: **profile before threading.** This codebase's actual bottleneck, if any, is GPU-bound draw calls (raylib `DrawRectangleRounded`/particles), not CPU-bound simulation - and raylib's rendering calls are not thread-safe to parallelize anyway. The two moves that would matter in practice are (c) getting asset I/O off the main thread, and (d) giving networking its own thread the moment multiplayer is added. (a) and (b) are correct in theory and worth knowing how to do, but premature at this scale - the tell is that neither would show up in a profile today.
