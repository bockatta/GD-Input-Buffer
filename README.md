# Input Buffer

An **input buffer for Geometry Dash** (Geode mod).

Every time you click, the mod simulates your trajectory on hidden copies of the player. If the
click is safe it goes through untouched. If it would kill you, the click is held and re-fired
after the smallest number of physics ticks that survives, as long as that fits inside your window.
Clicks only ever move later, never earlier, and nothing is pressed for you.

**Windows are in 60fps frames** (1.0 = 1 frame @ 60fps), set per mode, and converted to physics
ticks at runtime, so they are identical at 60 / 240 / 480+ TPS. Works with or without Click Between Frames.

- Gamemodes: cube, ball, spider, ship, UFO, wave, robot, swing, mini versions of all of them, dual.
  Platformer is left untouched.
- Exact smallest-delay search at any TPS, plus an optional safety margin.
- Predicts through yellow / pink / red / black / blue / green orbs (when your click reaches one
  within 125 ms), pads (strength learned from your first real hit on each type), speed portals and
  gravity portals. Mode, size, mirror, dual and teleport portals and dash / teleport / spider orbs
  still stop the lookahead instead of guessing past them.
- In ship, wave, robot, UFO and swing, a click is only treated as deadly if the death comes within
  100 ms or no follow-up input could avoid it, so clicks you'd have steered out of aren't delayed.
- Spam guard: wave / ship / UFO / swing bypass the buffer once you chain rapid inputs.
- Nothing is dropped: every input made while one is held back is replayed in order with the same
  shift, so double taps survive and hold length is kept.
- CPU budget: slow PCs get a shorter lookahead automatically instead of a hitch.

## Window

Press **Left Shift** to show / hide the window. Drag it by
its title bar; it remembers where you left it. Scroll with the mouse wheel, by dragging the list,
or with the arrows.

- **Log**: everything the buffer did this level, newest first (last 300 lines).
- **Stats**: decisions, deaths by reason, prediction accuracy overall and per mode, your rhythm
  (hold and gap length), CPU, windows, learned pads.
- **Stage**: pick Off, 1, 2, 3, 4 or Extreme, whether the Log lists safe clicks, and **Show paths**:
  a live line showing where your next input (press, or release while holding) would take you right
  now, with your hitbox at both ends. Green = safe, red = deadly, yellow = cut short by an orb /
  portal. While a click is held back it shows the path being tested for it.

While a level is running the window is display-only, so it never eats a click. Interact with it
while paused.

## Stages

One setting, **Stage**, picks everything:

| Stage | Saves clicks up to |
|-------|--------------------|
| Off | nothing (raw input) |
| Stage 1 | 1 frame early |
| Stage 2 | 2 frames early |
| Stage 3 | 4 frames early (3 for ship / wave) |
| Stage 4 | 8 frames early (5 for ship / wave) |
| Extreme | 20 frames early (12 for ship / wave) |

**F6** cycles Stage 1 > 2 > 3 > 4 > Extreme > Off. **Left Shift** shows / hides the window. The window's Stage tab sets it directly.
Works with mouse, keyboard and any key mapped to jump.

## Settings

Only four: **Stage**, **Stage Cycle Key** (F6), **Window Key** (Left Shift), **Window Size**.

## Extreme mode

Saves clicks up to 20 frames (80 ticks @240) early, 12 frames for ship and wave.

- **Live re-planning**: while a click is held back, the mod re-tests "fire now?" every tick from the
  real game state and fires at the first safe tick (plus one tick of margin, none in wave). No
  long-range guess that drifts.
- **Releases too**: a release (or any input) made while a click is held moves with it, then gets
  the same live check before it fires, so your hold lengths stay the same unless that would kill you.
- **Cube / ball / spider / robot**: a click is judged on getting back to the ground; whatever comes
  after the landing is your next jump's job.
- The spam guard is off. It still only touches inputs that would kill you.

## Reading the Log

- Window title: ON / EXTREME / OFF. Footer: live gamemode (e.g. `mini cube`), TPS, fps, window,
  prediction accuracy.
- Lines: `PRESS buffered +3t (12.5ms)`, `PRESS raw: yellow orb 20px ahead`,
  `PRESS raw: no fix in 2t (dies in 9t)`, `DEATH: click was 6t (25ms) too early, window 4t`, ...
  Repeats are merged (`x3`).

Tick **Log: show safe clicks** in the Stage tab to also see clicks that were already fine.

Death reasons in the Stats tab: sim said safe (mispredict), window too small (no delay in the
stage's window fixed it), orb/pad/portal cut the prediction, spam guard, CPU budget, buffered
click still died, needed a follow-up input, no recent click.

## High FPS + Click Between Frames (e.g. 170fps + CBF)

Built to work with CBF out of the box. What the mod does for these setups:

- **Game-time scheduling.** CBF adds short extra physics steps when a click lands mid-frame.
  Held clicks, the spam guard and death timings run on game time, so those extra steps can't
  make a buffered click fire early.
- **CBF timing guard**. A held click is re-fired on a physics step, not at
  CBF's exact sub-frame moment, so it can land up to one tick later than planned. The mod only
  picks a delay if the tick after it is also safe.
- **Auto CPU budget**. The lookahead may use about 30% of your measured frame
  time (50% in Extreme). At 170fps that is about 1.8 ms, so clicking never costs a frame.
- The window footer shows your fps, `CBF` when it is loaded, and the current CPU budget.

Nothing to set up: stages are in frames and converted to ticks, so fps doesn't change them.
If the Log often shows `raw: CPU budget`, your PC is hitting the limit: use a lower stage
(Extreme searches the most).

## Testing

`tests/core_tests.cpp` unit-tests the scheduler and the delay search (no game needed):

```
g++ -std=c++20 -O2 -I src tests/core_tests.cpp -o core_tests && ./core_tests
```

It checks for lost or duplicated inputs, ordering, hold length, firing never early and at most one
step late at 60/144/170/240/360 fps with and without CBF sub-steps, the search result, the guard,
margin and aim, and that no delay is simulated twice.

## Cube, ship and wave

- **Cube / ball / spider** (and mini): holding re-jumps on landing in 2.2, so the prediction lets
  go after your usual tap length (learned from your clicks) instead of holding forever.
  **Realistic Cube Taps** switches this off.
- **Ship / wave** (and mini): before treating a click as deadly, the mod checks whether one
  follow-up input, or a full release + re-press cycle at your own rhythm, would still save it.
  Deaths within 100 ms of the click always count.
- Stats shows accuracy per mode (cube, mini cube, ship, wave, mini wave...) so you can see where
  predictions are off.

## Changes in 1.10.0

- Wider windows for every stage (Stage 1 now 1 frame ... Extreme 20 frames).
- Show paths is live, with hitboxes at both ends.

## Changes in 1.9.1

- Buffered inputs are checked to reach the player on the chosen tick; if another input mod (e.g. Click
  Between Frames) holds them back, they're applied directly. Stats shows how often.
- **Show paths** in the Stage tab: red = your click as made, green = the buffered click.

## Changes in 1.9.0

- Extreme: live re-planning for clicks and releases, cube landing rule, ship / wave 10 frames.
- Deaths counted once (practice / noclip no longer flood the log and break the buffer).
- Left Shift opens the window; pause-screen button removed; window half the size.

## Changes in 1.8.0

- One Stage setting (Off, 1-4, Extreme) instead of many toggles; F6 can no longer lose track.
- Search checks each tick right after a click (narrow safe gaps were skipped, worst in Extreme).
- Every jump input works: mouse, keyboard, remapped keys. Key repeat isn't a new click.
- Cheaper checks so dense levels don't run out of CPU time.
- Quick taps into orbs are predicted.

## Changes in 1.7.0

- Draggable window replaces the F7 panel, the F9 coach page and the popup menu (F10).
- Timing coach removed.
- Cube tap model, ship / wave rhythm cycle, per-mode accuracy and rhythm in Stats.

## Changes in 1.6.0

- Menu with Log, Coach, Stats and Settings tabs (F10 or the pause screen's Buffer button).

## Changes in 1.5.0

- Live accuracy tracking, death replay, timing coach (F9), auto windows.
- Prediction through pads, speed portals, gravity portals, blue and green orbs.
- Trajectory overlay, practice-only mode, badge.

## Changes in 1.4.0

- No dropped taps: inputs made while one is held back are all replayed, in order, with spacing kept.
- A release made after a delayed press already fired moves with it (hold length kept).
- Each test starts from the real button state with a clean press (was inheriting the last test's).
- Fewer false alarms in ship / wave / robot / UFO / swing (follow-up input check).
- Only delays to paths that are actually safe.
- Orb prediction for yellow / pink / red / black orbs.
- Each delay simulated at most once per click.
- Extreme: earliest safe + margin by default, no spam guard, windows 60 / 24.

## Changes in 1.3.1

- Game-time scheduling for buffered clicks, spam guard and death timing (fixes early fires with CBF).
- CBF Timing Guard.
- Auto CPU Budget based on measured frame time.
- Sim steps use the level's time warp.
- Panel shows fps, CBF and CPU budget.

## Changes in 1.3

- New top-right log panel (F7) replacing the old top-left readout.
- New Extreme mode (F8, also in the F6 cycle).
- Fixed false `ORB/PORTAL - raw input`: the portal you just went through (e.g. a mini portal)
  no longer stops the lookahead. Objects behind you or already touched are ignored, orbs only count
  while you can actually click them, and real hitboxes are used instead of a fixed box.
- Mode names now include mini, ball and spider.
- Smallest delay is found exactly (bisection) instead of on a coarse grid at high TPS.
- A delay that survives until the next orb/pad/portal now counts as a fix in hold modes too.
- Hold length preserved for ship / wave / robot when a press is buffered.
- Tick length taken from the recent maximum so CBF sub-steps don't skew delays.
- Settings are cached instead of re-read on every click.
