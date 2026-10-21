// ============================================================================
//  Input Buffer  v1.24.1                    (Geode mod for Geometry Dash)
//
//  WHAT THIS MOD IS
//  ----------------
//  An INPUT BUFFER for Geometry Dash.
//
//  In vanilla GD a click that lands a hair too early or too late is simply a
//  death. This mod sits between your input and the game. For every click it
//  asks one question:
//
//      "If I let this click through right now, does the player die?"
//
//  It answers by simulating the next few hundred physics ticks on hidden copies
//  of the player (the "sim players"). If the click is safe it passes through
//  untouched. If it would kill you, the click is HELD and re-fired after the
//  smallest number of physics ticks that survives, as long as that delay fits
//  inside the window you configured. Nothing is ever "pressed for you": a
//  click only ever moves later, never earlier, and never out of thin air.
//
//  HOW THE FILE IS LAID OUT
//  ------------------------
//    1. Settings, presets and the F6 cycle stages
//    2. Gamemode profile (cube / ship / wave / ... incl. mini)
//    3. Object classification + the "prediction cut-off" geometry
//         (what makes the lookahead stop at an orb / pad / portal)
//    4. Decisions, death reasons, statistics
//    5. CSV log
//    6. Shared colors
//    7. TrajectoryBufferSim  - the engine (simulate / search / buffer queue)
//    8. Geode hooks
//    9. The window (BufferWindow)
//
//  NEW IN 1.3
//  ----------
//    * Top-right live log panel (F7): mode, TPS, window, every decision,
//      every death with its reason, and exactly WHICH object cut a prediction.
//    * Fixed false "orb/portal - raw input": objects BEHIND the player (the
//      portal you just flew through) no longer stop the lookahead. Pads and
//      portals only count if they have not been touched yet; orbs only count
//      while they can actually be clicked. Hitboxes come from the objects.
//    * Smallest-delay search is now exact at any TPS (stride + bisection).
//    * A candidate that survives until an orb/pad/portal now counts as a fix
//      instead of being thrown away.
//    * Hold length is preserved: a release queued behind a buffered press is
//      delayed by the same amount (matters for ship / wave / robot).
//    * Tick length is taken from the recent maximum, so Click-Between-Frames
//      sub-steps no longer skew the delay.
//    * EXTREME mode (F8): wide window (default 50 ticks @240) for every mode,
//      aims for the middle of the safe window instead of its edge.
//    * Settings are cached instead of re-read on every click.
//
//  NEW IN 1.3.1 (high-FPS / Click Between Frames setups, e.g. 170fps + CBF)
//  ------------------------------------------------------------------------
//    * Held clicks are scheduled in GAME TIME, not in update calls. CBF adds
//      short extra physics steps when a click lands mid-frame; counting calls
//      made buffered clicks, the spam guard and death timings run fast.
//    * CBF timing guard: a held click can only be re-fired on a step boundary,
//      so the real fire lands 0-1 tick after the chosen delay. A delay is only
//      used if the tick after it is safe too.
//    * Auto CPU budget: the lookahead may use at most ~30% of your measured
//      frame time (50% in extreme), so a click never costs a visible frame.
//      170fps -> ~1.8 ms. The old fixed 3 ms (6 ms extreme) was half to a full
//      170fps frame.
//    * Sim steps now use the level's time warp, matching the real physics.
//    * Panel shows fps, whether CBF is loaded and the current CPU budget.
//
//  NEW IN 1.4.0 (accuracy / latency pass; scheduler + search in BufferCore.hpp)
//  ------------------------------------------------------------------------
//    * No dropped taps: every input that arrives while one is held is replayed
//      in order with the same shift (old code kept only one queued release and
//      lost a double tap). A release made after the delayed press fired also
//      moves with it, so hold length is kept.
//    * Clean sim button state: each test starts from the game's real button
//      state with a fresh press edge (tests used to inherit the last test's).
//      Also syncs position, y-velocity, gravity, speed and used orbs.
//    * Fewer false alarms in ship / wave / robot / UFO / swing: a death more
//      than 100 ms after the click only counts if no follow-up input could
//      avoid it, so clicks you would have steered out of are left alone.
//    * Delays are only used when the delayed path is actually safe (UFO/swing
//      used to delay clicks whose delayed version still died, just later).
//    * Velocity orbs (yellow / pink / red / black) are modelled when the click
//      reaches one within 125 ms, so early orb clicks get buffered instead of
//      passing raw. Orb side effects are blocked and restored during tests.
//    * Each delay is simulated at most once per click (cached search).
//    * Extreme: earliest safe delay + margin by default (was mid-window, adding
//      ~7 ticks to every save), no spam guard, wider default windows.
//    * flipGravity hook moved to GJBaseGameLayer, the class that declares it.
//
//  NEW IN 1.5.0
//  ------------------------------------------------------------------------
//    * Live accuracy: every prediction is checked against the real player
//      100 ms after the click lands; hit rate in the panel and session log.
//    * Death replay: a click no delay in the window could save is probed
//      further, so the death line says how early it really was.
//    * (Timing coach - removed again in 1.7.0.)
//    * Auto windows: per-mode windows fitted to your measured timing error,
//      kept across sessions.
//    * Prediction through pads (strength learned from your real hits), speed
//      portals, gravity portals and blue / green orbs.
//    * Trajectory overlay, practice-only mode and an on-screen badge.
//
//  NEW IN 1.6.0
//  ------------------------------------------------------------------------
//    * Menu with Log / Stats / Settings tabs (replaced by the window in 1.7.0).
//      The Log keeps the last 300 lines even with every other log off.
//
//  NEW IN 1.7.0
//  ------------------------------------------------------------------------
//    * Draggable live window (section 9) replaces the F7 panel, the F9 coach
//      page and the popup menu. (1.8.1: Left Shift shows / hides it; no pause button.)
//    * Cube / ball / spider: the prediction lets go after your measured tap
//      length (holding re-jumps on landing in 2.2; holding forever predicted
//      bunny-hops and caused false saves).
//    * Ship / wave: "could you still save it" also tries a full release +
//      re-press cycle at your measured rhythm.
//    * Stats: accuracy per mode (cube, mini cube, ship, wave, ...) and your rhythm.
//
//  NEW IN 1.8.0
//  ------------------------------------------------------------------------
//    * One "Stage" setting: Off, Stage 1-4, Extreme. F6 cycles it, the window's
//      Stage tab sets it. Everything else is fixed defaults. (F6 used to guess
//      the current stage by comparing 8 window sliders and fell back to Stage 1
//      as soon as anything differed - "stages sometimes not working".)
//    * Search checks every tick in the first 12 ticks after a click before
//      striding: a coarse stride stepped over narrow safe gaps (Extreme missed
//      9 of 12 one-tick gaps in tests).
//    * Every jump input counts: mouse, keyboard, remapped keys, whichever
//      player flag GD delivers them with (player-2-flagged inputs used to pass
//      raw). Key auto-repeat / a second held key is never a new click.
//    * "Could you still save it" checks stop just past the original death:
//      far cheaper, so dense levels don't run out of CPU time and go raw.
//    * Orbs count a quick tap GD hasn't used up yet (its jump buffer).
//
//  NEW IN 1.9.0 (Extreme)
//  ------------------------------------------------------------------------
//    * Live re-planning (BufferCore liveStep): a held input is re-tested every
//      tick from the REAL game state and fires at the first safe tick, instead
//      of a delay predicted at click time (which drifts over long delays).
//      Releases and every input queued behind a held one get the same check.
//    * Cube / ball / spider / robot landing rule: a click is judged on getting
//      back to the ground; a death after landing is the next jump's job (dense
//      levels made clicks look doomed and pass raw).
//    * Extreme ship / wave window 6 -> 10 frames.
//    * Death counted once per death (practice mode / noclip called it every tick
//      and reset the buffer); deaths blamed on the press, not a cube release.
//    * Window key: Left Shift. Pause-screen button removed. Window half size.
//
//  NEW IN 1.9.1
//  ------------------------------------------------------------------------
//    * A buffered input is checked to have reached the player on the tick the
//      prediction chose. Input mods (Click Between Frames) can hold back or
//      re-time inputs they didn't create; if that happens the input is applied
//      to the player directly (and from then on always directly).
//    * "Show paths" (Stage tab): your click as made (red if deadly) and the
//      buffered one (green), redrawn from the real state when it fires.
//
//  NEW IN 1.10.0
//  ------------------------------------------------------------------------
//    * Wider windows: Stage 1 1f, 2 2f, 3 4f (3 ship/wave), 4 8f (5),
//      Extreme 20f (12).
//    * Show paths is live: the path of your next input right now (press, or
//      release while holding), with the player hitbox at both ends; green safe,
//      red deadly, yellow cut short. While a click is held, the path being
//      tested for it.
//
//  NEW IN 1.10.1
//  ------------------------------------------------------------------------
//    * The lookahead is never shortened to save CPU time any more. It used to
//      shrink to ~1/3 after budget overruns, so predictions couldn't see the
//      next spike: "safe" clicks died ~250 ms later and buffered clicks were
//      planned on too short a view. Out of time now = only that click is raw.
//    * More CPU time per click (half a frame, 3/4 in Extreme, up to 8 ms);
//      lighter "how early" probe; the path preview gets a third of the budget.
//
//  NEW IN 1.10.2
//  ------------------------------------------------------------------------
//    * Cube live check uses your REAL hold length (queued release, or how long
//      you've held so far) instead of the typical tap: holding through a landing
//      re-jumps in 2.2, which the typical-tap guess missed ("buffered still died").
//    * Log: too-late clicks say so ("too late: dies in 5t, no delay helps");
//      buffered / raw / death lines show the gamemode.
//
//  NEW IN 1.11.0  (accuracy)
//  ------------------------------------------------------------------------
//    * Clearance: every test also measures how close the path comes to a
//      hazard. A "safe" path that misses a spike by 0.3px is a coin flip in the
//      real game (the sim is off by ~1-2px). While a click is held, the live
//      check now waits up to 4 base ticks for a moment that clears the hazard by
//      the room this gamemode needs (1.25 x its measured prediction error,
//      1-4px); if none is coming it fires at the close one as before.
//    * Moving hazards: hazards near you are sampled every physics step; ones
//      that move (move / rotate triggers) are tested where they WILL be at each
//      sim step instead of frozen where they are now. Only visible,
//      axis-aligned, enabled hazards; anything else keeps GD's own test.
//    * Custom stage: your own frames for cube-type modes, ship and wave, set in
//      the window with Apply. Uses Extreme's engine. In the F6 cycle.
//
//  NEW IN 1.12.0  (learn from attempts)
//  ------------------------------------------------------------------------
//    * Spot memory: every input that reaches the game is remembered by where
//      it landed (position, vertical speed, gamemode, press / release,
//      gravity). Alive a moment later (100 ms in ship / wave / UFO / swing,
//      landed again in cube-type modes) = good spot; died first = bad spot.
//    * Next attempts: a held input fires the moment it reaches a good spot
//      (and waits for one coming up, if waiting is safe), and never fires on a
//      bad spot, whatever the prediction says. A click on a bad spot that the
//      sim calls safe is held and re-planned; a "no fix" click with a good spot
//      ahead in the window is held for it. Kept for the level, all attempts.
//      Stage tab: "Learn from attempts" (on by default).
//
//  NEW IN 1.12.1  (fixes)
//  ------------------------------------------------------------------------
//    * Out of CPU time on a deadly click: held and re-planned live (one cheap
//      test per tick) instead of going through raw ("raw: CPU budget" deaths).
//    * Releases: inputs queued behind your click only get the plain safety
//      check (no near-miss / learned-spot waits), so hold lengths stay yours.
//      Ship / wave / robot checks use your real release (or next press) when
//      it's already known, instead of "held forever".
//    * Cube: a learned spot in the air never fires a press (it wouldn't jump).
//    * Log: every held click says when it really fired and why (safe, learned
//      spot, window ended...); deaths say if it hadn't fired yet.
//    * Window: lives on Geode's overlay (menus, level select, levels), and the
//      Window Key is read straight from the keyboard (Left Shift works alone).
//
//  NEW IN 1.12.2
//  ------------------------------------------------------------------------
//    * Window Key on Windows is polled every frame (only while GD is focused),
//      because input mods like Click Between Frames take the keyboard events.
//    * Closing the window with X says how to bring it back.
//
//  NEW IN 1.12.3
//  ------------------------------------------------------------------------
//    * Window Key setting has a new id ("window-key-v3", default Left Alt):
//      older installs had kept F10 from an early version. Window switched on
//      once after updating.
//
//  NEW IN 1.13.0  (cube / mini cube: held clicks that never fired)
//  ------------------------------------------------------------------------
//    * The click-time search found a safe moment (+Nt), but the tick-by-tick
//      live check refused every tick until you died ("held click hadn't fired
//      yet"). Now a held click fires on its click-time plan at most 2 base
//      ticks after it, however often the live check refuses. The live check can
//      still fire it EARLIER (first safe tick) - it just can't hold it forever.
//    * Log: a click fired on its plan, or at the end of the window, says what the
//      live check objected to ("live: said no 14x: died 12 (dies 9t after
//      firing, landed at 30t), memory 2") so the disagreement can be fixed.
//
//  NEW IN 1.13.1
//  ------------------------------------------------------------------------
//    * Learned spots are saved per level (<save dir>/spots/online-<id>.txt or
//      local-<hash>.txt), written at every death and when you leave, and loaded
//      when you open the level again. Stats shows the file and how many came
//      from earlier sessions. Stage tab: "forget level" wipes them.
//
//  NEW IN 1.13.2
//  ------------------------------------------------------------------------
//    * A held click the live check keeps refusing fires EXACTLY on its
//      click-time plan (was plan + 2 ticks: too late in narrow gaps).
//    * Moving hazards: speed measured over a whole frame, not step to step
//      (uneven / CBF steps made it flip between 0 and 3x), and only trusted
//      once it has predicted the hazard's position correctly twice in a row.
//    * Refusal log says what the predicted jump hit ("hit MOVING spike at
//      +40px height") so a wrong prediction can be traced.
//    * New window look: rounded panel, stage-colored accent line and status
//      pill, highlighted tab, striped rows, GD close button.
//
//  NEW IN 1.14.0
//  ------------------------------------------------------------------------
//    * Modes tab (next to Stage): switch the buffer on / off per gamemode,
//      normal and mini separately (cube, ship, ball, UFO, wave, robot, spider,
//      swing). Off = that mode's clicks go through raw. "all on" / "all off".
//
//  NEW IN 1.14.1  (wave / ship releases)
//  ------------------------------------------------------------------------
//    * Learned spots are presses only: deaths in wave got blamed on the last
//      release, and saved "bad releases" held good releases back.
//    * Near-miss waits are presses only (wave corridors are always close).
//    * An input queued behind a held one (your release) is held at most 2
//      frames past its time, not the whole Extreme window.
//
//  NEW IN 1.14.2  (wave / ship on Stage 3, 4, Extreme)
//  ------------------------------------------------------------------------
//    * A ship / wave input is judged against YOUR rhythm: a press only counts
//      as deadly if it dies before your usual release (+4 ticks), or no
//      release at your rhythm saves it; same for a release and your usual gap.
//      Before, it assumed you'd hold at least 50 ms, so normal wave taps looked
//      deadly and were delayed - on the bigger stages by many ticks, with the
//      release shifted along ("late jumps", "holds feel off").
//
//  NEW IN 1.14.3  (wave)
//  ------------------------------------------------------------------------
//    * Ship / wave clicks that run out of CPU time go through raw again (as
//      before 1.12.1). Holding them for the live check left wave presses held
//      the whole 48-tick window ("held click hadn't fired yet ... planned +0t").
//      Cube-type modes keep the live fallback.
//    * Any held click with no plan to fall back on gives up after 1 frame
//      instead of waiting out the window.
//
//  NEW IN 1.15.0  (two engines)
//  ------------------------------------------------------------------------
//    * Mini cube keeps the current engine (it was tuned on Final Destination):
//      moving hazards, near-miss waits, learn from attempts, live fallback.
//    * Every other gamemode is back to the 1.10.2 behaviour: none of those, no
//      release cap, no rhythm judging, out of CPU = raw. See modernEngine().
//    * Both keep: a held click fires on its click-time plan at the latest.
//
//  NEW IN 1.15.1
//  ------------------------------------------------------------------------
//    * Inputs queued behind a held one (your release) fire exactly on their
//      scheduled time - your hold length, no re-check. The re-check could hold
//      wave releases for the whole window ("releases very late").
//
//  NEW IN 1.16.0  (wave spam)
//  ------------------------------------------------------------------------
//    * Wave inputs less than ~150 ms apart are a "stream". Once a stream has a
//      shift (an input got buffered), every following input gets the SAME
//      shift - or 1 tick less when that's safe too, so it winds down smoothly -
//      on a fixed schedule. Before, each input was judged alone (+3t, raw, +2t,
//      raw...), so hold lengths jumped around: the jitter.
//    * Those inputs are logged only with "Log: show safe clicks" on.
//
//  NEW IN 1.16.1
//  ------------------------------------------------------------------------
//    * "forget level" is always in the Stage tab (it only appeared when the tab
//      was opened inside a level); outside a level it says to open one.
//
//  NEW IN 1.16.2  (wave wobble)
//  ------------------------------------------------------------------------
//    * Wave judges presses / releases against your own hold / gap rhythm again
//      (1.14.2's rule, lost when 1.15.0 put wave back on the old engine):
//      short spam taps no longer look deadly and get delayed a tick or two.
//    * A stream keeps exactly the same shift until you pause (no 1-tick
//      wind-down per tap, which shortened one gap per tap).
//
//  NEW IN 1.17.0
//  ------------------------------------------------------------------------
//    * Changes tab in the window (every version, newest first) and changelog.md
//      (Geode shows it on the mod's page). (The in-game Changes tab was removed in 1.25.6.)
//
//  NEW IN 1.17.1
//  ------------------------------------------------------------------------
//    * Sim players now also copy position / last portal / last ground / landing
//      / sliding / direction / dash state from the real player (copyAttributes
//      doesn't). Wave tests were "dying" right after a release at impossible
//      heights, holding good releases back ("randomly goes up").
//
//  NEW IN 1.18.0  (performance: less chop)
//  ------------------------------------------------------------------------
//    * Held-click re-checks run once per physics tick (not on every CBF
//      sub-step too) with their own budget: 30% of a frame (min 0.8 ms).
//      A click due on its plan / window end still goes in on time.
//    * No extra full test of the chosen delay per buffered click (only drawn
//      for Show paths now).
//    * "How early was it" probe (log only): 1/8 budget, 8 coarse candidates.
//    * Moving-hazard bookkeeping only while in mini cube.
//    * Stats > PERFORMANCE: re-check cost, frame time, worst mod cost / frame.
//
//  NEW IN 1.18.1  (inputs never lost: Grief / Slaughterhouse reports)
//  ------------------------------------------------------------------------
//    * A repeated "press" while the mod thought you were already holding was
//      swallowed. After pausing while holding (the release happens in the pause
//      menu and never reaches us), that ate the next real press. Now only
//      swallowed while an input is actually waiting to fire.
//    * reconcile() never presses on its own (a stale "held" made it press).
//    * Pausing drops the held state and fires anything still waiting.
//    * If the physics-step hook (which fires held inputs) isn't running - another
//      mod replaced it, a long hitch - inputs go straight through.
//    * Watchdog: a held input waiting > 600 ms (wall clock) is fired.
//    * CPU budget checked every 4 sim steps (was 16): object-heavy levels can't
//      overrun it by much.
//
//  NEW IN 1.18.2  (heavy levels: "raw: CPU budget" spam)
//  ------------------------------------------------------------------------
//    * The first test of a click always gets >= 6 ms (it couldn't finish on
//      Grief / Slaughterhouse, so every click went raw); the search after it
//      gets a fresh budget of its own.
//    * Cube-type tests stop once you've landed and passed the 4-tick grace
//      (every verdict treats later deaths as the next jump's): ~40 fewer
//      collision-checked steps per test, same results.
//    * Search candidates get 1 "could you still save it" sample (was 2).
//    * Auto budget floor 2.5 ms (was 1.5). Stats: cost per predicted step.
//
//  NEW IN 1.18.3  (never give up on a deadly click)
//  ------------------------------------------------------------------------
//    * Compared with the original Predictive Buffer (which works on Grief /
//      Slaughterhouse for everyone): it has NO time limit and no follow-up
//      tests - every click is checked to the end, just simply. This one gave up
//      ("raw: CPU budget") when the full checks didn't fit the frame budget.
//    * Now: out of time on a deadly click -> finish with quick checks (no
//      follow-up tests; a death after your usual next input counts as yours to
//      steer out of) up to 12 ms from the click, instead of going raw.
//      Stats counts these clicks.
//
//  NEW IN 1.18.4  (diagnostics: "nothing in the Log")
//  ------------------------------------------------------------------------
//    * Inputs passed raw because the physics hook isn't running now say so in
//      the Log (once) - they used to pass silently, leaving it empty.
//    * Stats > CLICKS: inputs that reached the buffer (0 while clicking = another
//      mod handles inputs first).
//
//  NEW IN 1.18.5
//  ------------------------------------------------------------------------
//    * Clicks on an orb you're touching (within 6px, like GD's generous orb
//      area) are predicted through the orb instead of going raw
//      ("raw: pink orb (touching)" on Amethyst's UFO).
//
//  NEW IN 1.18.6  (orb clicks)
//  ------------------------------------------------------------------------
//    * The real bug: after a test used an orb, the same orb (still being
//      touched) stopped the prediction on the next tick, so every click on an
//      orb went raw. A used orb, or one GD already used this contact, is now
//      ignored for the rest of the test.
//
//  NEW IN 1.18.7
//  ------------------------------------------------------------------------
//    * "too late" / "no fix" lines say what the predicted death hits and whether
//      the prediction used an orb (to find wrong predictions).
//
//  NEW IN 1.18.8
//  ------------------------------------------------------------------------
//    * Undid 1.18.6's "skip orbs in GD's touched-rings list": GD lists an orb
//      there as soon as you overlap it, so the orb being clicked was ignored and
//      every orb click said "too late, no delay helps".
//
//  NEW IN 1.19.0  ("too late" even when clicking early)
//  ------------------------------------------------------------------------
//    * The search checks every tick only for the first 12 after a click, then
//      every few ticks (every 5 on Extreme), and the "how early" probe even
//      coarser. A narrow safe moment far from an early click was stepped over,
//      and the click was called "too late". Now, in cube / UFO / robot / ball /
//      spider / swing, a click the quick search can't fix is HELD and checked
//      every tick for the whole window, firing on the first safe tick (no
//      margin). Ship / wave keep the old behaviour.
//    * No-fix lines no longer claim "too late" (the coarse probe couldn't know).
//
//  NEW IN 1.19.1  (UFO: held clicks that never fired)
//  ------------------------------------------------------------------------
//    * UFO / swing: an orb only fires in the sim on a fresh click (press within
//      ~3 ticks), not because the button is still held.
//      Holding made every "fire now" test hit the orb and die, so nothing was
//      ever safe and the held click never fired.
//    * A held click checked every tick fires 2 ticks before doing nothing would
//      kill you, at the latest (it used to wait while you died).
//
//  NEW IN 1.19.2
//  ------------------------------------------------------------------------
//    * UFO / swing: no tick-by-tick hold for clicks the quick search can't fix
//      (raw again). In Amethyst's UFO part the prediction was wrong about both
//      pressing and not pressing, so held clicks never fired.
//
//  NEW IN 1.20.0  (portals)
//  ------------------------------------------------------------------------
//    * Gamemode and size portals are switched through in the sim (PlayerObject
//      toggle*Mode / togglePlayerScale on the hidden sim player, no effects)
//      instead of stopping the prediction. Clicks just before a portal - like an
//      orb right after a UFO portal - can be buffered now. The sim player is
//      put back in the real mode / size at the start of every test.
//    * Still not modelled (prediction stops): dual, mirror, teleport portals,
//      dash / spider / teleport orbs. The gamemode area's floor / ceiling after
//      a fly portal is not modelled either.
//
//  NEW IN 1.20.1
//  ------------------------------------------------------------------------
//    * 1.20.0 REVERTED: switching the hidden sim player's gamemode (toggle*Mode)
//      messed up the real portals. Gamemode / size portals stop the prediction
//      again, exactly as in 1.19.2.
//
//  NEW IN 1.20.2  (diagnostics only)
//  ------------------------------------------------------------------------
//    * Accuracy per mode also shows WHICH WAY predictions are off on average
//      (sideways vs up/down), and each "prediction off" log line too. Normal
//      UFO was off ~5px every time while mini UFO was exact.
//
//  NEW IN 1.21.0  (line up clicks the prediction can't see)
//  ------------------------------------------------------------------------
//    * Learn from attempts also records UFO presses (was mini cube only).
//    * An early click the prediction can't line up - blind past a portal or orb
//      ("raw: ... ahead"), or "no safe moment found" - is held when a spot where
//      a press WORKED on an earlier attempt is ahead within the window, in any
//      gamemode (spots match by position, so a spot past a UFO portal is found
//      from before it). It fires when you reach that spot (no prediction used),
//      or 2 ticks after its estimated arrival at the latest.
//    * Spots files get a 7th column (older files still load).
//
//  NEW IN 1.21.1
//  ------------------------------------------------------------------------
//    * UFO gets the rest of learn from attempts: a held UFO click fires on a
//      spot that worked before (even if the prediction disagrees), and a click
//      on a spot that killed you before is held and re-planned.
//
//  NEW IN 1.22.0  (moving spikes / orbs everywhere but ship / wave)
//  ------------------------------------------------------------------------
//    * Moving-hazard prediction (was mini cube only) now runs in every mode
//      except ship / wave: a spike that moves out of the way is tested where it
//      WILL be, so the small window it opens can be buffered ("no safe moment
//      found" while a spike was about to move). Same proof rule as before (a
//      speed is only used after it predicted the object right twice).
//    * Moving orbs too: sampled like hazards and used at their predicted spot.
//    * Near-miss clearance stays mini cube only (CPU).
//
//  NEW IN 1.22.1
//  ------------------------------------------------------------------------
//    * Moving objects: acceleration tracked (move triggers usually ease in /
//      out, which a constant-speed model never matched - they were never
//      trusted); slowing objects stop instead of reversing; trusted after one
//      correct prediction (within 3px / 25%), speed measured over half a frame.
//
//  NEW IN 1.22.2
//  ------------------------------------------------------------------------
//    * Removed the 1.19.0 "no safe moment found -> hold and check every tick"
//      behaviour: such clicks go straight through again, at once.
//    * "no safe moment found" / "no fix" log lines only with "Log: show safe
//      clicks" on.
//
//  NEW IN 1.23.0  (prediction: exact orbs + one-tick self-check)
//  ------------------------------------------------------------------------
//    * Exact orb contact: orbs the sim can use (yellow / pink / red / black /
//      blue / green) go through GD's own collision for the sim player, which
//      reports the exact tick it overlaps the orb's real hitbox
//      (playerTouchedRing, swallowed for sim players). Once seen, the box-based
//      orb guess is off and modelled orbs never cut the prediction.
//    * One-tick self-check (diagnostics only): every 8 ticks the sim predicts one
//      tick from your real state and compares with the game. Stats shows, per
//      mode, how often it's exact and the average x / y / y-speed difference; the
//      Log notes the first real mismatch per mode.
//
//  NEW IN 1.23.1
//  ------------------------------------------------------------------------
//    * Sim set-up: button state BEFORE speed / position. pushButton is a new
//      press (a flap in UFO), and coming last it gave every test made while you
//      held an extra flap (UFO one-tick check 72% exact, y-speed off by ~1).
//    * Learned-spot holds: at most 16 base ticks ahead (a 79-tick hold in UFO
//      let you fall, so the press landed below the orb), and only if not
//      pressing until then doesn't kill you first.
//
//  NEW IN 1.23.2
//  ------------------------------------------------------------------------
//    * Sim set-up copies all of the real player's plain jump / fall / slope /
//      force / timer state after the button (see syncSimPlayer), not just a
//      few fields: the "press waiting" state pushButton leaves behind made the
//      sim flap on its first tick (UFO one-tick check stayed ~67%).
//
//  NEW IN 1.23.3  (tick grid)
//  ------------------------------------------------------------------------
//    * With Click Between Frames a click lands mid-tick. The sim used to step
//      whole ticks from that moment, but held inputs can only fire on tick
//      boundaries, so "+9t" fired at the boundary after 9.x ticks - up to a
//      whole tick late (frame-perfect orbs missed / pressed low, live check
//      disagreeing with the plan). Now the mod tracks where it is inside the
//      tick (m_phase), every sim's first step is the REST of the current tick,
//      and held inputs are scheduled on the real tick grid.
//
//  NEW IN 1.24.0  (portal hold)
//  ------------------------------------------------------------------------
//    * A press with a gamemode / size portal at most 40px ahead used to go raw
//      ("raw: size portal ... 6px ahead"). Now it's held until you're through
//      the portal(s); then the live check (which can see again) fires it on the
//      first safe tick. Not in ship / wave. If no safe tick comes in the window
//      it fires at the window end.
//
//  NEW IN 1.24.1
//  ------------------------------------------------------------------------
//    * UFO / swing press while already touching an orb: the sim uses the orb on
//      that same tick with no flap, like GD. It flapped first and used the orb a
//      tick later, so the orb arc started late and high ("no fix ... hits object
//      at +22px, after using an orb"; accuracy check -6.8px up/down).
//    * Learn from attempts: mini cube only again (UFO spots fired presses
//      +17..+42 ticks late at spots from older attempts).
//
//  NEW IN 1.24.2
//  ------------------------------------------------------------------------
//    * Portal hold tightened: only a press at most 4 ticks before a gamemode /
//      size portal is held (was 40px, ~30 ticks), and it fires at most 6 ticks
//      after reaching the portal (was: whenever the prediction liked, up to the
//      window end - presses fired +17t / +35t late and every click queued
//      behind them shifted with them).
//
//  NEW IN 1.24.3
//  ------------------------------------------------------------------------
//    * Orb press check: a UFO / swing press on an orb is predicted three ways
//      (orb first with no flap / flap then orb / press + orb together). On your
//      own orb presses that go in on time, all three are compared with where you
//      really are 10 ticks later, and the closest one is used from then on
//      (saved). Shown in Stats > ONE-TICK CHECK and as "orb check" log lines.
//    * The orb a press uses at the start must really overlap you and not be
//      used already (a stale "touching" entry could fire a used orb).
//    * Deaths name spikes / saws / blocks / slopes instead of "object".
//
//  NEW IN 1.24.4
//  ------------------------------------------------------------------------
//    * Orb snap: a UFO / swing press that kills you now, with an unused orb
//      coming (reached without pressing, inside the window, before anything
//      kills you), is held and fired on the tick GD has you touching that orb -
//      or the tick the prediction has you touching it. The orb jump itself
//      isn't predicted for this (it was 6-18px off), so a wrong orb arc can't
//      make it say "no fix" any more. The orb check showed all three press
//      models equally off: the orb jump, not the press, is what's off.
//
//  NEW IN 1.24.5
//  ------------------------------------------------------------------------
//    * Orb jump measurement: on an orb press that went in on time, the log
//      shows the tick GD used the orb on and the y-speed it gave, next to the
//      prediction's tick and y-speed ("orb jump [pink orb]: game ... |
//      predicted ..."). Same tick but a different speed: the prediction learns
//      a correction for that orb type (and size) from then on.
//    * Sim players use orbs with the same second ringJump argument GD uses.
//
//  NEW IN 1.24.6
//  ------------------------------------------------------------------------
//    * The measurement showed the sim players' orb call does nothing: pink and
//      green orbs both came out at y-speed 6.87 (a plain flap) while the game
//      gave 4.46 (pink) and -10.62 + gravity flip (green). Every "after using an
//      orb" prediction was really a flap - why the first orb was never
//      bufferable.
//    * Orb table: every orb you really use records the y-speed right after it
//      and whether gravity flipped, per orb type / gamemode / size / gravity.
//      Predictions set exactly that when they use the orb. Saved across
//      sessions; listed in Stats.
//    * Removed 1.24.5's y-speed ratio (learned from that same no-op call).
//
//  NEW IN 1.24.7
//  ------------------------------------------------------------------------
//    * Orb aim replaces orb snap: a deadly UFO / swing press with an orb coming
//      tests EVERY tick you'd overlap the orb (up to 12) and fires on the one
//      whose orb jump keeps you alive longest. The normal search could never
//      pick an orb tick: it wants safe ticks around its pick (margin) and
//      strides over far ticks, and an orb is a 1-2 tick window. 1.24.4's snap
//      ran the window with the full lookahead and ran out of CPU every click
//      (Stats: "held for an orb: 0").
//    * Fires exactly on the tested tick (no waiting on GD's touch list).
//    * Orb press model defaults to GD's order (flap step, then the orb) and only
//      switches if another one is clearly closer; old orb checks (made while the
//      orb did nothing) are discarded.
//
//  NEW IN 1.24.8
//  ------------------------------------------------------------------------
//    * Orb aim never started ("held for an orb: 0"): GD reports orb contact only
//      while the button is down, so the no-press path never saw the orb. Orb
//      contact on that path now comes from the hitboxes; GD's own contact still
//      decides which press ticks really use the orb.
//    * A press held through a size / gamemode portal is aimed at the orb after
//      it once the prediction can see past the portal (it fired at the portal
//      cap into the spikes before).
//    * With "Log: show safe clicks": why orb aim didn't aim (no orb ahead / no
//      press uses it).
//
//  NEW IN 1.24.9
//  ------------------------------------------------------------------------
//    * GD sets an orb's m_isActivated as soon as you TOUCH it, not when it's
//      used. Orb aim skipped "activated" orbs - i.e. the orb you were in - so it
//      always said "no orb ahead", and the same check blocked the "orb first"
//      press model (why all three orb-press models always tied). Used orbs now
//      come from the mod's own record of the orbs you really used this attempt.
//
//  NEW IN 1.25.0
//  ------------------------------------------------------------------------
//    * THE orb bug (since 1.23.0's exact orb contact): only the FIRST test of a
//      click could use an orb. A test touching an orb leaves GD's "already
//      activated" state on it (its per-player flags were never put back, and
//      m_isActivated was snapshotted after the touch), so every later test -
//      the search, orb aim, the live check - was told the orb was gone. So an
//      early click never found the orb tick ("no safe moment" / "too late"),
//      while a click ON the orb (the first test) did use it. Now:
//        - sim players may use any orb you haven't really used this attempt
//          (and not the one the test already used), whatever GD's flags say;
//        - orbs' m_isActivated is saved before GD sees them in a test and put
//          back oldest-first.
//    * "Log: show safe clicks": orb aim lists what every tested tick did
//      ("+3:orb dies 14t  +4:orb ok ...").
//
//  NEW IN 1.25.1
//  ------------------------------------------------------------------------
//    * Size portals no longer stop the prediction. In Amethyst's UFO part the
//      orbs sit right at mini / normal size portals, so EVERY orb click there
//      went "raw: size portal (mini) 6px ahead" and nothing (search, orb aim)
//      ever ran. The sim player's size is set directly (m_vehicleSize + scale)
//      when it overlaps a size portal - no GD portal code is called and the real
//      portal object is never touched (1.20.0's GD toggles broke real portals).
//      Each test starts from your real size again.
//    * Learned orbs: an orb at a size / gravity not learned yet uses the same
//      orb's other size / gravity (mirrored) instead of the sim's own orb.
//    * Stats: "size portals passed inside predictions".
//
//  NEW IN 1.25.2  (log clean-up)
//  ------------------------------------------------------------------------
//    * Orb check / orb jump lines only when the orb prediction is off (> 2px).
//    * Gone: "REL raw (no effect here)", "orb aim: no orb ahead".
//    * Orb aim tests grouped: "+0 orb, dies 11t | +1..+10 flap, dies 7..1t".
//    * A click on an orb's last usable tick that still dies said "PRESS ok
//      (safe 11t)" (and "sim said safe" at the death): it keeps the real
//      verdict now, plus "PRESS too late for the green orb: ... click a bit
//      earlier".
//
//  NEW IN 1.25.3
//  ------------------------------------------------------------------------
//    * "Early UFO clicks go to the next orb" (Stage tab, on by default): a UFO /
//      swing press is aimed at the next orb even when pressing now would be a
//      safe flap - if you'd reach that orb without pressing, inside the window,
//      and pressing on it keeps you alive (whole lookahead). Before, only a
//      press that killed you now was aimed, so an early click far from the orb
//      just flapped. If you need a flap to reach the orb, nothing changes.
//
//  NEW IN 1.25.4
//  ------------------------------------------------------------------------
//    * "too late for the orb" only when nothing later keeps you alive either
//      (it showed up while a later flap was fine).
//    * Orb aim CPU: shorter tests (40t lookahead, 10 ticks max), and a safe
//      press you make while already on the orb isn't re-tested (the frame rate
//      dropped to ~124fps with it running on every UFO click).
//
//  NEW IN 1.25.5  (orb aim, thought through)
//  ------------------------------------------------------------------------
//    * Tests the WHOLE orb overlap, not its first 10 ticks. At 1x you overlap
//      an orb for ~40-50 ticks and the tick that clears the spikes is usually
//      late in it (on-time clicks were always "+0 orb, then flaps" - the end of
//      the overlap), so early clicks only ever tested deadly ticks.
//    * A tick "gets you through" if the orb is used and you survive the
//      lookahead OR reach the next orb with 6 ticks to spare (orb chains: the
//      next orb is your next click's job - like cube's landing rule). Before,
//      a chain's right tick looked deadly and the toggle refused to aim.
//    * Only a tick that gets you through is ever aimed at (it used to aim at
//      the "longest-surviving" tick even if it died, overriding the search).
//    * Fires in the MIDDLE of the longest stretch of working ticks.
//    * Tick picking moved to ibcore::pickOrbTick and unit tested: every tick
//      for overlaps up to 24, else every 3rd tick, then every tick +-2 around
//      working ticks, the orb's start / end, and the 3 longest survivors, then
//      (if still nothing) every remaining tick from the end back. Finds a
//      1-tick window at any position of a 50-tick overlap, with or without
//      hints. CPU: 2.5 ms per click at most; a test stops once it's safely at
//      the next orb.
//
//  NEW IN 1.25.6  (window)
//  ------------------------------------------------------------------------
//    * Changes tab removed (changelog.md stays). "Early UFO clicks go to the
//      next orb" is always on - its toggle is gone.
//    * New look: tabs as one bar with 4 equal segments, the Stage tab in three
//      cards (Stage / Custom frames / Options), the Modes tab in two cards, a
//      real scrollbar on Log / Stats, "clear" in the footer, an empty-log hint.
//    * "by AlQuerez" next to the title; "Discord: AlQuerez#3900" and the window
//      key (read from the Window Key setting, e.g. "Left Alt") on the Stage tab.
//
//  NEW IN 1.25.7  (window colours / buttons)
//  ------------------------------------------------------------------------
//    * Midnight + purple: the body is a vertical gradient (purple at the top
//      into midnight blue), a violet-to-pink line under the title, violet
//      selected tab / stage, lavender card titles, purple-tinted rows.
//    * Every tab and button is a rounded box and the WHOLE box is clickable
//      (tabs only reacted on their text before). Stage buttons, Apply, +/-,
//      forget level, all settings, clear, all on / off, scroll arrows - one
//      style (GD's green / grey button sprites are gone).
//    * White title (bigFont) instead of the yellow goldFont.
//
//  NEW IN 1.25.8  (window polish)
//  ------------------------------------------------------------------------
//    * Boxes are drawn as exact rounded polygons (CCDrawNode). The 9-slice
//      texture came out square on small boxes in the game - every button
//      looked like a block.
//    * The status pill text ("EXTREME"), the stage name and the close X are
//      white. The X is a box like the other buttons (GD's close sprite is green).
//    * Stats rewritten: short sections (This level / Prediction / Orbs / Your
//      rhythm / Windows / Performance) of "label  value" rows - label on the
//      left, value right-aligned. Debug numbers moved to a DETAILS section that
//      only shows with "Log: show safe clicks".
//
//  NEW IN 1.25.9
//  ------------------------------------------------------------------------
//    * Removed everything that held or moved a click for an orb (orb aim, the
//      early-click orb snap, aiming through portals), the gamemode-portal hold,
//      and the 3-way orb press check (3 extra predictions per UFO click). Kept,
//      because they only make predictions right and never hold a click: learned
//      orb speeds, "only the first test could use an orb" fix, size portals.
//    * Wave: an input in a spam burst goes in on time whenever that's safe - the
//      burst's delay used to stay on every input until you paused (lag).
//    * Learn from attempts: spots saved for other gamemodes by 1.21-1.24.0 are
//      dropped when a level's spots load (mini cube only, as intended).
//    * Stats: the first section is the level's name; section titles are bold
//      white; Windows shows what clicks really get (Extreme: 20f / 12f - it
//      showed the base 8f / 5f). Learned orbs moved to Details.
//    * Mouse wheel over the window scrolls only the window (CCMouseDispatcher
//      hook) - a level list behind it scrolled too.
//
//  NEW IN 1.26.0  (lighter: less work every frame)
//  ------------------------------------------------------------------------
//    * Moving-object tracking off: it logged every nearby spike / orb on every
//      physics step (240x a second) and never predicted anything in practice.
//    * Diagnostics only with "Log: show safe clicks": the one-tick check (a
//      prediction every 8 ticks) and accuracy tracking (every prediction's path
//      recorded and checked 100ms later), plus the "| live: said no ..." detail
//      on fired clicks.
//    * The window only rebuilds text that changed (header, footer, Log when a
//      line was added; Stats about once a second while a level runs) - all of
//      its labels were rebuilt 4x a second during play.
//
//  NEW IN 1.26.1  (smoother: follow the plan, in-game pill, new Log)
//  ------------------------------------------------------------------------
//    * Held clicks follow the click-time plan (not mini cube): one prediction
//      at click time records the path; each tick only compares your position
//      with it (<= 0.5px) - no full prediction per tick. It fires on the tick
//      the per-tick check would have chosen (the first safe tick with a safe
//      tick after it; wave: the first safe tick). Off the path -> the full
//      per-tick check takes over. Stats: "Held clicks fired on plan".
//    * "Small while playing" (on by default): a small pill (stage + latest Log
//      line) while a level runs, the full window when paused / in menus. The
//      hidden window costs nothing per frame.
//    * Click Log lines are built once, only if they'll be shown (they were
//      built twice, and hidden ones stored too).
//    * Log redesign: "time  TAG  text" - coloured tags (BUFFER, FIRED, DEATH,
//      RAW, QUEUED, OK, NOTE) and short texts.
//    * Polish: the window pops in, the tab highlight slides, the last tab is
//      remembered, Window Opacity setting (panels only, text stays solid);
//      "forget level" / "all settings" moved into the OPTIONS title row.
//    * "V2" badge next to the title.
//
//  NEW IN 1.26.2
//  ------------------------------------------------------------------------
//    * Ghost orb fixed: sim players no longer call GD's ringJump - it drew the
//      orb's circle effect where the prediction was (a "random orb" next to the
//      real one) and never moved the sim player anyway. The learned orb table
//      does the jump; blue / green orbs not learned yet still flip gravity.
//      PlayerObject circle effects are also blocked during tests.
//    * Android: mod.json lists android 2.2081; the mouse-wheel hook is desktop
//      only (GEODE_IS_DESKTOP); the keyboard hook is macOS only (Windows polls
//      the key). New on every platform: an "Input Buffer" button in the pause
//      menu shows / hides the window (the way to open it on Android).
//      .github/workflows/multi-platform.yml builds Windows + Android32/64 and
//      combines them into one .geode.
// ============================================================================

#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#ifdef GEODE_IS_DESKTOP
#include <Geode/modify/CCMouseDispatcher.hpp>
#endif
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/RingObject.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "BufferCore.hpp"

using namespace geode::prelude;

namespace {

using Clock = std::chrono::steady_clock;

// Tick constants are written for GD's default 240 TPS ("base ticks") and are
// scaled to whatever TPS the game is really running at.
constexpr float BASE_DT        = 0.25f; // one 240-TPS tick, in 60fps-frame units
constexpr int   MAX_SIM_STEPS  = 480;   // hard cap on the lookahead length
constexpr int   MIN_HORIZON    = 16;
constexpr int   MAX_CANDIDATES = 16;    // coarse steps over the far part of the window
constexpr int   DENSE_SCAN_BASE = 12;   // first 12 base ticks after a click are checked tick by tick
constexpr int   DT_RING        = 16;    // recent tick lengths kept to find the nominal tick
constexpr int   EXTREME_MIN_BUDGET_US = 6000; // manual-budget floor in extreme mode (auto budget off)
// Auto CPU budget: a share of the measured frame time, so the lookahead never
// costs a visible frame at high FPS (170fps = 5.9 ms per frame -> ~1.8 ms budget).
constexpr float AUTO_BUDGET_FRAC         = 0.50f;
constexpr float AUTO_BUDGET_FRAC_EXTREME = 0.75f;
constexpr int   AUTO_BUDGET_MIN_US       = 2500;
// The first test ("does the click as made kill you?") always gets at least
// this much: on object-heavy levels it couldn't finish inside a small budget
// and every click went raw.
constexpr int   R0_BUDGET_MIN_US         = 6000;
// Out of time on a deadly click: cheap checks may continue up to this (from
// the click), instead of the click going raw.
constexpr int   CLICK_HARD_CAP_US        = 12000;
constexpr int   AUTO_BUDGET_MAX_US       = 8000;
constexpr double TIME_EPS                = 1e-4; // 60fps-frame units
// Live re-check of a held click (every tick): this share of the frame time.
constexpr float LIVE_BUDGET_FRAC         = 0.30f;
constexpr int   LIVE_BUDGET_MIN_US       = 800;
// Safety nets: the physics-step hook counts as "not running" after this long
// without a call; a held input is force-fired after this long (wall clock,
// longer than the biggest window).
constexpr int   STEP_STALL_MS            = 150;
constexpr int   WATCHDOG_MS              = 600;
// A velocity orb counts as the target of a click if the sim player reaches it
// within this many base ticks after the click lands (125 ms).
constexpr int   ORB_INTENT_BASE          = 30;
// UFO / swing: an orb only fires on a fresh click, i.e. a press at most this
// many base ticks before touching it (GD's own click buffer is about that).
constexpr int   ORB_CLICK_BASE           = 3;
// "Could you still save this yourself?" (see interceptInput):
//   a death within COMMIT base ticks of the click (100 ms) is always the click's
//   fault - nobody corrects that fast, and it is exactly what the buffer is for;
//   a later death only counts if no follow-up input, made at least FOLLOW base
//   ticks (50 ms) after the click, avoids it.
constexpr int   ESCAPE_COMMIT_BASE       = 24;
constexpr int   ESCAPE_FOLLOW_BASE       = 12;
// Landing rule grace: dying within this many base ticks of touching down still
// counts as the click's fault (landing straight on a spike).
constexpr int   LAND_GRACE_BASE          = 4;
// Accuracy check: compare the real player with the prediction this many base
// ticks (100 ms) after the click lands; a hit is within ACCURACY_TOL_PX.
constexpr int   ACCURACY_CHECK_BASE      = 24;
constexpr float ACCURACY_TOL_PX          = 3.0f;
// Auto windows: samples needed, percentile covered, margin, floor (frames @60).
constexpr size_t AUTO_MIN_SAMPLES        = 12;
constexpr float  AUTO_PERCENTILE         = 0.90f;
constexpr float  AUTO_MARGIN_FRAMES      = 0.25f;
constexpr float  AUTO_MIN_FRAMES         = 0.25f;
// No-fix probe: how far past the window we look to tell you how early you were.
constexpr int   PROBE_MIN_BASE           = 40;
// Your input rhythm (60fps frames), used until enough real samples exist:
// typical tap length, and typical time between letting go and pressing again.
constexpr float  DEFAULT_TAP_FRAMES      = 4.0f;  // ~67 ms
constexpr float  DEFAULT_GAP_FRAMES      = 6.0f;  // ~100 ms
constexpr size_t RHYTHM_MIN_SAMPLES      = 6;
// Clearance (v1.11): a path that misses a hazard by less than the target is a
// "near miss". In live re-planning the buffer waits up to CLEAN_LOOK_BASE base
// ticks for a moment that clears it by the target (and by CLEAN_GAIN_PX more
// than now). Target = CLEAR_ERR_SCALE x this mode's measured prediction error.
constexpr float CLEAR_DEFAULT_PX         = 1.5f;
constexpr float CLEAR_MIN_PX             = 1.0f;
constexpr float CLEAR_MAX_PX             = 4.0f;
constexpr float CLEAR_ERR_SCALE          = 1.25f;
constexpr int   CLEAR_MIN_CHECKS         = 8;
constexpr float CLEAN_GAIN_PX            = 0.5f;
constexpr int   CLEAN_LOOK_BASE          = 4;
// Moving hazards: a velocity is trusted if measured within MOVER_FRESH frames
// (60fps units) and faster than MOVER_MIN_SPEED px/frame.
constexpr double MOVER_FRESH             = 2.0;
constexpr double MOVER_MAX_GAP           = 3.0;
constexpr float  MOVER_MIN_SPEED         = 0.05f;
constexpr float  MOVER_TELEPORT          = 200.f;
constexpr size_t MOVER_MAX_TRACKED       = 4096;
// Spot memory (learning from attempts): an input is "good" if you're alive
// this long after it (60fps frames): ship / wave / UFO / swing 100 ms; cube-type
// modes until you land again (+ grace), at most 0.5 s.
constexpr double SPOT_WINDOW_HOLD        = 6.0;
constexpr double SPOT_WINDOW_LAND_MAX    = 30.0;
constexpr double SPOT_LAND_GRACE         = 1.0;
// Ship / wave: a death within this many base ticks after your usual next input
// still counts as the click's fault (you can't react faster than your rhythm).
constexpr int    WAVE_GRACE_BASE         = 4;
// A held click the live check never approves, with no click-time plan to fall
// back on, goes in after this many base ticks (1 frame).
constexpr int    LIVE_GIVEUP_BASE        = 4;
// A press is held through a gamemode / size portal only if the portal is at
// most this far ahead (px) - i.e. you clicked right before it.
constexpr float  PORTAL_HOLD_PX          = 40.f;
constexpr int    PORTAL_HOLD_TICKS_BASE  = 4;  // only presses this close (in ticks) to the portal are held
constexpr int    ORB_CAL_STEPS           = 10; // orb press check: compare 10 ticks after the press
constexpr float  MINI_SIZE               = 0.6f; // GD's mini scale
constexpr bool   MOVERS_ON               = false; // moving-object tracking (1.26.0: off, see sampleMovers)
constexpr float  PLAN_DRIFT_PX           = 0.5f;  // held click: off the planned path by more than this = re-check (1.26.1)
constexpr int    ORB_AIM_MAX_TESTS       = 64; // orb aim: tests per click at most (the time limit below decides first)
constexpr int    ORB_AIM_FINE_SPAN       = 24; // overlap this short: every tick; longer: every 3rd, then around good ones
constexpr int    ORB_AIM_LOOK_BASE       = 40; // lookahead after each tested press
// Reaching the next orb (hitbox + 6px) at least this long before dying = "gets
// you through" (the 6px pad is ~4 ticks early at 1x: ~2 ticks left to click it).
constexpr int    ORB_CHAIN_SPARE_BASE    = 6;
constexpr long long ORB_AIM_BUDGET_US    = 2500; // orb aim: CPU per click at most
constexpr int    PORTAL_WAIT_AFTER_BASE  = 6;  // ...and fired at most this many ticks after reaching it
// Learn from attempts: an early click is held for a learned spot at most this
// many base ticks ahead (4 frames) - a longer hold changes your path.
constexpr int    SPOT_HOLD_MAX_BASE      = 16;
// Wave spam: inputs less than this apart (60fps frames, ~150 ms) are one stream;
// a stream keeps one shift until a pause.
constexpr double STREAM_GAP_FRAMES       = 9.0;
constexpr std::uintmax_t MAX_LOG_BYTES = 5u * 1024u * 1024u;

// ----------------------------------------------------------------------------
//  1. Settings
// ----------------------------------------------------------------------------
enum WinIdx {
    W_CUBE,     // cube / ball / spider
    W_UFO,
    W_SWING,
    W_ROBOT,
    W_SHIP,     // ship press
    W_SHIP_REL, // ship release
    W_WAVE,     // wave press
    W_WAVE_REL, // wave release
    W_COUNT
};

constexpr char const* WINDOW_KEYS[W_COUNT] = {
    "window-cube", "window-ufo", "window-swing", "window-robot",
    "window-ship", "window-ship-release", "window-wave", "window-wave-release",
};

// ----------------------------------------------------------------------------
//  Stages. One setting picks everything: Off, Stage 1-4, Extreme.
//  Windows are in 60fps frames (1.0 = one frame at 60fps) per WinIdx.
// ----------------------------------------------------------------------------
struct StageDef {
    char const* setting; // value in mod.json's "stage" dropdown
    char const* name;    // shown in notifications / the window
    bool   on;
    bool   extreme;
    float  w[W_COUNT];
};
constexpr StageDef STAGES[] = {
    {"Off",     "OFF (raw input)",                        false, false, {0, 0, 0, 0, 0, 0, 0, 0}},
    // order: cube, ufo, swing, robot, ship press, ship release, wave press, wave release
    {"Stage 1", "Stage 1: 1 frame",                       true,  false, {1, 1, 1, 1, 1, 1, 1, 1}},
    {"Stage 2", "Stage 2: 2 frames",                      true,  false, {2, 2, 2, 2, 2, 2, 2, 2}},
    {"Stage 3", "Stage 3: 4 frames (3 ship / wave)",      true,  false, {4, 4, 4, 4, 3, 3, 3, 3}},
    {"Stage 4", "Stage 4: 8 frames (5 ship / wave)",      true,  false, {8, 8, 8, 8, 5, 5, 5, 5}},
    {"Extreme", "EXTREME: 20 frames (12 ship / wave)",    true,  true,  {8, 8, 8, 8, 5, 5, 5, 5}},
    // Your own frames (window's Stage tab, Apply). Windows are filled in by Config::load.
    {"Custom",  "CUSTOM",                                 true,  true,  {0, 0, 0, 0, 0, 0, 0, 0}},
};
constexpr int STAGE_COUNT = static_cast<int>(sizeof(STAGES) / sizeof(STAGES[0]));
constexpr int STAGE_EXTREME = 5;
constexpr int STAGE_CUSTOM  = 6;

// Custom stage: frames (60fps) for cube-type modes (cube, ball, spider, UFO,
// swing, robot), ship and wave. Saved values, set from the window.
constexpr float CUSTOM_MIN_FRAMES = 0.5f;
constexpr float CUSTOM_MAX_FRAMES = 30.0f;
struct CustomFrames {
    float cube = 6.f, ship = 4.f, wave = 4.f;
    static float clampF(double v) { return std::clamp(static_cast<float>(v), CUSTOM_MIN_FRAMES, CUSTOM_MAX_FRAMES); }
    static CustomFrames load() {
        auto* mod = Mod::get();
        CustomFrames f;
        f.cube = clampF(mod->getSavedValue<double>("custom-cube", 6.0));
        f.ship = clampF(mod->getSavedValue<double>("custom-ship", 4.0));
        f.wave = clampF(mod->getSavedValue<double>("custom-wave", 4.0));
        return f;
    }
    void save() const {
        auto* mod = Mod::get();
        mod->setSavedValue<double>("custom-cube", cube);
        mod->setSavedValue<double>("custom-ship", ship);
        mod->setSavedValue<double>("custom-wave", wave);
    }
};

int stageFromSetting(std::string const& v) {
    for (int i = 0; i < STAGE_COUNT; i++) if (v == STAGES[i].setting) return i;
    return 1;
}

// Everything the engine reads. Only the stage, window size and two window
// preferences come from the user; the rest are fixed, tested defaults.
struct Config {
    int   stage        = 1;
    bool  enabled      = true;
    float window[W_COUNT] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // 60fps frames
    int   safetyTicks  = 1;     // extra delay margin, in 240-TPS ticks (not used in wave)
    float spamFrames   = 2.0f;  // inputs closer than this count as spam
    int   spamStreak   = 3;     // rapid inputs in a row before the buffer steps aside
    int   budgetUs     = 8000;  // CPU budget per click, microseconds (hard cap)
    bool  autoBudget   = true;  // cap the budget to a share of the measured frame time
    bool  cbfGuard     = true;  // only buffer to a delay where one tick later is also safe
    bool  orbPredict   = true;  // model yellow / pink / red / black / blue / green orbs
    bool  padPredict   = true;  // model pads (strength learned from real hits), speed + gravity portals
    bool  realTaps     = true;  // cube/ball/spider: the sim lets go after your usual tap length
    bool  accuracy     = true;  // compare predictions with what really happened
    bool  autoWindow   = false; // (feature kept in code, not exposed)
    float autoWindowMax = 4.0f;
    bool  calibReset   = false;
    bool  overlay      = false;
    bool  learnSpots   = true;  // learn from attempts (saved value, Stage tab)
    bool  practiceOnly = false;
    bool  showBadge    = false;
    bool  debug        = false; // Geode log (developer)
    bool  csv          = false; // CSV file (developer)

    // Window
    bool  hud          = true;  // window shown (saved value, toggled with the window key)
    float hudScale     = 1.0f;  // "Window Size" setting
    bool  hudVerbose   = false; // Log tab also lists safe clicks (saved value)

    // Extreme
    bool  extreme         = false;
    int   extremeWindow   = 80;   // base ticks @240 (20 frames) for every mode except ship/wave
    int   extremeHoldCap  = 48;   // base ticks @240 (12 frames) for ship/wave (live re-planning keeps it safe)
    float extremeAim      = 0.0f; // earliest safe delay (+ margin)
    bool  custom          = false; // Custom stage: your frames, Extreme's engine

    static Config load() {
        auto* mod = Mod::get();
        Config c;
        c.stage = stageFromSetting(mod->getSettingValue<std::string>("stage"));
        StageDef const& st = STAGES[c.stage];
        c.enabled = st.on;
        c.extreme = st.extreme;
        for (int i = 0; i < W_COUNT; i++) c.window[i] = st.w[i];
        if (c.stage == STAGE_CUSTOM) {
            // Exactly your frames, with Extreme's engine (live re-planning,
            // no spam guard, bigger CPU share) - but no 20 / 12 frame floor.
            CustomFrames const f = CustomFrames::load();
            c.custom = true;
            for (int i : {W_CUBE, W_UFO, W_SWING, W_ROBOT}) c.window[i] = f.cube;
            for (int i : {W_SHIP, W_SHIP_REL}) c.window[i] = f.ship;
            for (int i : {W_WAVE, W_WAVE_REL}) c.window[i] = f.wave;
            c.extremeWindow = 0;
            c.extremeHoldCap = 0;
        }
        c.hudScale   = static_cast<float>(mod->getSettingValue<double>("window-size"));
        c.hud        = mod->getSavedValue<bool>("window-shown", true);
        c.hudVerbose = mod->getSavedValue<bool>("log-safe-clicks", false);
        c.overlay    = mod->getSavedValue<bool>("show-paths", false);
        c.learnSpots = mod->getSavedValue<bool>("learn-spots", true);
        // Accuracy tracking records the path of every prediction and checks it
        // 100ms later: diagnostics, so only with "Log: show safe clicks" (1.26.0).
        c.accuracy = c.hudVerbose;
        return c;
    }
};

// ----------------------------------------------------------------------------
//  2. Gamemode profile (P1 and, in dual, P2 combined)
//
//  Detection reads the live PlayerObject flags every click, so it is never
//  stale. "Mini" is size-based and applies to every gamemode.
// ----------------------------------------------------------------------------
struct ModeInfo {
    bool wave = false, miniWave = false, ship = false;
    bool ufo = false, swing = false, robot = false;
    bool cube = false, ball = false, spider = false;
    bool mini = false;
    bool cubeLike = false; // cube, ball or spider: one input = one action

    void add(PlayerObject* p) {
        if (!p) return;
        bool any = false;
        bool const isMini = p->m_vehicleSize > 0.f && p->m_vehicleSize < 0.99f;
        if (isMini) mini = true;
        if (p->m_isDart) {
            wave = true;
            any = true;
            if (isMini) miniWave = true;
        }
        if (p->m_isShip)  { ship  = true; any = true; }
        if (p->m_isBird)  { ufo   = true; any = true; }
        if (p->m_isSwing) { swing = true; any = true; }
        if (p->m_isRobot) { robot = true; any = true; }
        if (!any) {
            cubeLike = true;
            if (p->m_isBall)        ball = true;
            else if (p->m_isSpider) spider = true;
            else                    cube = true;
        }
    }

    // Letting go changes the trajectory.
    bool releaseMatters() const { return wave || ship || robot; }
    // Hold-based: the sim holds the button for the whole horizon, so a delayed
    // path naturally shifts the hit time and needs the stricter acceptance rule.
    bool holdBased() const { return wave || ship; }
    // No natural "landing" to end a lookahead early.
    bool noLanding() const { return wave || ship || ufo || swing; }
    // Rapid tapping is normal in these modes.
    bool spamGuarded() const { return wave || ship || ufo || swing; }

    // Lookahead length in base ticks (240 TPS). Hold-based modes use a short
    // horizon because the sim never releases, so far-away "deaths" are fake.
    int baseHorizon() const {
        if (miniWave) return 50;
        if (wave)     return 65;
        if (ship)     return 56;
        if (ufo || swing) return 90;
        return 160;
    }

    // Configured window in 60fps frames (before extreme mode).
    float windowFrames(Config const& c, bool push) const {
        float w = 1e9f;
        if (wave) w = std::min(w, c.window[push ? W_WAVE : W_WAVE_REL]);
        if (ship) w = std::min(w, c.window[push ? W_SHIP : W_SHIP_REL]);
        if (robot) w = std::min(w, c.window[W_ROBOT]);
        if (push) {
            if (ufo)      w = std::min(w, c.window[W_UFO]);
            if (swing)    w = std::min(w, c.window[W_SWING]);
            if (cubeLike) w = std::min(w, c.window[W_CUBE]);
        }
        return (w > 1e8f) ? c.window[W_CUBE] : w;
    }

    // Effective window in physics ticks. Extreme mode only ever WIDENS it.
    int windowTicks(Config const& c, bool push, float dt) const {
        int t = static_cast<int>(std::floor(windowFrames(c, push) / dt + 0.001f));
        if (c.extreme) {
            float const s = BASE_DT / dt;
            int ext = holdBased() ? c.extremeHoldCap : c.extremeWindow;
            ext = static_cast<int>(std::lround(ext * s));
            t = std::max(t, ext);
        }
        return t;
    }

    std::string name() const {
        std::string n;
        auto add = [&](char const* s) {
            if (!n.empty()) n += '+';
            n += s;
        };
        if (wave)   add("wave");
        if (ship)   add("ship");
        if (ufo)    add("ufo");
        if (swing)  add("swing");
        if (robot)  add("robot");
        if (cube)   add("cube");
        if (ball)   add("ball");
        if (spider) add("spider");
        if (n.empty()) n = "?";
        return mini ? ("mini " + n) : n;
    }
};

// ----------------------------------------------------------------------------
//  Per-gamemode on / off (window: Modes tab). Saved values "mode-on-<id>".
// ----------------------------------------------------------------------------
struct ModeToggle { char const* id; char const* label; };
constexpr ModeToggle MODE_TOGGLES[] = {
    {"cube", "Cube"}, {"ship", "Ship"}, {"ball", "Ball"}, {"ufo", "UFO"},
    {"wave", "Wave"}, {"robot", "Robot"}, {"spider", "Spider"}, {"swing", "Swing"},
};
constexpr int MODE_TOGGLE_COUNT = static_cast<int>(sizeof(MODE_TOGGLES) / sizeof(MODE_TOGGLES[0]));

// "cube" / "mini-cube" / "wave" ... for one player.
std::string modeId(PlayerObject* p) {
    if (!p) return "cube";
    char const* base = p->m_isDart ? "wave" : p->m_isShip ? "ship" : p->m_isBird ? "ufo" : p->m_isSwing ? "swing"
                     : p->m_isRobot ? "robot" : p->m_isBall ? "ball" : p->m_isSpider ? "spider" : "cube";
    bool const mini = p->m_vehicleSize > 0.f && p->m_vehicleSize < 0.99f;
    return mini ? fmt::format("mini-{}", base) : std::string(base);
}

bool modeOn(std::string const& id) {
    return Mod::get()->getSavedValue<bool>("mode-on-" + id, true);
}

// Which engine a gamemode uses (1.15.0):
//   * mini cube: the current one (moving hazards, near-miss waits, learn from
//     attempts, live fallback when out of CPU) - tuned on Final Destination;
//   * everything else: the 1.10.2 behaviour, which played better there.
// Both keep "a held click fires on its plan at the latest".
bool modernEngine(ModeInfo const& m) {
    return m.cube && m.mini && !(m.ship || m.wave || m.ufo || m.swing || m.robot || m.ball || m.spider);
}

// The window a click in this mode "belongs" to (auto windows, rhythm).
WinIdx primaryWin(ModeInfo const& m, bool push) {
    if (m.wave)  return push ? W_WAVE : W_WAVE_REL;
    if (m.ship)  return push ? W_SHIP : W_SHIP_REL;
    if (m.robot) return W_ROBOT;
    if (m.ufo)   return W_UFO;
    if (m.swing) return W_SWING;
    return W_CUBE;
}

char const* winLabel(int idx) {
    switch (idx) {
        case W_CUBE:     return "cube/ball/spider";
        case W_UFO:      return "ufo";
        case W_SWING:    return "swing";
        case W_ROBOT:    return "robot";
        case W_SHIP:     return "ship press";
        case W_SHIP_REL: return "ship release";
        case W_WAVE:     return "wave press";
        case W_WAVE_REL: return "wave release";
        default:         return "?";
    }
}

inline int scaleTicks(int baseTicks, float s) {
    return std::max(1, static_cast<int>(std::lround(baseTicks * s)));
}

// ----------------------------------------------------------------------------
//  3. Object classification and prediction cut-offs
//
//  The sim players only model solid blocks, hazards and slopes. Anything that
//  changes mode / gravity / size / speed, or launches the player (portals,
//  pads, orbs, speed changers) is NOT modelled. The lookahead must therefore
//  stop when the player is about to interact with one, because everything
//  after that point would be a guess.
//
//  The old code stopped whenever such an object was inside a fixed 52x72 box
//  around the player, in any direction. That also caught objects BEHIND the
//  player (e.g. the mini-size portal you had just flown through), which made
//  the buffer give up immediately and report "orb/portal - raw input".
//  Now each object is judged by its real hitbox and by whether the player can
//  still touch it (see TrajectoryBufferSim::checkStateChange).
// ----------------------------------------------------------------------------
enum class SimEnd : uint8_t {
    Survived,     // reached the horizon (or landed safely)
    Died,         // hit something lethal
    Truncated,    // reached a portal / pad / orb we can't predict past
    OutOfBudget   // ran out of CPU budget -> result is not trustworthy
};

char const* simEndName(SimEnd e) {
    switch (e) {
        case SimEnd::Survived:    return "survived";
        case SimEnd::Died:        return "died";
        case SimEnd::Truncated:   return "truncated";
        case SimEnd::OutOfBudget: return "budget";
    }
    return "?";
}

enum class ObjClass : uint8_t { Ignore, Collide, StateChange };

ObjClass classify(GameObjectType t) {
    switch (t) {
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
        case GameObjectType::CollisionObject:
        case GameObjectType::Slope:
            return ObjClass::Collide;

        // Objects that change mode / gravity / size / speed or launch the player.
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::CubePortal:
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::InverseMirrorPortal:
        case GameObjectType::NormalMirrorPortal:
        case GameObjectType::BallPortal:
        case GameObjectType::RegularSizePortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::Modifier:
        case GameObjectType::DualPortal:
        case GameObjectType::SoloPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::TeleportPortal:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::SpiderPortal:
        case GameObjectType::RedJumpPad:
        case GameObjectType::RedJumpRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SwingPortal:
        case GameObjectType::GravityTogglePortal:
        case GameObjectType::SpiderOrb:
        case GameObjectType::SpiderPad:
        case GameObjectType::TeleportOrb:
            return ObjClass::StateChange;

        default:
            return ObjClass::Ignore;
    }
}

// Orbs fire on a CLICK while overlapped; everything else fires on contact.
bool isOrb(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SpiderOrb:
        case GameObjectType::TeleportOrb:
            return true;
        default:
            return false;
    }
}

// Orbs the sim can use itself: speed-only orbs, plus blue / green orbs whose
// gravity flip is applied to the sim player alone (see the flipGravity hook).
// Dash / teleport / spider / custom orbs can't be modelled.
bool isModelledOrb(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::DropRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
            return true;
        default:
            return false;
    }
}

// Pads the sim can use once their strength has been seen on a real hit.
// (Spider pads teleport and are not modelled.)
bool isModelledPad(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::GravityPad:
            return true;
        default:
            return false;
    }
}

// Speed portals by object ID -> player speed value GD uses for them.
bool speedForPortal(int objectID, float& out) {
    switch (objectID) {
        case 200:  out = 0.7f; return true; // 0.5x
        case 201:  out = 0.9f; return true; // 1x
        case 202:  out = 1.1f; return true; // 2x
        case 203:  out = 1.3f; return true; // 3x
        case 1334: out = 1.6f; return true; // 4x
        default:   return false;
    }
}

// Gamemode / size portals: the prediction stops at them (1.24.0: a click right
// before one is held until you're through, then lined up).
bool isModeOrSizePortal(GameObjectType t) {
    switch (t) {
        case GameObjectType::CubePortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::BallPortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::RegularSizePortal:
            return true;
        default:
            return false;
    }
}

bool isGravityPortal(GameObjectType t) {
    return t == GameObjectType::NormalGravityPortal || t == GameObjectType::InverseGravityPortal ||
           t == GameObjectType::GravityTogglePortal;
}

// Readable name for the log ("yellow orb", "size portal (mini)", ...).
char const* objectName(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:        return "yellow pad";
        case GameObjectType::PinkJumpPad:          return "pink pad";
        case GameObjectType::RedJumpPad:           return "red pad";
        case GameObjectType::GravityPad:           return "gravity pad";
        case GameObjectType::SpiderPad:            return "spider pad";
        case GameObjectType::YellowJumpRing:       return "yellow orb";
        case GameObjectType::PinkJumpRing:         return "pink orb";
        case GameObjectType::RedJumpRing:          return "red orb";
        case GameObjectType::GravityRing:          return "blue orb";
        case GameObjectType::GreenRing:            return "green orb";
        case GameObjectType::DropRing:             return "black orb";
        case GameObjectType::CustomRing:           return "custom orb";
        case GameObjectType::DashRing:             return "dash orb";
        case GameObjectType::GravityDashRing:      return "gravity dash orb";
        case GameObjectType::SpiderOrb:            return "spider orb";
        case GameObjectType::TeleportOrb:          return "teleport orb";
        case GameObjectType::InverseGravityPortal: return "gravity portal (up)";
        case GameObjectType::NormalGravityPortal:  return "gravity portal (down)";
        case GameObjectType::GravityTogglePortal:  return "gravity toggle";
        case GameObjectType::ShipPortal:           return "ship portal";
        case GameObjectType::CubePortal:           return "cube portal";
        case GameObjectType::BallPortal:           return "ball portal";
        case GameObjectType::UfoPortal:            return "ufo portal";
        case GameObjectType::WavePortal:           return "wave portal";
        case GameObjectType::RobotPortal:          return "robot portal";
        case GameObjectType::SpiderPortal:         return "spider portal";
        case GameObjectType::SwingPortal:          return "swing portal";
        case GameObjectType::InverseMirrorPortal:
        case GameObjectType::NormalMirrorPortal:   return "mirror portal";
        case GameObjectType::RegularSizePortal:    return "size portal (normal)";
        case GameObjectType::MiniSizePortal:       return "size portal (mini)";
        case GameObjectType::DualPortal:           return "dual portal";
        case GameObjectType::SoloPortal:           return "solo portal";
        case GameObjectType::TeleportPortal:       return "teleport portal";
        case GameObjectType::Modifier:             return "speed portal/trigger";
        case GameObjectType::Hazard:               return "spike";
        case GameObjectType::AnimatedHazard:       return "saw";
        case GameObjectType::Solid:                return "block";
        case GameObjectType::Slope:                return "slope";
        default:                                   return "object";
    }
}

// Used only if an object reports a nonsensical hitbox.
struct Extent { float hw, hh; };
Extent fallbackExtent(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::SpiderPad:
            return {17.f, 8.f};
        default:
            break;
    }
    if (isOrb(t)) return {18.f, 18.f};
    return {16.f, 46.f}; // portals, speed changers, modifiers
}

// What cut a lookahead short.
struct TruncInfo {
    bool           valid = false;
    GameObjectType type  = GameObjectType::Decoration;
    float          gap   = 0.f; // px between the player's leading edge and the object (<= 0: touching)
};

std::string describeTrunc(TruncInfo const& t) {
    if (!t.valid) return "orb/portal ahead";
    if (t.gap > 0.5f) return fmt::format("{} {:.0f}px ahead", objectName(t.type), t.gap);
    return fmt::format("{} (touching)", objectName(t.type));
}

struct SimResult {
    int       ticks = 0;     // ticks survived, counted from the moment the input lands
    SimEnd    end   = SimEnd::Survived;
    TruncInfo trunc;
    int       landTick = -1; // cube/ball/spider/robot: first tick back on the ground after the jump
    // Closest the path came to a hazard (px) during the part the click is
    // judged on: until the landing (+ grace), or the first 100 ms in ship /
    // wave / UFO / swing. 1e9 = no hazard nearby.
    float     clear = 1e9f;
    bool      mover = false;  // a moving hazard was tested at its predicted position
    int       killType = -1;  // what killed it (GameObjectType), -1 unknown
    bool      killMover = false; // killed by a moving hazard at its predicted position
    float     killDy = 0.f;   // height at death relative to the start (px)
    bool      orbUsed = false; // the test used a predicted orb
    RingObject* firstRing = nullptr; // first unused orb you touch in this test (GD's contact)
    int       firstRingTick = -1;    // ...on this step (from the start, incl. the delay)
    int       firstRingLast = -1;    // last step of that contact (in a row)
    int       nextRingTick = -1;     // first step you overlap ANOTHER orb after using one
    int       deathStep = -1;  // step (from the start, incl. the delay) it died on
};
// Extra inputs injected into a test at fixed steps (-1 = none).
struct FollowUp {
    int  at[2]   = {-1, -1};
    bool held[2] = {false, false};
};

// Per sim player: where it started and how it moves, so objects can be judged
// "ahead" or "behind" and "already touched" or "not yet touched".
struct SimTrack {
    float startX = 0.f;
    float dir    = 1.f; // +1 moving right, -1 moving left
    float stepPx = 0.f; // distance covered per tick
};

// ----------------------------------------------------------------------------
//  4. Decisions, death reasons, stats
// ----------------------------------------------------------------------------
enum class Decision : uint8_t {
    None,
    PassSafe,       // frame 0 survives the horizon
    PassTruncated,  // frame 0 reached an orb / pad / portal (can't predict)
    PassSpam,       // spam guard bypass
    PassRelease,    // release in a mode where release doesn't matter
    PassNoWindow,   // window is shorter than one physics tick
    PassBudget,     // out of CPU budget, fell back to raw input
    PassNoFix,      // frame 0 predicted dead, no delay in the window clears it
    PassEscapable, // frame 0 dies only if you never input again: you can still save it
    Buffered,       // input delayed
    Queued,         // opposite input queued behind a pending buffer
    PassModeOff,    // this gamemode is switched off in the Modes tab
    Count
};

char const* decisionName(Decision d) {
    switch (d) {
        case Decision::None:          return "none";
        case Decision::PassSafe:      return "pass_safe";
        case Decision::PassTruncated: return "pass_truncated";
        case Decision::PassSpam:      return "pass_spam";
        case Decision::PassRelease:   return "pass_release";
        case Decision::PassNoWindow:  return "pass_nowindow";
        case Decision::PassBudget:    return "pass_budget";
        case Decision::PassNoFix:     return "pass_nofix";
        case Decision::PassEscapable: return "pass_escape";
        case Decision::Buffered:      return "buffered";
        case Decision::Queued:        return "queued";
        case Decision::PassModeOff:   return "pass_modeoff";
        default:                      return "?";
    }
}

enum class DeathReason : uint8_t {
    NoRecentClick,    // died with no click in the horizon
    SimMispredict,    // sim said safe, real died inside the horizon
    LaterObstacle,    // died after the horizon
    Truncated,        // orb / pad / portal blocked the prediction
    SpamBypass,       // spam guard stepped aside
    WindowTooSmall,   // sim saw the death, window couldn't fix it
    OutOfBudget,      // CPU budget fallback
    BufferedStillDied,// delayed click still died
    EscapeNotTaken, // click was fine if followed up, the follow-up didn't happen in time
    Other,
    Count
};

char const* deathName(DeathReason r) {
    switch (r) {
        case DeathReason::NoRecentClick:     return "no_recent_click";
        case DeathReason::SimMispredict:     return "sim_mispredict";
        case DeathReason::LaterObstacle:     return "later_obstacle";
        case DeathReason::Truncated:         return "truncated";
        case DeathReason::SpamBypass:        return "spam_bypass";
        case DeathReason::WindowTooSmall:    return "window_too_small";
        case DeathReason::OutOfBudget:       return "out_of_budget";
        case DeathReason::BufferedStillDied: return "buffered_still_died";
        case DeathReason::EscapeNotTaken:    return "escape_not_taken";
        case DeathReason::Other:             return "other";
        default:                             return "?";
    }
}

// Short human text for the on-screen log.
char const* deathText(DeathReason r) {
    switch (r) {
        case DeathReason::NoRecentClick:     return "no recent click";
        case DeathReason::SimMispredict:     return "sim said safe (mispredict)";
        case DeathReason::LaterObstacle:     return "obstacle past lookahead";
        case DeathReason::Truncated:         return "orb/pad/portal cut prediction";
        case DeathReason::SpamBypass:        return "spam guard bypassed buffer";
        case DeathReason::WindowTooSmall:    return "window too small";
        case DeathReason::OutOfBudget:       return "CPU budget fallback";
        case DeathReason::BufferedStillDied: return "buffered click still died";
        case DeathReason::EscapeNotTaken:    return "needed a follow-up input";
        case DeathReason::Other:             return "other";
        default:                             return "?";
    }
}

struct Stats {
    int byDecision[static_cast<int>(Decision::Count)] = {};
    int byDeath[static_cast<int>(DeathReason::Count)] = {};
    int clicks = 0;
    int deaths = 0;
    int simClicks = 0;
    long long sumUs = 0;
    long long maxUs = 0;
};

struct ClickRecord {
    int       needed   = -1; // no-fix probe: ticks the click needed (-1 not probed, 0 not found)
    int       probe    = 0;  // how far the probe looked
    int       window   = 0;  // window in ticks at the time
    Decision  decision = Decision::None;
    SimEnd    f0End    = SimEnd::Survived;
    int       f0Ticks  = 0;
    int       delay    = 0;
    int       horizon  = 0;
    double    time     = -1.0; // game time (60fps frames) when the click arrived
    TruncInfo trunc;
    int       liveTicks = -1;   // buffered: when it really fired (ticks after the click), -1 not yet
    ibcore::FireReason liveReason = ibcore::FireReason::None;
};

// Everything the logs need to know about one click.
struct ClickNote {
    bool      push        = true;
    Decision  d           = Decision::None;
    int       windowTicks = 0;
    int       horizon     = 0;
    long long us          = 0;
    SimResult r0;
    int       delay       = 0;
    int       evals       = 0; // delays simulated by the search
    int       needed      = -1; // no-fix probe: -1 not run, 0 nothing helps, >0 ticks needed
    std::string modeName;       // for the log
    bool      orb         = false; // the chosen path uses a predicted orb
    bool      stream      = false; // wave spam: kept the stream's steady shift
    bool      rescue      = false; // no safe moment found by the quick search: held, checked every tick
    bool      portal      = false; // held until past a gamemode / size portal (its own log line)
};

// ----------------------------------------------------------------------------
//  5. CSV log (bounded size, buffered writes, only touched when enabled)
// ----------------------------------------------------------------------------
class CsvLog {
    std::ofstream m_out;
    int m_pending = 0;

    void openIfNeeded() {
        if (m_out.is_open()) return;
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path path = Mod::get()->getSaveDir() / "buffer-log.csv";
        bool fresh = true;
        if (fs::exists(path, ec)) {
            auto size = fs::file_size(path, ec);
            if (!ec && size > 0 && size < MAX_LOG_BYTES) fresh = false;
        }
        m_out.open(path, fresh ? std::ios::trunc : std::ios::app);
        if (m_out.is_open() && fresh) {
            m_out << "event,t_ms,mode,input,tps,window_ticks,horizon,sim_us,f0_end,f0_ticks,decision,delay,extra\n";
        }
    }

public:
    void row(std::string const& line) {
        openIfNeeded();
        if (!m_out.is_open()) return;
        m_out << line << '\n';
        if (++m_pending >= 32) flush();
    }

    void flush() {
        if (m_out.is_open()) m_out.flush();
        m_pending = 0;
    }

    void close() {
        if (m_out.is_open()) {
            m_out.flush();
            m_out.close();
        }
        m_pending = 0;
    }
};

// ----------------------------------------------------------------------------
//  6. Colors shared by the window and the log.
//     The window itself (section 9) is defined after the engine; the engine
//     only asks it to exist via windowEnsure().
// ----------------------------------------------------------------------------
const ccColor3B COL_WHITE  = {235, 235, 235};
const ccColor3B COL_GREY   = {165, 165, 165};
const ccColor3B COL_GREEN  = {110, 255, 130};
const ccColor3B COL_YELLOW = {255, 225, 90};
const ccColor3B COL_ORANGE = {255, 160, 60};
const ccColor3B COL_RED    = {255, 95, 95};
const ccColor3B COL_CYAN   = {110, 220, 255};
const ccColor3B COL_LAV    = {198, 166, 255}; // section titles (window theme)
const ccColor3B COL_DIM    = {120, 110, 160};

// Log tags (1.26.1): every Log line is "time  TAG  text"; the tag's colour says
// what kind of line it is at a glance.
ccColor3B logTagColor(std::string const& tag) {
    if (tag == "DEATH")  return {255, 95, 95};
    if (tag == "BUFFER") return {110, 255, 130};
    if (tag == "FIRED")  return {80, 220, 170};
    if (tag == "QUEUED") return {110, 200, 255};
    if (tag == "RAW")    return {255, 170, 80};
    if (tag == "HOLD")   return {110, 200, 255};
    if (tag == "OK")     return {150, 150, 170};
    if (tag == "CHECK")  return {255, 225, 90};
    return {198, 166, 255}; // NOTE
}

void windowEnsure();   // defined with BufferWindow (section 9)

// ----------------------------------------------------------------------------
//  7. TrajectoryBufferSim - the engine
//
//  Flow of one click (interceptInput):
//     read live gamemode  ->  spam guard  ->  simulate "click now" (frame 0)
//       safe?            pass the click through untouched
//       dies?            simulate the same click delayed by 1..window ticks,
//                        keep the smallest delay that survives, hold the click
//                        and re-fire it from onPlayerUpdate when it is due
//       cut by an orb /  pass the click through (we cannot see past it) -
//       pad / portal?    the log says exactly which object did it
// ----------------------------------------------------------------------------
class TrajectoryBufferSim {
    Ref<PlayerObject> m_simPlayer1;
    Ref<PlayerObject> m_simPlayer2;
    gd::vector<GameObject*> m_scratch; // reused every sim step (no per-step allocation)

    // Clearance + moving hazards (v1.11)
    float  m_tickClr  = 1e9f;   // closest hazard in the current sim step (px)
    bool   m_simMover = false;  // this test used a moving hazard's predicted position
    bool   m_simModern = false; // this test measures near-miss clearance (current engine)
    bool   m_simMovers = false; // this test predicts moving hazards / orbs
    int    m_simKillType = -1;  // what killed the sim player (object type, -1 unknown)
    bool   m_simKillMover = false; // ... a moving hazard at its predicted position
    float  m_simStartY = 0.f, m_simKillDy = 0.f;
    double m_simNow   = 0.0;    // game time the test starts at
    float  m_simDtCur = BASE_DT;// step length of the current test
    std::unordered_map<GameObject*, ibcore::MoverTrack> m_movers; // hazards seen near you (never dereferenced from here)
    long long m_moverTests  = 0; // tests that used a moving hazard
    long long m_moverDeaths = 0; // ... and died to one
    long long m_cleanWaits  = 0; // live: waited for a cleaner moment

    // Spot memory (v1.12): where inputs landed on earlier attempts of this level
    ibcore::SpotMemory     m_spots;
    ibcore::OutcomeTracker m_outcome;
    double m_outcomeWindow = SPOT_WINDOW_HOLD;
    bool   m_outcomeLand   = false; // judged on landing (cube-type)
    bool   m_outcomeAir    = false; // left the ground since the input
    double m_outcomeLandAt = -1.0;
    float  m_p1Speed = 0.f, m_p1Dir = 1.f; // real P1 horizontal speed (px / frame), direction
    float  m_p1LastX = 0.f;
    double m_p1LastT = -1.0;
    long long m_spotFires = 0, m_spotBlocks = 0, m_spotSaves = 0;
    std::filesystem::path m_spotFile;  // <save dir>/spots/<level>.txt ("" = not saved)
    std::string m_spotLevelName;
    bool   m_spotsDirty  = false;
    size_t m_spotsLoaded = 0;          // spots that came from earlier sessions
    bool      m_liveWaited  = false;
    int       m_liveWaitWhy = 0; // 1 near miss, 2 waiting for a learned spot
    // Why the live check said "not now" for the held click (diagnostics, per click)
    enum Refuse { RF_DIED, RF_CUT, RF_SPOT, RF_CLEAN, RF_AHEAD, RF_COUNT };
    int       m_refuse[RF_COUNT] = {};
    SimResult m_refuseFirst;      // the first "died" test: how / when it died
    bool      m_refuseHave = false;

    bool m_simulating     = false;
    bool m_firingBuffered = false;
    bool m_simDead        = false;
    TruncInfo m_simTrunc;       // set when the player is about to touch something we can't model
    bool  m_recordPath  = false;         // keep the P1 path of each test (accuracy / overlay)
    std::vector<ibcore::Point2> m_path;  // path of the last test
    bool  m_simHeld     = false; // button state inside the current test
    int   m_simTick     = 0; // step index inside the current test
    int   m_simDelay    = 0; // step at which the tested input lands
    bool  m_orbPredict  = false; // this click may model velocity orbs
    int   m_orbIntent   = 0; // steps after the input in which an orb counts as its target
    bool  m_simOrbUsed  = false; // this test already used an orb
    int   m_orbClick    = 3;     // UFO / swing: steps after a press that still count as a click on an orb
    int   m_simPressTick = -1;   // step of the last press inside the current test (-1 none)
    RingObject* m_tickRing[2] = {nullptr, nullptr}; // orb GD says each sim player touches this step
    RingObject* m_prevRing[2] = {nullptr, nullptr}; // orb each sim player touched last step (or the real one now)
    bool  m_gdRings = false;     // GD reports orb contact for sim players (exact orbs on)
    GameObject* m_simOrbObj = nullptr; // ... this one (ignored for the rest of the test)
    std::vector<std::pair<GameObject*, bool>> m_objRestore; // activation flags to put back
    std::vector<GameObject*> m_simUsed;  // pads / portals already applied in this test
    bool  m_padPredict  = false;         // this click may model pads and speed / gravity portals

    // Pad strength as GD passes it to bumpPlayer, learned per pad type from real
    // hits. A type whose strength varied between hits is not predicted.
    struct PadLearn { bool seen = false; bool unstable = false; float bump = 0.f; };
    PadLearn m_pads[64];
    bool     m_padsDirty = false;
    SimTrack  m_track[2];       // [0] = sim player 1, [1] = sim player 2

    // Real physical button state vs what the game currently has.
    bool m_physicalHolding = false;
    bool m_gameHolding     = false;
    bool m_enabled         = true;

    // ---- Game clock --------------------------------------------------------
    // Everything below is scheduled in GAME TIME (60fps-frame units, time warp
    // removed), not in update calls. Click Between Frames splits a frame into
    // extra, shorter physics steps when a click lands mid-frame, so counting
    // update calls would make a "3 tick" delay fire early. Time can't drift.
    double m_gameTime = 0.0;
    // Buffer state: the held input and everything that arrives while it is
    // held, replayed in order with the same shift (see BufferCore.hpp).
    ibcore::InputQueue m_queue;

    // Live re-planning: while an input is held back, every tick tests "fire it
    // now?" from the REAL game state and fires at the first safe tick, instead
    // of trusting a delay predicted at click time (long Extreme delays drift).
    // Every input queued behind it gets the same check when it is next.
    bool   m_live = false;          // the live re-planner owns the queue head
    ibcore::LiveState m_liveState;  // see BufferCore.hpp (tested there)
    double m_liveArrive = 0.0;      // when the triggering click arrived
    int    m_livePlanTicks = 0;     // planned delay, ticks (for the log)

    // Nominal tick length. CBF sub-steps are shorter, so the nominal tick is the
    // largest of the last few. m_warp is the level's time warp (sim steps use dt*warp).
    float m_frameDt = BASE_DT; // 60fps-frame units per physics tick
    float m_warp    = 1.0f;
    float m_dtRing[DT_RING] = {};
    int   m_dtIdx = 0;

    // Frame-time meter (wall clock between rendered frames) for the auto budget.
    Clock::time_point m_lastFrameAt{};
    bool  m_haveFrame = false;
    float m_frameUs   = 1e6f / 60.f; // smoothed
    int   m_lastBudgetUs = 0;        // what the last click was allowed (for the window)
    bool  m_cbfLoaded = false;

    // Live accuracy: prediction vs reality. A buffered click's check starts
    // when it actually fires, so it is aligned to the real step.
    ibcore::PredictionTracker m_tracker;
    bool  m_trackPending = false;
    std::vector<ibcore::Point2> m_pendingPath;
    int   m_pendingDelay = 0;
    int   m_pendingCheck = 0;

    // Auto windows (kept across sessions).
    ibcore::Calibrator m_calib[W_COUNT];

    // Input rhythm per mode (this session): how long you hold, and how long you
    // stay released before pressing again. Feeds the cube tap model and the
    // ship / wave "could you still save it" cycle.
    ibcore::Calibrator m_holdLen[W_COUNT];
    ibcore::Calibrator m_gapLen[W_COUNT];
    double m_lastPressAt   = -1.0;
    double m_lastReleaseAt = -1.0;

    // Accuracy per mode name ("cube", "mini wave", ...).
    struct AccCount { int checks = 0; int hits = 0; int deaths = 0; double errSum = 0.0; double dxSum = 0.0, dySum = 0.0; };
    std::map<std::string, AccCount> m_accByMode;
    std::string m_trackMode;        // mode of the check in progress
    std::string m_pendingMode;      // mode of a buffered check waiting to start
    float m_autoW[W_COUNT] = {};            // auto window per mode (0 = still learning)
    bool  m_calibDirty = false;

    // Trajectory overlay and badge
    Ref<CCDrawNode> m_overlay;
    bool  m_overlayShown = false;
    Ref<CCLabelBMFont> m_badge;
    std::string m_badgeText;
    bool  m_active = true;                  // enabled and allowed here (practice-only)

    bool m_deathReported = false;  // this death already counted (until respawn)
    int  m_directFires = 0;        // buffered inputs another mod held back (applied directly)
    bool m_directMode = false;     // session-wide: apply buffered inputs straight to the player

    // Last input treated as yours (to tell GD's player-2 mirror apart).
    long long m_lastPrimaryTick = -1;
    bool      m_lastPrimaryPush = false;

    // Spam guard
    double m_lastInputTime = -1e9;
    // Performance (1.18.0): live re-checks
    double    m_lastLiveAt = -1e9;
    Clock::time_point m_lastStepWall{};      // last physics-step hook call (wall clock)
    Clock::time_point m_pendingSinceWall{};  // when the current held input started waiting
    long long m_watchdogFires = 0;
    long long m_simStepsTotal = 0;   // sim steps run (for us-per-step in Stats)
    long long m_cheapClicks = 0;     // clicks finished with cheap checks (out of time)
    long long m_inputsSeen = 0;      // inputs that reached the buffer this level
    // One-tick self-check (1.23.0): predicted vs real state after one tick.
    struct OneStep { int n = 0, exact = 0; double sdx = 0, sdy = 0, sdv = 0; float worst = 0.f; bool logged = false; };
    std::map<std::string, OneStep> m_oneStep;
    bool   m_osPending = false, m_osInput = false;
    // Orb press check (1.24.3): how a UFO / swing press on an orb plays out in
    // GD isn't certain, so the three candidate models are tested against what
    // really happens on your own orb presses, and the closest one is used.
    //   0: orb on the press tick, before the step, no flap      (1.24.1)
    //   1: flap step first, orb right after it on the same tick  (<= 1.24.0)
    //   2: press (flap state) + orb before the step
    static constexpr int ORB_MODELS = 3;
    int    m_orbModel = 1; // GD's order (1.24.7): ties go here
    bool   m_orbModelLoaded = false;
    ibcore::PredictionTracker m_orbTr[ORB_MODELS];
    bool   m_orbCalArmed = false;   // trackers started for a raw press
    bool   m_orbCalReal = false;    // the real player used an orb since that press
    std::vector<ibcore::Point2> m_orbCalPath[ORB_MODELS];
    double m_orbCalT0 = 0.0;
    bool   m_orbCalReady = false;   // paths computed for the click being judged
    int    m_orbCalN = 0;
    double m_orbCalErr[ORB_MODELS] = {0.0, 0.0, 0.0};
    // Orb jump measurement (1.24.5): what GD really does when you use an orb vs
    // what the prediction did, and a learned correction of the orb's y-speed.
    int    m_simOrbStep = -1;       // step the sim used its orb on (this test)
    double m_simOrbVy = 0.0, m_simOrbVyBefore = 0.0;
    int    m_calOrbStep[ORB_MODELS] = {-1, -1, -1};
    double m_calOrbVy[ORB_MODELS] = {0, 0, 0};
    double m_realOrbT = -1.0, m_realOrbVy = 0.0, m_realOrbVyBefore = 0.0;
    int    m_realOrbCalls = 0, m_realOrbType = -1;
    bool   m_realOrbMini = false;
    // What each orb really does (1.24.6), learned from your own orb hits and
    // saved: the y-speed right after it and whether it flipped gravity. The
    // sim players' own orb call turned out to do nothing (pink and green orbs
    // both predicted 6.87 = a plain flap; the game gave 4.46 and -10.62).
    struct OrbFact { double vySum = 0.0; int n = 0; int flips = 0; };
    std::map<int, OrbFact> m_orbTable;
    bool   m_orbTableDirty = false;
    int    m_orbTableUses = 0; // predictions that used a learned orb
    float  m_osX = 0.f, m_osY = 0.f;
    double m_osVy = 0.0;
    std::string m_osMode;
    float  m_lastStepD = 0.f;        // length of the last real P1 step (60fps-frame units)
    float  m_stepAccum = 0.f;        // time into the current physics tick (60fps-frame units)
    float  m_phase = 0.f;            // ... as a fraction of a tick (0 = on a tick boundary)
    bool      m_liveRescue = false;  // the held click is a rescue (checked every tick, no plan)
    bool      m_liveSpotHold = false; // the held click waits for a learned spot (no prediction)
    // Follow the plan (1.26.1): a held click fires on the click-time plan while
    // you stay on the path the plan predicted - checked by position, no
    // prediction per tick. Off the path: the full per-tick check takes over.
    bool      m_planTrust = false;
    std::vector<ibcore::Point2> m_planPath; // predicted position after each step from the click
    double    m_planGrid = 0.0;             // tick grid start of that click
    long long m_planFires = 0, m_planDrifts = 0, m_planTicksSaved = 0;
    // Orbs the real player used this attempt (1.24.9). GD's m_isActivated is set
    // as soon as you TOUCH an orb, so it can't tell which orbs are really used.
    std::vector<GameObject*> m_realUsedOrbs;
    RingObject* m_simFirstRing = nullptr;
    int       m_simFirstRingTick = -1;
    int       m_simFirstRingLast = -1;
    int       m_simNextRingTick = -1;
    float     m_spotTargetX = 0.f, m_spotTargetDir = 1.f; // where that spot is (level x) and travel direction
    long long m_hookMissing = 0;     // ... passed raw because the physics hook wasn't running
    long long m_simUsTotal = 0;
    long long m_liveUsSum = 0, m_liveChecks = 0, m_liveUsMax = 0;
    // Wave spam ("stream"): one steady shift for the whole stream (1.16.0)
    double m_streamLast  = -1e9; // last wave input (game time)
    double m_streamShift = 0.0;  // shift of the stream's inputs so far (game time)
    int    m_streak        = 0;

    // CPU budget controller
    float m_horizonScale = 1.0f;
    bool  m_hasDeadline  = false;
    Clock::time_point m_deadline;

    // Config cache (re-read at most every 150 ms instead of on every click)
    Config m_cfg;
    Clock::time_point m_cfgAt{};
    bool m_cfgValid = false;

    // Logging / diagnostics
    bool m_logDebug = false;
    bool m_logCsv   = false;
    CsvLog m_csv;

    // Full log for the window's Log tab.
    struct HistEntry {
        std::string tag;    // BUFFER / FIRED / DEATH / RAW / ... (see logTagColor)
        std::string text;
        ccColor3B   color;
        float       t;      // seconds since level start
        int         repeat;
        bool        minor;  // safe pass / irrelevant release (hidden unless "show safe clicks")
    };
    static constexpr size_t MAX_HISTORY = 300;
    std::deque<HistEntry> m_history;
    Stats m_stats;
    ClickRecord m_last;
    std::string m_lastMode;
    long long m_tick = 0;          // update-call counter (death de-dupe, housekeeping)
    long long m_lastDeathTick = -1;
    Clock::time_point m_levelStart = Clock::now();

    // ------------------------------------------------------------------------
    //  Config cache
    // ------------------------------------------------------------------------
    Config const& cfg() {
        auto const now = Clock::now();
        if (!m_cfgValid || now - m_cfgAt > std::chrono::milliseconds(150)) {
            m_cfg = Config::load();
            m_cfgAt = now;
            m_cfgValid = true;
        }
        return m_cfg;
    }

    void invalidateCfg() { m_cfgValid = false; }

    // Enabled, and allowed in this attempt (practice-only mode).
    static bool allowedHere(Config const& c, PlayLayer* pl) {
        if (!c.enabled) return false;
        if (c.practiceOnly && !(pl && pl->m_isPracticeMode)) return false;
        return true;
    }

    void refreshFlags() {
        Config const& c = cfg();
        m_active   = allowedHere(c, PlayLayer::get());
        m_enabled  = m_active;
        m_logDebug = c.debug;
        m_logCsv   = c.csv;
    }

    // Settings with the auto windows applied (when on and learned).
    Config effectiveCfg(Config const& c) const {
        Config e = c;
        if (c.autoWindow) {
            for (int i = 0; i < W_COUNT; i++) if (m_autoW[i] > 0.f) e.window[i] = m_autoW[i];
        }
        return e;
    }

    // ------------------------------------------------------------------------
    //  Auto windows
    // ------------------------------------------------------------------------
    void refreshAuto(Config const& c, int w) {
        auto win = m_calib[w].window(AUTO_MIN_SAMPLES, AUTO_PERCENTILE, AUTO_MARGIN_FRAMES,
                                     AUTO_MIN_FRAMES, std::max(AUTO_MIN_FRAMES, c.autoWindowMax));
        m_autoW[w] = win.value_or(0.f);
    }

    // Median of a rhythm sample set, or a default until there are enough.
    static float rhythm(ibcore::Calibrator const& c, float def) {
        return c.count() >= RHYTHM_MIN_SAMPLES ? c.percentile(0.5f) : def;
    }

    // A buffered click needed `frames` (60fps frames) of delay to survive.
    void recordSave(Config const& c, int w, float frames) {
        m_calib[w].add(frames);
        m_calibDirty = true;
        refreshAuto(c, w);
    }

    // A click was too early even for the window (frames < 0: probe found nothing).
    void recordMiss(Config const& c, int w, float frames) {
        if (frames > 0.f) {
            m_calib[w].add(frames);
            m_calibDirty = true;
            refreshAuto(c, w);
        }
    }

    // Calibration + learned pad strengths live in the mod's saved values.
    void loadPersisted(Config const& c) {
        auto* mod = Mod::get();
        if (c.calibReset) {
            for (int i = 0; i < W_COUNT; i++) {
                m_calib[i].clear();
                mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], "");
            }
            invalidateCfg();
            log::info("[InputBuffer] calibration data cleared");
        } else {
            for (int i = 0; i < W_COUNT; i++) {
                m_calib[i].deserialize(mod->getSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], ""));
            }
        }
        for (int i = 0; i < W_COUNT; i++) refreshAuto(c, i);

        // pads: "type:bump:unstable;..."
        for (auto& pd : m_pads) pd = PadLearn{};
        std::string const pads = mod->getSavedValue<std::string>("pad-strengths", "");
        size_t pos = 0;
        while (pos < pads.size()) {
            size_t const end = pads.find(';', pos);
            std::string const tok = pads.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            // "type:bump:unstable" (parsed by hand: sscanf is deprecated on MSVC)
            char const* p = tok.c_str();
            char* q = nullptr;
            long const type = std::strtol(p, &q, 10);
            if (q && *q == ':') {
                float const bump = std::strtof(q + 1, &q);
                if (q && *q == ':') {
                    long const unstable = std::strtol(q + 1, &q, 10);
                    if (type >= 0 && type < 64) m_pads[type] = PadLearn{true, unstable != 0, bump};
                }
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        m_calibDirty = false;
        m_padsDirty = false;
    }

    void savePersisted() {
        auto* mod = Mod::get();
        if (m_calibDirty) {
            for (int i = 0; i < W_COUNT; i++) {
                mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], m_calib[i].serialize());
            }
            m_calibDirty = false;
        }
        if (m_padsDirty) {
            std::string out;
            for (int t = 0; t < 64; t++) {
                if (!m_pads[t].seen) continue;
                out += fmt::format("{}:{:.4f}:{};", t, m_pads[t].bump, m_pads[t].unstable ? 1 : 0);
            }
            mod->setSavedValue<std::string>("pad-strengths", out);
            m_padsDirty = false;
        }
    }

    // ------------------------------------------------------------------------
    //  Trajectory overlay + badge
    // ------------------------------------------------------------------------
    // ---- Show paths: live preview ---------------------------------------------
    // The path your NEXT input would take if made right now (press if you're
    // not holding, release if you are), redrawn continuously: a line along the
    // path plus the player hitbox (red outer, blue inner) at the start and at
    // the predicted end. Green = safe, red = dies, yellow = cut short by an
    // orb / portal the prediction can't see past. While a click is held back,
    // it shows the "fire now?" path the live re-planner is testing.
    static ccColor4F previewColor(SimEnd e) {
        switch (e) {
            case SimEnd::Died:      return ccColor4F{1.f, 0.3f, 0.3f, 0.9f};
            case SimEnd::Truncated: return ccColor4F{1.f, 0.85f, 0.3f, 0.9f};
            default:                return ccColor4F{0.2f, 0.85f, 0.25f, 0.9f};
        }
    }

    bool ensureOverlay(PlayLayer* pl) {
        if (!pl || !pl->m_objectLayer) return false;
        if (!m_overlay.data()) {
            m_overlay = CCDrawNode::create();
            if (!m_overlay.data()) return false;
        }
        if (!m_overlay->getParent()) pl->m_objectLayer->addChild(m_overlay.data(), 9999);
        return true;
    }

    void drawHitbox(CCPoint c, float rotDeg, float size, ccColor4F col, float width) {
        float const h = size * 0.5f;
        float const r = -rotDeg * 3.14159265f / 180.f;
        float const cs = std::cos(r), sn = std::sin(r);
        CCPoint pts[4];
        float const corner[4][2] = {{-h, -h}, {h, -h}, {h, h}, {-h, h}};
        for (int i = 0; i < 4; i++) {
            pts[i] = CCPoint{c.x + corner[i][0] * cs - corner[i][1] * sn,
                             c.y + corner[i][0] * sn + corner[i][1] * cs};
        }
        m_overlay->drawPolygon(pts, 4, ccColor4F{0.f, 0.f, 0.f, 0.f}, width, col);
    }

    void drawPlayerBox(CCPoint c, float rotDeg, float size) {
        drawHitbox(c, rotDeg, size, ccColor4F{1.f, 0.2f, 0.2f, 0.95f}, 1.0f);         // outer (red)
        drawHitbox(c, rotDeg, size * 0.36f, ccColor4F{0.2f, 0.35f, 1.f, 0.95f}, 1.0f); // inner (blue)
    }

    void drawPreview(PlayLayer* pl, std::vector<ibcore::Point2> const& path, ccColor4F lineCol) {
        if (path.size() < 2 || !ensureOverlay(pl)) return;
        PlayerObject* p1 = pl->m_player1;
        m_overlay->clear();
        size_t const step = path.size() > 200 ? 2 : 1;
        CCPoint prev = p1 ? p1->getPosition() : CCPoint{path[0].x, path[0].y};
        for (size_t i = 0; i < path.size(); i += step) {
            CCPoint const cur{path[i].x, path[i].y};
            m_overlay->drawSegment(prev, cur, 1.1f, lineCol);
            prev = cur;
        }
        float const size = 30.f * std::clamp(p1 ? p1->m_vehicleSize : 1.f, 0.3f, 1.3f);
        if (p1) drawPlayerBox(p1->getPosition(), p1->getRotation(), size);
        drawPlayerBox(CCPoint{path.back().x, path.back().y}, path.back().rot, size);
        m_overlay->setVisible(true);
        m_overlayShown = true;
    }

    void clearPreview() {
        if (m_overlayShown && m_overlay.data()) m_overlay->clear();
        m_overlayShown = false;
    }

    // Idle preview (no click held): "your next input, right now".
    void updatePreview(PlayLayer* pl) {
        Config const& c = cfg();
        if (!c.overlay || !m_active) { clearPreview(); return; }
        if (m_live && m_queue.pending()) return; // liveStep draws the held click's path
        bool const next = !m_physicalHolding;
        Ctx x;
        if (!makeCtx(pl, next, c, x)) { clearPreview(); return; }
        int const budget = clickBudgetUs(c) / 3; // display only: never crowd out real clicks
        m_hasDeadline = budget > 0;
        if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
        m_recordPath = true;
        SimResult const r = runSim(x, 0);
        m_hasDeadline = false;
        if (r.end == SimEnd::OutOfBudget) return;
        drawPreview(pl, m_path, previewColor(r.end));
    }

    void tickOverlay() {
        if (!cfg().overlay) clearPreview();
    }

    void releaseOverlay() {
        if (m_overlay.data() && m_overlay->getParent()) m_overlay->removeFromParent();
        m_overlay.swap(nullptr);
        m_overlayShown = false;
    }

    // Small tag bottom-left while the buffer is active (fair play / recordings).
    void updateBadge(PlayLayer* pl, Config const& c) {
        bool const show = c.showBadge && m_active;
        if (!show) {
            if (m_badge.data()) m_badge->setVisible(false);
            return;
        }
        if (!m_badge.data() || !m_badge->getParent()) {
            auto* l = CCLabelBMFont::create("", "chatFont.fnt");
            if (!l) return;
            l->setAnchorPoint({0.f, 0.f});
            l->setScale(0.45f);
            l->setOpacity(150);
            l->setPosition({4.f, 3.f});
            pl->addChild(l, 2000);
            m_badge = l;
            m_badgeText.clear();
        }
        std::string text = c.custom ? "Input Buffer: CUSTOM" : c.extreme ? "Input Buffer: EXTREME" : "Input Buffer";
        if (c.practiceOnly) text += " (practice)";
        if (text != m_badgeText) {
            m_badge->setString(text.c_str());
            m_badgeText = text;
        }
        m_badge->setVisible(true);
    }

    void releaseBadge() {
        if (m_badge.data() && m_badge->getParent()) m_badge->removeFromParent();
        m_badge.swap(nullptr);
        m_badgeText.clear();
    }

    // ------------------------------------------------------------------------
    //  Sim players (created lazily, P2 only when dual is actually used)
    // ------------------------------------------------------------------------
    PlayerObject* createSimPlayer(PlayLayer* pl) {
        PlayerObject* p = PlayerObject::create(1, 1, pl, pl, true);
        if (!p) return nullptr;
        p->setPosition({0, 105});
        p->setVisible(false);
        pl->m_objectLayer->addChild(p);
        return p;
    }

    void ensureSimPlayers(PlayLayer* pl) {
        if (!m_simPlayer1.data()) m_simPlayer1 = createSimPlayer(pl);
        if (!m_simPlayer2.data() && pl->m_gameState.m_isDualMode) m_simPlayer2 = createSimPlayer(pl);
    }

    void releaseSimPlayers() {
        if (m_simPlayer1.data() && m_simPlayer1->getParent()) m_simPlayer1->removeFromParent();
        if (m_simPlayer2.data() && m_simPlayer2->getParent()) m_simPlayer2->removeFromParent();
        m_simPlayer1.swap(nullptr);
        m_simPlayer2.swap(nullptr);
    }

    static void resetCollisionLog(PlayerObject* self) {
        self->m_collisionLogTop->removeAllObjects();
        self->m_collisionLogBottom->removeAllObjects();
        self->m_collisionLogLeft->removeAllObjects();
        self->m_collisionLogRight->removeAllObjects();
    }

    // Same attribute + hitbox synchronisation Show Trajectory uses, plus the
    // movement state and the BUTTON state, so every test starts from exactly
    // what the real player has right now.
    void syncSimPlayer(PlayerObject* sim, PlayerObject* real) {
        sim->setVisible(false);
        sim->copyAttributes(real);
        // Button FIRST (1.23.1). Previous tests left the sim in whatever state
        // they ended in: release so a later press is a clean new press, then
        // hold if the game holds. pushButton is a new press - in UFO a flap -
        // so it must come before speed / position are copied; it used to come
        // after, and every test while you held in UFO started with a flap you
        // never made (one-tick check: UFO only 72% exact, y-speed off by ~1).
        sim->releaseButton(PlayerButton::Jump);
        if (m_gameHolding) sim->pushButton(PlayerButton::Jump);
        sim->setPosition(real->getPosition());
        sim->setScale(real->getScale());
        sim->setContentSize(real->getContentSize());
        sim->m_width = real->m_width;
        sim->m_height = real->m_height;
        sim->m_gravityMod = real->m_gravityMod;
        sim->m_isOnGround = real->m_isOnGround;
        sim->m_isUpsideDown = real->m_isUpsideDown;
        sim->m_vehicleSize = real->m_vehicleSize;
        sim->m_playerSpeed = real->m_playerSpeed;
        sim->m_yVelocity = real->m_yVelocity;
        sim->m_gravity = real->m_gravity;
        sim->m_speedMultiplier = real->m_speedMultiplier;
        // Ground / landing / portal state copyAttributes leaves behind (1.17.1).
        // Without it the sim player kept the last test's values: wave tests
        // "died" a tick after letting go, at a height it can't reach in a tick,
        // so good wave releases were held back (the wave went up).
        sim->m_position = real->m_position;
        sim->m_lastPortalPos = real->m_lastPortalPos;
        sim->m_lastGroundedPos = real->m_lastGroundedPos;
        sim->m_lastActivatedPortal = real->m_lastActivatedPortal;
        sim->m_isOnGround2 = real->m_isOnGround2;
        sim->m_lastLandTime = real->m_lastLandTime;
        sim->m_stateOnGround = real->m_stateOnGround;
        sim->m_isGoingLeft = real->m_isGoingLeft;
        sim->m_isSideways = real->m_isSideways;
        sim->m_isDashing = real->m_isDashing;
        // Orbs the real player already used this contact must not fire again.
        sim->m_touchedRings.clear();
        for (int id : real->m_touchedRings) sim->m_touchedRings.insert(id);
        // GD's own press buffer, as the real player has it (after the button).
        sim->m_jumpBuffered = real->m_jumpBuffered;
        // Everything else pushButton / earlier tests may have changed that the
        // physics reads (1.23.2). The one-tick check showed UFO still only ~67%
        // exact with the y-speed off: pushButton leaves a "press waiting" state
        // (m_stateJumpBuffered etc.) that the real player had already used, so
        // the sim flapped on its first tick. All plain values, no pointers.
        sim->m_stateJumpBuffered = real->m_stateJumpBuffered;
        sim->m_wasJumpBuffered = real->m_wasJumpBuffered;
        sim->m_stateRingJump = real->m_stateRingJump;
        sim->m_stateRingJump2 = real->m_stateRingJump2;
        sim->m_wasRobotJump = real->m_wasRobotJump;
        sim->m_fixRobotJump = real->m_fixRobotJump;
        sim->m_touchedRing = real->m_touchedRing;
        sim->m_touchedCustomRing = real->m_touchedCustomRing;
        sim->m_touchedGravityPortal = real->m_touchedGravityPortal;
        sim->m_touchedPad = real->m_touchedPad;
        sim->m_fallSpeed = real->m_fallSpeed;
        sim->m_isAccelerating = real->m_isAccelerating;
        sim->m_accelerationOrSpeed = real->m_accelerationOrSpeed;
        sim->m_isOnSlope = real->m_isOnSlope;
        sim->m_wasOnSlope = real->m_wasOnSlope;
        sim->m_slopeVelocity = real->m_slopeVelocity;
        sim->m_maybeUpsideDownSlope = real->m_maybeUpsideDownSlope;
        sim->m_slopeRotation = real->m_slopeRotation;
        sim->m_currentSlopeYVelocity = real->m_currentSlopeYVelocity;
        sim->m_isCurrentSlopeTop = real->m_isCurrentSlopeTop;
        sim->m_unk3d0 = real->m_unk3d0;
        sim->m_blackOrbRelated = real->m_blackOrbRelated;
        sim->m_unk3e0 = real->m_unk3e0;
        sim->m_unk3e1 = real->m_unk3e1;
        sim->m_collidedTopMinY = real->m_collidedTopMinY;
        sim->m_collidedBottomMaxY = real->m_collidedBottomMaxY;
        sim->m_collidedLeftMaxX = real->m_collidedLeftMaxX;
        sim->m_collidedRightMinX = real->m_collidedRightMinX;
        sim->m_stateFlipGravity = real->m_stateFlipGravity;
        sim->m_stateForce = real->m_stateForce;
        sim->m_stateForceVector = real->m_stateForceVector;
        sim->m_affectedByForces = real->m_affectedByForces;
        sim->m_lastMovedTime = real->m_lastMovedTime;
        sim->m_playerSpeedAC = real->m_playerSpeedAC;
        sim->m_jumpRelatedAC2 = real->m_jumpRelatedAC2;
        sim->m_lastJumpTime = real->m_lastJumpTime;
        sim->m_lastFlipTime = real->m_lastFlipTime;
        sim->m_lastSpiderFlipTime = real->m_lastSpiderFlipTime;
        sim->m_snapDistance = real->m_snapDistance;
        sim->m_totalTime = real->m_totalTime;
    }
    void setHeld(PlayerObject* p1, PlayerObject* p2, bool held) {
        if (held && !m_simHeld) m_simPressTick = m_simTick; // a new press inside this test
        m_simHeld = held;
        if (held) {
            p1->pushButton(PlayerButton::Jump);
            if (p2) p2->pushButton(PlayerButton::Jump);
        } else {
            p1->releaseButton(PlayerButton::Jump);
            if (p2) p2->releaseButton(PlayerButton::Jump);
        }
    }

    float getValidDt() const {
        if (m_frameDt > 0.001f && m_frameDt < 2.0f) return m_frameDt;
        return BASE_DT;
    }

    bool budgetExpired() const {
        return m_hasDeadline && Clock::now() > m_deadline;
    }
    void clearBuffer() {
        m_queue.clear();
        m_live = false;
        m_planTrust = false;
    }

    // Still on the planned path? Checked on tick boundaries only (a CBF sub-step
    // lands between two predicted positions).
    bool planDrifted(PlayLayer* pl, double now, float dt) const {
        PlayerObject* p = pl ? pl->m_player1 : nullptr;
        if (!p || dt <= 0.f) return true;
        double const steps = (now - m_planGrid) / static_cast<double>(dt);
        int const j = static_cast<int>(std::lround(steps));
        if (std::fabs(steps - static_cast<double>(j)) > 0.2) return false; // mid-tick: next boundary decides
        int const idx = j - 1; // after j steps (the first is the rest of the click's tick)
        if (idx < 0 || idx >= static_cast<int>(m_planPath.size())) return false;
        float const dx = p->getPositionX() - m_planPath[static_cast<size_t>(idx)].x;
        float const dy = p->getPositionY() - m_planPath[static_cast<size_t>(idx)].y;
        return std::fabs(dx) > PLAN_DRIFT_PX || std::fabs(dy) > PLAN_DRIFT_PX;
    }

    float simDt() const { return getValidDt() * (m_warp > 0.f ? m_warp : 1.f); }

    // Fraction of the current tick already done (0 on a boundary). Sims start
    // with the rest of this tick, and held inputs are scheduled on the real
    // tick grid, so a click made mid-tick (CBF) lines up with what was tested.
    float tickPhase() const { return (m_phase > 0.02f && m_phase < 0.98f) ? m_phase : 0.f; }

    // Per-tick budget for the live re-check of a held click: a fraction of the
    // frame time (it runs every tick), never more than a click's budget.
    int liveBudgetUs(Config const& c) const {
        int const click = clickBudgetUs(c);
        if (!m_haveFrame) return click;
        int const share = std::max(LIVE_BUDGET_MIN_US, static_cast<int>(m_frameUs * LIVE_BUDGET_FRAC));
        return click > 0 ? std::min(click, share) : share;
    }

    // CPU budget for this click in microseconds (0 = unlimited).
    int clickBudgetUs(Config const& c) const {
        int b = c.budgetUs;
        if (c.extreme && b > 0 && !c.autoBudget) b = std::max(b, EXTREME_MIN_BUDGET_US);
        if (c.autoBudget && m_haveFrame) {
            float const frac = c.extreme ? AUTO_BUDGET_FRAC_EXTREME : AUTO_BUDGET_FRAC;
            int a = static_cast<int>(m_frameUs * frac);
            a = std::clamp(a, AUTO_BUDGET_MIN_US, AUTO_BUDGET_MAX_US);
            b = (b > 0) ? std::min(b, a) : a;
        }
        return b;
    }

    static bool holdingJump(PlayerObject* p) {
        if (!p) return false;
        auto it = p->m_holdingButtons.find(static_cast<int>(PlayerButton::Jump));
        return it != p->m_holdingButtons.end() && it->second;
    }

    void fireButton(PlayLayer* pl, bool push) {
        bool const dualSingle = pl->m_gameState.m_isDualMode && (!pl->m_level || !pl->m_level->m_twoPlayerMode);
        m_gameHolding = push;
        m_osInput = true;
        rememberInput(pl, push);
        if (!m_directMode) {
            m_firingBuffered = true;
            pl->handleButton(push, 1, true);
            if (dualSingle) pl->handleButton(push, 1, false);
            m_firingBuffered = false;
        }

        // The input must reach the player on THIS tick - that is the tick the
        // prediction chose. Input mods (Click Between Frames) hook the same
        // function and can hold back or re-time inputs that don't come from
        // them. If the player didn't get it, apply it directly, now.
        auto apply = [&](PlayerObject* p) {
            if (!p || holdingJump(p) == push) return false;
            if (push) p->pushButton(PlayerButton::Jump);
            else      p->releaseButton(PlayerButton::Jump);
            return true;
        };
        bool direct = apply(pl->m_player1);
        if (dualSingle) direct = apply(pl->m_player2) || direct;
        if (direct) {
            m_directFires++;
            if (!m_directMode) {
                // From now on apply buffered inputs straight to the player, so a
                // late copy from that mod can't arrive as a second press.
                m_directMode = true;
                pushLog("note: another input mod held back a buffered input - applying them directly from now on",
                        COL_YELLOW, false);
            }
        }
    }

    // Make the game's hold state match the real button so nothing can stay stuck.
    // Nothing pending: make the game match the button. Only ever RELEASES on
    // its own - a stale "held" (release lost in the pause menu) used to make it
    // press for you.
    void reconcile(PlayLayer* pl) {
        if (m_gameHolding && !m_physicalHolding) fireButton(pl, false);
    }

    // ------------------------------------------------------------------------
    //  Safety nets (1.18.1): no input is ever lost or held forever.
    // ------------------------------------------------------------------------
    // Fire everything still waiting, in order, right now.
    // Diagnostics only: every 8 ticks, predict ONE tick from the real state with
    // the current button state, and compare with what the game did. Skipped if
    // an input happened in that tick or the step wasn't a full tick (CBF).
    void oneStepCheck(PlayLayer* pl, PlayerObject* p) {
        float const nominal = getValidDt();
        if (m_osPending) {
            m_osPending = false;
            if (!m_osInput && std::fabs(m_lastStepD - nominal) < 0.02f * nominal && !p->m_isDead) {
                float const dx = p->getPositionX() - m_osX;
                float const dy = p->getPositionY() - m_osY;
                double const dv = p->m_yVelocity - m_osVy;
                OneStep& o = m_oneStep[m_osMode];
                o.n++;
                float const off = std::hypot(dx, dy);
                if (off < 0.01f && std::fabs(dv) < 0.01) o.exact++;
                o.sdx += std::clamp(dx, -20.f, 20.f);
                o.sdy += std::clamp(dy, -20.f, 20.f);
                o.sdv += std::clamp(dv, -20.0, 20.0);
                o.worst = std::max(o.worst, off);
                if (off > 0.5f && !o.logged && o.n >= 3) {
                    o.logged = true;
                    pushLog(fmt::format("one-tick check [{}]: prediction differs after ONE tick (x {:+.2f}, y {:+.2f}, "
                                        "y-speed {:+.2f}) - your state isn't fully copied", m_osMode, dx, dy, dv),
                            COL_YELLOW, false);
                }
            }
        }
        if ((m_tick & 7) != 0 || m_queue.pending() || p->m_isDead || m_simulating || tickPhase() > 0.f) return;
        PlayerObject* p2 = (pl->m_gameState.m_isDualMode && pl->m_player2) ? pl->m_player2 : nullptr;
        bool const simP2 = p2 && !(pl->m_level && pl->m_level->m_twoPlayerMode);
        bool const rec = m_recordPath;
        m_recordPath = false;
        float const sdt = simDt();
        // delay 1 > horizon 0: one step with the button exactly as the game has it
        simulate(pl, p, p2, simP2, m_gameHolding, 1, 0, true, sdt, BASE_DT / std::max(0.001f, sdt));
        m_recordPath = rec;
        PlayerObject* s1 = m_simPlayer1.data();
        if (!s1) return;
        m_osX = s1->getPositionX();
        m_osY = s1->getPositionY();
        m_osVy = s1->m_yVelocity;
        ModeInfo mi;
        mi.add(p);
        m_osMode = mi.name();
        m_osInput = false;
        m_osPending = true;
    }

public:
    // Any input reached the game (raw or buffered): the one-tick check for this
    // step doesn't count.
    void noteInputForCheck() { m_osInput = true; }
private:

    void flushQueue(PlayLayer* pl) {
        int guard = 0;
        while (m_queue.pending() && guard++ < 64) {
            auto due = m_queue.poll(1e18, TIME_EPS);
            if (!due) break;
            fireButton(pl, *due);
        }
        m_queue.clear();
        m_live = false;
    }

    // Is the physics-step hook (which fires held inputs) running right now?
    bool stepsRunning() const {
        if (m_lastStepWall.time_since_epoch().count() == 0) return false;
        return Clock::now() - m_lastStepWall < std::chrono::milliseconds(STEP_STALL_MS);
    }

public:
    // Once per rendered frame: a held input waiting far longer than any window
    // allows (wall clock), or held while the step hook isn't running at all
    // (another mod replaced it, a huge hitch), is fired now.
    void watchdog(PlayLayer* pl) {
        if (!pl || pl->m_isPaused || !m_queue.pending()) {
            m_pendingSinceWall = Clock::time_point{};
            return;
        }
        auto const now = Clock::now();
        if (m_pendingSinceWall.time_since_epoch().count() == 0) m_pendingSinceWall = now;
        bool const stuck = now - m_pendingSinceWall > std::chrono::milliseconds(WATCHDOG_MS);
        if (stuck || !stepsRunning()) {
            m_watchdogFires++;
            if (m_watchdogFires == 1) {
                pushLog(stuck ? "note: a held input waited too long - released it (watchdog)"
                              : "note: physics hook not running - inputs go straight through",
                        COL_YELLOW, false);
            }
            flushQueue(pl);
            m_pendingSinceWall = Clock::time_point{};
        }
    }

    // Pausing: the release usually happens in the pause menu, which never reaches
    // us. Drop what was waiting and assume the button is up.
    void onPause(PlayLayer* pl) {
        if (m_queue.pending()) flushQueue(pl);
        clearBuffer();
        m_physicalHolding = false;
        m_pendingSinceWall = Clock::time_point{};
    }
private:

    // ------------------------------------------------------------------------
    //  Prediction cut-off: should this unmodelled object stop the lookahead?
    //
    //  Uses the object's real hitbox and the direction the sim player travels:
    //    * pads / portals / speed changers fire on CONTACT. They only matter if
    //      the player had not already touched them when the input arrived, and
    //      only once the player is about to touch them. Anything behind the
    //      player (or already being touched) is history.
    //    * orbs fire on a CLICK while overlapped, so they matter whenever the
    //      player is about to overlap one or is overlapping one right now.
    //    * an object the player cannot reach vertically never matters.
    // ------------------------------------------------------------------------
    // True the first time `obj` is applied in the current test.
    bool markUsed(GameObject* obj) {
        if (std::find(m_simUsed.begin(), m_simUsed.end(), obj) != m_simUsed.end()) return false;
        m_simUsed.push_back(obj);
        return true;
    }

    bool padStrength(GameObjectType t, float& out) const {
        int const i = static_cast<int>(t);
        if (i < 0 || i >= 64) return false;
        PadLearn const& p = m_pads[i];
        if (!p.seen || p.unstable) return false;
        out = p.bump;
        return true;
    }

    void checkStateChange(PlayerObject* player, GameObject* obj, SimTrack const& tr) {
        CCPoint pos = obj->getPosition();
        // A moving orb (move triggers): test it where it will be at this sim
        // step, not where it is now (1.22.0; not in ship / wave).
        float odx = 0.f, ody = 0.f;
        if (m_simMovers && isOrb(obj->m_objectType) && !m_movers.empty()) {
            auto it = m_movers.find(obj);
            if (it != m_movers.end() && it->second.moving(m_simNow, MOVER_FRESH, MOVER_MIN_SPEED) &&
                obj->isVisible() && !obj->m_isGroupDisabled && !obj->m_isDisabled) {
                it->second.offsetAt(m_simNow + static_cast<double>(m_simTick + 1) * m_simDtCur, odx, ody);
                pos.x += odx;
                pos.y += ody;
            }
        }
        float const px = player->getPositionX();
        float const py = player->getPositionY();

        // Cheap reject before asking the object for its hitbox.
        if (std::fabs(pos.x - px) > 240.f || std::fabs(pos.y - py) > 240.f) return;

        float hw = 0.f, hh = 0.f;
        float cx = pos.x, cy = pos.y;
        CCRect const& r = obj->getObjectRect();
        if (r.size.width > 1.f && r.size.height > 1.f && r.size.width < 260.f && r.size.height < 520.f) {
            hw = r.size.width * 0.5f;
            hh = r.size.height * 0.5f;
            cx = r.origin.x + hw + odx;
            cy = r.origin.y + hh + ody;
        } else {
            Extent const e = fallbackExtent(obj->m_objectType);
            hw = e.hw;
            hh = e.hh;
        }

        float const size = std::clamp(player->m_vehicleSize, 0.3f, 1.3f);
        float const ph   = 15.f * size; // player half-size (mini is smaller)
        float const pad  = 6.f;

        // Vertical: can the player's hitbox ever overlap this object at this height?
        float const dy = std::fabs(cy - py) - hh - ph;
        if (dy > pad) return;

        // Horizontal, along the direction of travel.
        float const ahead    = (cx - px) * tr.dir;
        float const gap      = ahead - hw - ph;                       // > 0: not touching yet
        float const gapStart = (cx - tr.startX) * tr.dir - hw - ph;   // same, when the input arrived
        float const reach    = tr.stepPx + pad;                       // about one tick of travel
        bool const overlapping = gap <= 0.f && ahead >= -(hw + ph) && dy <= 0.f;
        // Orbs: GD's activation area is a bit more generous than this box test.
        // A click "touching" an orb within the pad counts as hitting it (it used
        // to stop the prediction instead: "raw: pink orb (touching)").
        bool const orbOverlap = gap <= pad && ahead >= -(hw + ph) - pad && dy <= pad;
        GameObjectType const type = obj->m_objectType;

        // Orb hit by the click being tested: let the sim player use it (only the
        // first one, and only shortly after the input lands, since we can't know
        // if a long hold would still be held by then). Otherwise the normal
        // cut-off rules below apply.
        // "Pressed" = still holding, or a press GD hasn't used up yet (its own
        // jump buffer), so a quick tap just before the orb still counts.
        // UFO / swing only use an orb on a fresh click (a press within the last
        // ~3 ticks) - holding from an earlier
        // click doesn't fire it. The sim used to treat a held UFO press as
        // hitting every orb on the way, so every "fire now" died and held
        // clicks never fired. Other modes keep hold-to-hit-orb.
        bool const clickOnly = player->m_isBird || player->m_isSwing;
        bool const freshPress = m_simHeld && m_simPressTick >= 0 && m_simTick - m_simPressTick <= m_orbClick;
        bool const pressed = clickOnly ? freshPress : (m_simHeld || player->m_jumpBuffered);
        // The orb this test already used: nothing more can happen with it. It
        // used to stop the prediction on the very next tick, so EVERY click on
        // an orb went "raw: ... orb (touching)".
        // (1.18.6 also skipped orbs in GD's "touched rings" list - but GD puts an
        // orb there as soon as you overlap it, so the orb you were clicking got
        // ignored and every click said "too late". Removed in 1.18.8.)
        // Orb contact by hitbox (1.24.8): GD only reports orb contact while the
        // button is down, so a path without a press never "saw" the orb coming
        // and orb aim never started.
        if (isModelledOrb(type) && player == m_simPlayer1.data() && orbOverlap && !realUsedOrb(obj) &&
            obj != m_simOrbObj) {
            if (auto* ring = typeinfo_cast<RingObject*>(obj)) {
                // the next orb after the one this test used (orb chains)
                if (m_simOrbUsed && m_simNextRingTick < 0) m_simNextRingTick = m_simTick;
                if (!m_simFirstRing) {
                    m_simFirstRing = ring;
                    m_simFirstRingTick = m_simTick;
                    m_simFirstRingLast = m_simTick;
                } else if (ring == m_simFirstRing && m_simTick > m_simFirstRingLast && m_simTick <= m_simFirstRingLast + 1) {
                    m_simFirstRingLast = m_simTick;
                }
            }
        }
        if (isOrb(type) && obj == m_simOrbObj) return;
        // GD reports orb contact exactly (see simRingTouched / applyGdRing): no
        // guessing from boxes, and a modelled orb never cuts the prediction.
        if (m_gdRings && m_orbPredict && isModelledOrb(type)) return;
        if (m_orbPredict && isModelledOrb(type) && !m_simOrbUsed && pressed && orbOverlap) {
            int const since = m_simTick - m_simDelay;
            if (since >= 0 && since <= m_orbIntent) {
                if (auto* ring = typeinfo_cast<RingObject*>(obj)) {
                    m_objRestore.emplace_back(obj, obj->m_isActivated);
                    m_simOrbUsed = true;
                    m_simOrbObj = obj;
                    simRingJump(player, ring);
                    return; // modelled: no cut-off
                }
            }
        }

        // Size portals (1.25.1): applied to the hidden sim player only, by
        // setting its size directly - no GD portal code, nothing on the real
        // portal object changes (1.20.0 called GD's toggles and broke real
        // portals). The orbs in Amethyst's UFO part sit right at mini / normal
        // size portals, so every orb click there went "raw: size portal".
        if (type == GameObjectType::MiniSizePortal || type == GameObjectType::RegularSizePortal) {
            if (overlapping && markUsed(obj)) {
                float const want = (type == GameObjectType::MiniSizePortal) ? MINI_SIZE : 1.f;
                if (std::fabs(player->m_vehicleSize - want) > 0.01f) {
                    player->m_vehicleSize = want;
                    player->setScale(want);
                    m_simSizeSwitches++;
                }
            }
            return; // modelled: never cuts the prediction
        }

        // Pads and speed / gravity portals fire on contact, no click involved,
        // so the sim can apply them itself. Anything the player was already
        // touching when the input arrived has been applied by the game.
        if (m_padPredict && gapStart > 0.f) {
            if (isModelledPad(type)) {
                float bump = 0.f;
                if (padStrength(type, bump)) {
                    if (overlapping && markUsed(obj)) {
                        m_objRestore.emplace_back(obj, obj->m_isActivated);
                        player->bumpPlayer(bump, static_cast<int>(type), true, obj);
                    }
                    return; // modelled (now or on contact)
                }
                // strength not seen yet -> cut-off, like before
            } else if (type == GameObjectType::Modifier) {
                float speed = 0.f;
                if (speedForPortal(obj->m_objectID, speed)) {
                    if (overlapping && markUsed(obj)) player->m_playerSpeed = speed;
                    return;
                }
            } else if (isGravityPortal(type)) {
                if (overlapping && markUsed(obj)) {
                    bool const want = type == GameObjectType::InverseGravityPortal ? true
                                    : type == GameObjectType::NormalGravityPortal  ? false
                                    : !player->m_isUpsideDown;
                    if (want != player->m_isUpsideDown) player->flipGravity(want, true);
                }
                return;
            }
        }

        bool relevant = false;
        if (isOrb(obj->m_objectType)) {
            relevant = gap <= reach && ahead >= -(hw + ph) - pad;
        } else {
            relevant = gapStart > 0.f && gap <= reach;
        }
        if (!relevant) return;

        m_simTrunc.valid = true;
        m_simTrunc.type  = obj->m_objectType;
        m_simTrunc.gap   = gap;
    }

    // ------------------------------------------------------------------------
    //  Core lookahead: apply `push` after `delay` ticks and see what happens.
    // ------------------------------------------------------------------------
    SimResult simulate(PlayLayer* pl, PlayerObject* realP1, PlayerObject* realP2, bool simP2,
                       bool push, int delay, int horizon, bool noLanding, float dt, float s,
                       FollowUp const& follow = FollowUp{}) {
        SimResult out;
        PlayerObject* s1 = m_simPlayer1.data();
        PlayerObject* s2 = (simP2 && realP2) ? m_simPlayer2.data() : nullptr;
        if (!s1 || !realP1) {
            out.ticks = horizon;
            return out;
        }

        auto const simT0 = Clock::now();
        m_simulating = true;
        m_simDead = false;
        m_simTrunc = TruncInfo{};
        m_simDelay = delay;
        m_simTick = 0;
        m_simOrbUsed = false;
        m_simOrbObj = nullptr;
        m_simPressTick = -1; // a hold from before the test isn't a fresh click
        m_simFirstRing = nullptr;
        m_simFirstRingTick = -1;
        m_simFirstRingLast = -1;
        m_simNextRingTick = -1;
        m_simOrbStep = -1;
        orbModelLoad();
        m_prevRing[0] = touchingOrb(realP1);
        m_prevRing[1] = (simP2 && realP2) ? touchingOrb(realP2) : nullptr;
        m_simMover = false;
        m_simKillType = -1;
        m_simKillMover = false;
        m_simKillDy = 0.f;
        m_simStartY = realP1 ? realP1->getPositionY() : 0.f;
        m_simNow = m_gameTime;
        m_simDtCur = dt;
        m_objRestore.clear();
        m_simUsed.clear();
        if (m_recordPath) m_path.clear();

        syncSimPlayer(s1, realP1);
        if (s2) syncSimPlayer(s2, realP2);
        m_simHeld = m_gameHolding; // until the action lands, the button keeps the game's current state

        m_track[0] = SimTrack{s1->getPositionX(), 1.f, 0.f};
        m_track[1] = SimTrack{s2 ? s2->getPositionX() : 0.f, 1.f, 0.f};

        int airborne = 0;
        int landed = 0;
        int const airMin = scaleTicks(15, s);
        // Long enough that a spike right after landing is still seen.
        int const landMin = scaleTicks(40, s);
        int const total = delay + horizon;
        int const clrCommit = scaleTicks(ESCAPE_COMMIT_BASE, s);
        int const clrGrace  = scaleTicks(LAND_GRACE_BASE, s);

        float const fullDt = dt;
        float const phase0 = tickPhase();
        for (int i = 0; i < total; i++) {
            m_simTick = i;
            m_simStepsTotal++;
            // step 0 = the rest of the current tick (mid-tick after a CBF click)
            dt = (i == 0 && phase0 > 0.f) ? fullDt * (1.f - phase0) : fullDt;
            if (i == delay) {
                if (push && m_orbModel == 2) {
                    setHeld(s1, s2, push);
                    orbPress(s1, s2);
                } else if (!(push && m_orbModel == 0 && orbPress(s1, s2))) {
                    setHeld(s1, s2, push);
                }
            }
            // Optional follow-up inputs (used to ask "could the player still save this?").
            if (i == follow.at[0]) setHeld(s1, s2, follow.held[0]);
            if (i == follow.at[1]) setHeld(s1, s2, follow.held[1]);

            s1->update(dt);
            s1->updateRotation(dt);
            if (s2) {
                s2->update(dt);
                s2->updateRotation(dt);
            }
            if (m_simDead) { out.end = SimEnd::Died; break; }

            // After the first tick we know which way each sim player travels and how far per tick.
            if (i == 0) {
                // (a partial first step covers less ground: scale to a full tick)
                float const full = (phase0 > 0.f) ? 1.f / (1.f - phase0) : 1.f;
                float const dx1 = s1->getPositionX() - m_track[0].startX;
                m_track[0].dir = dx1 >= 0.f ? 1.f : -1.f;
                m_track[0].stepPx = std::fabs(dx1) * full;
                if (s2) {
                    float const dx2 = s2->getPositionX() - m_track[1].startX;
                    m_track[1].dir = dx2 >= 0.f ? 1.f : -1.f;
                    m_track[1].stepPx = std::fabs(dx2) * full;
                }
            }

            m_tickClr = 1e9f;
            m_tickRing[0] = m_tickRing[1] = nullptr;
            resetCollisionLog(s1);
            pl->checkCollisions(s1, dt, false);
            if (m_simDead) { out.end = SimEnd::Died; break; }
            m_prevRing[0] = m_tickRing[0]; // what it touches now = "already touching" next step
            applyGdRing(s1, 0);

            if (s2) {
                resetCollisionLog(s2);
                pl->checkCollisions(s2, dt, false);
                if (m_simDead) { out.end = SimEnd::Died; break; }
                m_prevRing[1] = m_tickRing[1];
                applyGdRing(s2, 1);
            }
            // Orb aim: alive a few ticks past the next orb = that orb is your next
            // click's job; the rest of the path doesn't matter (CPU).
            if (m_simChainStop > 0 && m_simNextRingTick >= 0 && i >= m_simNextRingTick + m_simChainStop) {
                out.ticks = horizon;
                out.end = SimEnd::Survived;
                break;
            }

            if (i >= delay) {
                out.ticks++;
                if (!noLanding) {
                    bool grounded = s1->m_isOnGround && (!s2 || s2->m_isOnGround);
                    if (!grounded) {
                        airborne++;
                        landed = 0;
                    } else if (airborne >= airMin) {
                        if (out.landTick < 0) out.landTick = out.ticks;
                        // Landed and alive past the grace: every verdict treats a
                        // later death as the next jump's job, so stop here (was
                        // 40 more ticks of collision checks per test).
                        ++landed;
                        if (out.ticks > out.landTick + clrGrace || landed >= landMin) {
                            out.ticks = horizon;
                            out.end = SimEnd::Survived;
                            break;
                        }
                    }
                }
                // Clearance over the part of the path this click is judged on.
                bool const judged = noLanding ? (out.ticks <= clrCommit)
                                              : (out.landTick < 0 || out.ticks <= out.landTick + clrGrace);
                if (judged) out.clear = std::min(out.clear, m_tickClr);
            }

            if (m_recordPath) m_path.push_back(ibcore::Point2{s1->getPositionX(), s1->getPositionY(), s1->getRotation()});

            if (m_simTrunc.valid) {
                out.end = SimEnd::Truncated;
                out.trunc = m_simTrunc;
                break;
            }

            if ((i & 3) == 3 && budgetExpired()) { // every 4 steps: heavy levels can't overrun far
                out.end = SimEnd::OutOfBudget;
                break;
            }
        }

        // Put back anything an orb activation changed on real level objects.
        // Newest first, so the oldest snapshot (before the test touched it) wins.
        for (auto it = m_objRestore.rbegin(); it != m_objRestore.rend(); ++it) it->first->m_isActivated = it->second;
        m_objRestore.clear();

        out.mover = m_simMover;
        out.killType = m_simKillType;
        out.killMover = m_simKillMover;
        out.killDy = m_simKillDy;
        out.orbUsed = m_simOrbUsed;
        out.firstRing = m_simFirstRing;
        out.firstRingTick = m_simFirstRingTick;
        out.firstRingLast = m_simFirstRingLast;
        out.nextRingTick = m_simNextRingTick;
        if (out.end == SimEnd::Died) out.deathStep = m_simTick; // step index incl. the delay
        if (m_simMover) {
            m_moverTests++;
            if (out.end == SimEnd::Died) m_moverDeaths++;
        }
        m_simUsTotal += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - simT0).count();
        m_simulating = false;
        return out;
    }

    // ------------------------------------------------------------------------
    //  Diagnostics
    // ------------------------------------------------------------------------
    static float ticksToMs(long long ticks, float dt) {
        return static_cast<float>(ticks) * dt * (1000.f / 60.f);
    }

    float levelMs() const {
        return std::chrono::duration<float, std::milli>(Clock::now() - m_levelStart).count();
    }

    // "hits spike at +40px, via orb" - what a predicted death runs into.
    static std::string deathWhat(SimResult const& r) {
        std::string what = r.killType >= 0 ? objectName(static_cast<GameObjectType>(r.killType)) : "a block edge";
        if (r.killMover) what = "moving " + what;
        return fmt::format("hits {} at {:+.0f}px{}", what, r.killDy, r.orbUsed ? ", after using an orb" : "");
    }

    // Decide whether (and how) a click shows up in the on-screen log.
    // One click as a Log line: tag + short text ("BUFFER  press +6t (25ms) [ufo]").
    static bool panelLine(ClickNote const& n, Config const& c, float dt, std::string& tag, std::string& text,
                          ccColor3B& color) {
        char const* act = n.push ? "press" : "release";
        std::string const mode = n.modeName.empty() ? std::string() : fmt::format(" [{}]", n.modeName);
        color = COL_WHITE;
        switch (n.d) {
            case Decision::Buffered:
                if (n.rescue) {
                    tag = "HOLD";
                    text = fmt::format("{}, checking every tick (up to +{}t){}", act, n.windowTicks, mode);
                    return true;
                }
                if (n.stream && !c.hudVerbose) return false; // spam: one line per stream is enough
                tag = "BUFFER";
                text = fmt::format("{} +{}t ({:.1f}ms){}{}", act, n.delay, ticksToMs(n.delay, dt),
                                   n.stream ? ", spam kept steady" : "", mode);
                return true;
            case Decision::Queued:
                tag = "QUEUED";
                text = fmt::format("{}, keeps your spacing", act);
                color = COL_GREY;
                return true;
            case Decision::PassTruncated:
                tag = "RAW";
                text = fmt::format("{}, {}", act, describeTrunc(n.r0.trunc));
                return true;
            case Decision::PassNoFix:
                // Shown only with "Log: show safe clicks": the click went through
                // as made, nothing for you to act on.
                if (!c.hudVerbose) return false;
                tag = "RAW";
                if (n.needed == 0) {
                    text = fmt::format("{}, too late: dies in {}t ({}){}", act, n.r0.ticks, deathWhat(n.r0), mode);
                } else if (n.needed > 0) {
                    text = fmt::format("{}, needed +{}t (window {}t){}", act, n.needed, n.windowTicks, mode);
                } else {
                    text = fmt::format("{}, no fix in {}t (dies in {}t, {}){}", act, n.windowTicks, n.r0.ticks,
                                       deathWhat(n.r0), mode);
                }
                color = COL_GREY;
                return true;
            case Decision::PassSpam:
                tag = "RAW";
                text = fmt::format("{}, spam guard", act);
                return true;
            case Decision::PassBudget:
                tag = "RAW";
                text = fmt::format("{}, out of CPU time", act);
                return true;
            case Decision::PassEscapable:
                if (!c.hudVerbose) return false;
                tag = "OK";
                text = fmt::format("{}, your next click saves it", act);
                color = COL_GREY;
                return true;
            case Decision::PassSafe:
                if (!c.hudVerbose) return false;
                tag = "OK";
                text = fmt::format("{}, safe", act);
                color = COL_GREY;
                return true;
            case Decision::PassRelease:
                return false; // releases that do nothing: never worth a line (1.25.2)
            case Decision::PassModeOff:
                if (!c.hudVerbose) return false;
                tag = "RAW";
                text = fmt::format("{}, {} is off (Modes tab)", act, n.modeName.empty() ? "this mode" : n.modeName);
                color = COL_GREY;
                return true;
            case Decision::PassNoWindow:
                if (!c.hudVerbose) return false;
                tag = "RAW";
                text = fmt::format("{}, window under 1 tick", act);
                color = COL_GREY;
                return true;
            default:
                return false;
        }
    }

    // Record one click decision (stats always, text only if a log is enabled).
    void note(Config const& c, ModeInfo const& mode, float dt, ClickNote const& n) {
        m_stats.clicks++;
        m_stats.byDecision[static_cast<int>(n.d)]++;
        if (n.us > 0) {
            m_stats.simClicks++;
            m_stats.sumUs += n.us;
            m_stats.maxUs = std::max(m_stats.maxUs, n.us);
        }
        // The click a death gets blamed on: an irrelevant release (cube) or an
        // input queued behind a held one must not hide the press that mattered.
        if (n.d != Decision::PassRelease && n.d != Decision::Queued) {
            m_last.decision = n.d;
            m_last.f0End = n.r0.end;
            m_last.f0Ticks = n.r0.ticks;
            m_last.delay = n.delay;
            m_last.horizon = n.horizon;
            m_last.time = m_gameTime;
            m_last.trunc = n.r0.trunc;
        }

        // (no early return: the menu's Log tab keeps a history even with all logs off)

        std::string const modeName = mode.name();
        m_lastMode = modeName;
        int const tps = static_cast<int>(std::lround(60.0f / dt));

        // Extra column: what cut the prediction (if anything) and whether extreme was on.
        std::string extra;
        if (n.d == Decision::PassTruncated) extra = describeTrunc(n.r0.trunc);
        if (c.extreme) extra += extra.empty() ? (c.custom ? "custom" : "extreme") : (c.custom ? " [custom]" : " [extreme]");

        if (c.debug) {
            log::debug("[InputBuffer] {} {} {} tps={} win={}t h={} sim={}us evals={} f0={}/{} delay={}{} {}",
                       modeName, n.push ? "press" : "release", decisionName(n.d), tps, n.windowTicks,
                       n.horizon, n.us, n.evals, simEndName(n.r0.end), n.r0.ticks, n.delay,
                       n.orb ? " orb" : "", extra);
        }
        if (c.csv) {
            m_csv.row(fmt::format("CLICK,{:.1f},{},{},{},{},{},{},{},{},{},{},{}",
                                  levelMs(), modeName, n.push ? "press" : "release", tps, n.windowTicks,
                                  n.horizon, n.us, simEndName(n.r0.end), n.r0.ticks, decisionName(n.d),
                                  n.delay, extra));
        }
        {
            // One line, built only if it will be shown (1.26.1: every click used to
            // build it twice and keep hidden "safe click" lines too).
            std::string tag, text;
            ccColor3B color = COL_WHITE;
            ClickNote tagged = n;
            tagged.modeName = modeName;
            if (panelLine(tagged, c, dt, tag, text, color)) pushTagged(std::move(tag), std::move(text), color);
        }
    }

    // One log line into the history shown by the window's Log tab.
    void pushLog(std::string text, ccColor3B color, bool minor) {
        // Tag from how the line starts (lines that don't say: NOTE / CHECK).
        auto starts = [&](char const* p) { return text.rfind(p, 0) == 0; };
        std::string tag = "NOTE";
        if (starts("DEATH: ")) { tag = "DEATH"; text = text.substr(7); }
        else if (starts("PRESS fired ") || starts("REL fired ")) {
            tag = "FIRED";
            text = (starts("PRESS") ? "press " : "release ") + text.substr(text.find("fired ") + 6);
        }
        else if (starts("note: ") || starts("NOTE: ")) text = text.substr(6);
        else if (starts("prediction off") || starts("one-tick") || starts("orb ") || starts("learned")) tag = "CHECK";
        else if (starts("PRESS ") || starts("REL ")) {
            tag = "HOLD";
            text = (starts("PRESS") ? "press" : "release") + text.substr(text.find(' '));
        }
        pushTagged(std::move(tag), std::move(text), color, minor);
    }

    void pushTagged(std::string tag, std::string text, ccColor3B color, bool minor = false) {
        m_logVersion++;
        float const t = levelMs() / 1000.f;
        if (!m_history.empty() && m_history.back().text == text && m_history.back().tag == tag) {
            m_history.back().repeat++;
            m_history.back().t = t;
        } else {
            m_history.push_back(HistEntry{std::move(tag), std::move(text), color, t, 1, minor});
        }
        while (m_history.size() > MAX_HISTORY) m_history.pop_front();
    }

    void notePredictionMiss(float err, float dx = 0.f, float dy = 0.f) {
        Config const& c = cfg();
        if (c.debug) log::debug("[InputBuffer] prediction off by {:.1f}px at the 100ms check", err);
        (void)c;
        pushLog(fmt::format("prediction off by {:.1f}px [{}] (sideways {:+.1f}, up/down {:+.1f}; you vs predicted)",
                            err, m_trackMode, dx, dy), COL_YELLOW, false);
    }

    DeathReason classifyDeath(long long age) const {
        if (m_last.decision == Decision::None || age < 0) return DeathReason::NoRecentClick;
        if (age > static_cast<long long>(m_last.horizon) + m_last.delay + 8) {
            return DeathReason::NoRecentClick;
        }
        switch (m_last.decision) {
            case Decision::PassSafe:
                return age <= m_last.horizon ? DeathReason::SimMispredict : DeathReason::LaterObstacle;
            case Decision::PassTruncated: return DeathReason::Truncated;
            case Decision::PassSpam:      return DeathReason::SpamBypass;
            case Decision::PassNoFix:     return DeathReason::WindowTooSmall;
            case Decision::PassBudget:    return DeathReason::OutOfBudget;
            case Decision::Buffered:      return DeathReason::BufferedStillDied;
            case Decision::PassEscapable: return DeathReason::EscapeNotTaken;
            default:                      return DeathReason::Other;
        }
    }

    void reportDeath() {
        if (m_lastDeathTick == m_tick) return;
        m_lastDeathTick = m_tick;

        float const dt = getValidDt();
        long long const age = (m_last.time >= 0.0)
            ? static_cast<long long>(std::llround((m_gameTime - m_last.time) / dt)) : -1;
        DeathReason const why = classifyDeath(age);
        m_stats.deaths++;
        m_stats.byDeath[static_cast<int>(why)]++;

        float const ageMs = (age >= 0) ? ticksToMs(age, dt) : -1.f;
        // For "no fix" / "mispredict" the interesting number is predicted vs real death time.
        if (m_logDebug) {
            log::debug("[InputBuffer] DEATH reason={} lastClick={} ({:.1f}ms ago) sim_f0={}/{} real_age={}t",
                       deathName(why), decisionName(m_last.decision), ageMs,
                       simEndName(m_last.f0End), m_last.f0Ticks, age);
        }
        if (m_logCsv) {
            m_csv.row(fmt::format("DEATH,{:.1f},{},,{},,{},,{},{},{},{},{:.1f}",
                                  levelMs(), m_lastMode, static_cast<int>(std::lround(60.0f / dt)),
                                  m_last.horizon, simEndName(m_last.f0End), m_last.f0Ticks,
                                  deathName(why), age, ageMs));
            m_csv.flush();
        }
        {
            std::string text;
            if (why == DeathReason::WindowTooSmall && m_last.needed > 0) {
                text = fmt::format("DEATH: click was {}t ({:.0f}ms) too early, window {}t", m_last.needed,
                                   ticksToMs(m_last.needed, dt), m_last.window);
            } else if (why == DeathReason::WindowTooSmall && m_last.needed == 0) {
                text = fmt::format("DEATH: click came too late (no delay up to {}t helps){}", m_last.probe,
                                   m_lastMode.empty() ? std::string() : fmt::format(" [{}]", m_lastMode));
            } else if (why == DeathReason::BufferedStillDied) {
                std::string const tag = m_lastMode.empty() ? std::string() : fmt::format(" [{}]", m_lastMode);
                if (m_last.liveTicks < 0 && m_queue.pending()) {
                    // still held when you died: the live check never found it safe
                    text = fmt::format("DEATH: held click hadn't fired yet ({}t after it, planned +{}t){} | live: {}",
                                       age, m_last.delay, tag, refuseSummary());
                } else if (m_last.liveTicks >= 0) {
                    text = fmt::format("DEATH: buffered, fired +{}t ({}), planned +{}t, still died{}", m_last.liveTicks,
                                       fireReasonText(m_last.liveReason), m_last.delay, tag);
                } else {
                    text = fmt::format("DEATH: buffered +{}t still died (prediction was off){}", m_last.delay, tag);
                }
            } else if (why == DeathReason::Truncated && m_last.trunc.valid) {
                text = fmt::format("DEATH: {} cut prediction", objectName(m_last.trunc.type));
            } else if (ageMs >= 0.f && why != DeathReason::NoRecentClick) {
                text = fmt::format("DEATH: {} ({:.0f}ms after click)", deathText(why), ageMs);
            } else {
                text = fmt::format("DEATH: {}", deathText(why));
            }
            pushLog(std::move(text), COL_WHITE, false); // the red DEATH tag says it
        }
    }

    std::string summary() const {
        auto const& st = m_stats;
        auto d = [&](Decision x) { return st.byDecision[static_cast<int>(x)]; };
        auto w = [&](DeathReason x) { return st.byDeath[static_cast<int>(x)]; };
        long long avg = st.simClicks > 0 ? st.sumUs / st.simClicks : 0;
        return fmt::format(
            "clicks={} buffered={} safe={} escape={} truncated={} spam={} nofix={} budget={} | sim avg={}us max={}us scale={:.2f} | "
            "accuracy={:.1f}% of {} (err {:.1f}px, died {}, dropped {}) | "
            "deaths={} [mispredict={} later={} truncated={} spam={} window={} budget={} buffered={} escape={} none={}]",
            st.clicks, d(Decision::Buffered), d(Decision::PassSafe), d(Decision::PassEscapable), d(Decision::PassTruncated),
            d(Decision::PassSpam), d(Decision::PassNoFix), d(Decision::PassBudget),
            avg, st.maxUs, m_horizonScale,
            m_tracker.stats().percent(), m_tracker.stats().checks, m_tracker.stats().avgErr(),
            m_tracker.stats().deaths, m_tracker.stats().dropped,
            st.deaths, w(DeathReason::SimMispredict), w(DeathReason::LaterObstacle),
            w(DeathReason::Truncated), w(DeathReason::SpamBypass), w(DeathReason::WindowTooSmall),
            w(DeathReason::OutOfBudget), w(DeathReason::BufferedStillDied), w(DeathReason::EscapeNotTaken),
            w(DeathReason::NoRecentClick));
    }

    // ------------------------------------------------------------------------
    //  Top-right panel content
    // ------------------------------------------------------------------------
    std::string headerText(Config const& c) const {
        if (!c.enabled) return "INPUT BUFFER  OFF (raw input)";
        if (!m_active)  return "INPUT BUFFER  OFF (practice only)";
        if (c.custom)   return "INPUT BUFFER  CUSTOM";
        if (c.extreme)  return "INPUT BUFFER  EXTREME";
        return "INPUT BUFFER  ON";
    }

    ccColor3B headerColor(Config const& c) const {
        if (!c.enabled || !m_active) return COL_RED;
        if (c.custom)   return COL_CYAN;
        if (c.extreme)  return COL_ORANGE;
        return COL_GREEN;
    }

    std::string statusText(PlayLayer* pl, Config const& c) const {
        PlayerObject* p1 = pl->m_player1;
        if (!p1) return "-";
        if (p1->m_isPlatformer) return "platformer: buffer not used";
        bool const dual = pl->m_gameState.m_isDualMode && pl->m_player2;
        ModeInfo mode;
        mode.add(p1);
        if (dual) mode.add(pl->m_player2);
        float const dt = getValidDt();
        int const tps = static_cast<int>(std::lround(60.0f / dt));
        int const wt = mode.windowTicks(effectiveCfg(c), true, dt);
        std::string s = fmt::format("{}{} | {}tps", mode.name(), dual ? " dual" : "", tps);
        if (m_haveFrame) s += fmt::format(" {}fps", static_cast<int>(std::lround(1e6f / m_frameUs)));
        if (m_cbfLoaded) s += " CBF";
        s += fmt::format(" | {:.2f}f = {}t", static_cast<float>(wt) * dt, wt);
        int const budget = clickBudgetUs(c);
        if (budget > 0) s += fmt::format(" | cpu {:.1f}ms", budget / 1000.0);
        if (m_horizonScale < 0.99f) s += fmt::format(" x{:.2f}", m_horizonScale);
        return s;
    }

    std::string footerText() const {
        auto const& st = m_stats;
        std::string out = fmt::format("clicks {}  buffered {}  deaths {}", st.clicks,
                                      st.byDecision[static_cast<int>(Decision::Buffered)], st.deaths);
        auto const& a = m_tracker.stats();
        if (a.checks > 0) out += fmt::format("  |  pred {:.1f}% ({})", a.percent(), a.checks);
        return out;
    }

    // Badge, overlay fade-out, and making sure the window exists when it should.
    // (The window refreshes its own contents.)
    void refreshHud(PlayLayer* pl, bool) {
        if (!pl) return;
        Config const& c = cfg();
        updateBadge(pl, c);
        tickOverlay();
        if (c.hud) windowEnsure();
    }

public:
    // ------------------------------------------------------------------------
    //  Lifecycle (no heavy work at level load: sim players are made lazily)
    // ------------------------------------------------------------------------
    void init(PlayLayer* pl) {
        releaseSimPlayers(); // stale ones from a previous level, if any
        m_simulating = false;
        m_firingBuffered = false;
        m_horizonScale = 1.0f;
        m_hasDeadline = false;
        m_stats = Stats{};
        m_planFires = m_planDrifts = m_planTicksSaved = 0;
        m_planTrust = false;
        m_liveUsSum = m_liveChecks = m_liveUsMax = 0;
        m_simStepsTotal = m_simUsTotal = 0;
        m_cheapClicks = 0;
        m_inputsSeen = m_hookMissing = 0;
        m_oneStep.clear();
        m_osPending = false;
        m_lastLiveAt = -1e9;
        m_last = ClickRecord{};
        m_tick = 0;
        m_lastDeathTick = -1;
        m_spots.clear();   // a new level: load what earlier sessions learned on it
        loadSpots(pl);
        m_outcome.clear();
        m_spotFires = m_spotBlocks = m_spotSaves = 0;
        m_p1LastT = -1.0;
        m_levelStart = Clock::now();
        m_frameDt = BASE_DT;
        m_dtIdx = 0;
        for (float& v : m_dtRing) v = 0.f;
        m_warp = 1.0f;
        m_gameTime = 0.0;
        m_haveFrame = false;
        m_lastFrameAt = Clock::time_point{};
        m_cbfLoaded = Loader::get()->isModLoaded("syzzi.click_between_frames");
        releaseOverlay();
        releaseBadge();
        m_history.clear();
        m_directFires = 0;
        for (auto& r : m_holdLen) r.clear();
        for (auto& r : m_gapLen) r.clear();
        m_lastPressAt = m_lastReleaseAt = -1.0;
        m_accByMode.clear();
        m_tracker = ibcore::PredictionTracker{};
        m_trackPending = false;
        invalidateCfg();
        loadPersisted(cfg());
        refreshFlags();
        onReset();
    }

    void quit() {
        if ((m_logDebug || m_logCsv) && m_stats.clicks > 0) {
            std::string text = summary();
            log::info("[InputBuffer] session: {}", text);
            if (m_logCsv) {
                m_csv.row(fmt::format("SUMMARY,{:.1f},,,,,,,,,,,\"{}\"", levelMs(), text));
            }
        }
        m_csv.close();
        m_outcome.clear(); // leaving: the last input is neither proven nor blamed
        saveSpots();
        m_spotFile.clear();
        savePersisted();
        releaseSimPlayers();
        releaseOverlay();
        releaseBadge();
        m_simulating = false;
        m_firingBuffered = false;
        onReset();
    }

    // Level restart / respawn: drop anything pending so the new attempt is clean.
    // ------------------------------------------------------------------------
    //  Orb press check (1.24.3)
    // ------------------------------------------------------------------------
    // "1,2.5,3" -> numbers (saved values only; no sscanf - MSVC flags it).
    template <class... T>
    static bool parseNums(std::string const& v, char sep, T&... out) {
        std::vector<std::string> parts;
        size_t pos = 0;
        while (true) {
            size_t const e = v.find(sep, pos);
            parts.push_back(v.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
            if (e == std::string::npos) break;
            pos = e + 1;
        }
        if (parts.size() != sizeof...(T)) return false;
        size_t i = 0;
        bool ok = true;
        auto one = [&](auto& x) {
            char const* b = parts[i].c_str();
            char* end = nullptr;
            double const d = std::strtod(b, &end);
            if (end == b) ok = false;
            x = static_cast<std::remove_reference_t<decltype(x)>>(d);
            i++;
        };
        (one(out), ...);
        return ok;
    }

    void orbModelLoad() {
        if (m_orbModelLoaded) return;
        m_orbModelLoaded = true;
        orbTableLoad();
        std::string const v = Mod::get()->getSavedValue<std::string>("orb-model-v2", "");
        double e[ORB_MODELS] = {0, 0, 0};
        int n = 0;
        if (parseNums(v, ',', n, e[0], e[1], e[2]) && n > 0) {
            m_orbCalN = n;
            for (int k = 0; k < ORB_MODELS; k++) m_orbCalErr[k] = e[k];
            m_orbModel = bestOrbModel();
        }
    }
    int bestOrbModel() const {
        // GD's own order unless another model is clearly closer (0.5px on average).
        int best = 1;
        double const margin = 0.5 * std::max(1, m_orbCalN);
        for (int k : {0, 2}) if (m_orbCalErr[k] < m_orbCalErr[best] - margin) best = k;
        return best;
    }
    static char const* orbModelName(int k) {
        switch (k) {
            case 0:  return "orb first, no flap";
            case 1:  return "flap, then orb";
            default: return "press + orb together";
        }
    }

    // A UFO / swing press the prediction says uses an orb: record where each
    // model says you'll be (only used if the press goes in as you made it).
    template <class Sim>
    void orbCalPrepare(bool push, ModeInfo const& mode, SimResult const& r0, Sim&& simH) {
        m_orbCalReady = false;
        orbModelLoad();
        (void)r0;
        if (!push || !(mode.ufo || mode.swing) || !m_orbPredict || m_orbCalArmed) return;
        int const keep = m_orbModel;
        bool const rec = m_recordPath;
        m_recordPath = true;
        bool const dl = m_hasDeadline;
        m_hasDeadline = false;
        int const steps = ORB_CAL_STEPS + 2;
        for (int k = 0; k < ORB_MODELS; k++) {
            m_orbModel = k;
            simH(steps);
            m_orbCalPath[k] = m_path;
            m_calOrbStep[k] = m_simOrbStep;
            m_calOrbVy[k] = m_simOrbVy;
        }
        m_orbModel = keep;
        m_recordPath = rec;
        m_hasDeadline = dl;
        float const dt = simDt();
        m_orbCalT0 = m_gameTime - static_cast<double>(tickPhase()) * dt;
        m_orbCalReady = true;
    }
    void orbCalArm() {
        if (!m_orbCalReady) return;
        m_orbCalReady = false;
        float const dt = simDt();
        for (int k = 0; k < ORB_MODELS; k++) {
            m_orbTr[k].start(m_orbCalT0, dt, m_orbCalPath[k], ORB_CAL_STEPS - 1, 2.f);
        }
        m_orbCalArmed = m_orbTr[0].active();
        m_orbCalReal = false;
        m_realOrbT = -1.0;
        m_realOrbCalls = 0;
    }
    void orbCalDrop() {
        for (auto& t : m_orbTr) t.invalidate();
        m_orbCalArmed = false;
        m_orbCalReady = false;
    }
    // orb type | gamemode | mini | upside down (before the orb)
    static int orbKey(RingObject* r, PlayerObject* p) {
        int m = 0;
        if (p->m_isBird) m = 1;
        else if (p->m_isSwing) m = 2;
        else if (p->m_isBall) m = 3;
        else if (p->m_isRobot) m = 4;
        else if (p->m_isSpider) m = 5;
        else if (p->m_isShip) m = 6;
        else if (p->m_isDart) m = 7;
        return static_cast<int>(r->m_objectType) * 64 + m * 4 + (p->m_vehicleSize < 0.99f ? 2 : 0) +
               (p->m_isUpsideDown ? 1 : 0);
    }
    static std::string orbKeyName(int key) {
        static char const* modes[8] = {"cube", "ufo", "swing", "ball", "robot", "spider", "ship", "wave"};
        int const type = key / 64, m = (key / 4) % 16;
        return fmt::format("{} ({}{}{})", objectName(static_cast<GameObjectType>(type)), modes[std::clamp(m, 0, 7)],
                           (key & 2) ? ", mini" : "", (key & 1) ? ", upside down" : "");
    }
    void orbTableLoad() {
        std::string const v = Mod::get()->getSavedValue<std::string>("orb-table-v1", "");
        size_t pos = 0;
        while (pos < v.size()) {
            size_t const end = v.find(';', pos);
            std::string const item = v.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            int key = 0, n = 0, flips = 0;
            double sum = 0.0;
            if (parseNums(item, ':', key, sum, n, flips) && n > 0) {
                m_orbTable[key] = OrbFact{sum, n, flips};
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
    }
    void orbTableSave() {
        if (!m_orbTableDirty) return;
        m_orbTableDirty = false;
        std::string v;
        for (auto const& [k, f] : m_orbTable) v += fmt::format("{}:{:.4f}:{}:{};", k, f.vySum, f.n, f.flips);
        Mod::get()->setSavedValue<std::string>("orb-table-v1", v);
    }
    // Every orb a sim player uses goes through here: GD's own argument, the
    // learned y-speed correction, and a record for the orb check.
    void simRingJump(PlayerObject* p, RingObject* r) {
        double const before = p->m_yVelocity;
        int const key = orbKey(r, p);
        bool const udBefore = p->m_isUpsideDown;
        // No GD ringJump for sim players (1.26.2): it drew the orb's circle effect
        // where the prediction was (a ghost "random orb" next to the real one),
        // and it never moved the sim player anyway (measured in 1.24.5: y-speed
        // stayed a plain flap). The learned orb below is what the game does.
        // Learned from the game: set exactly what this orb really does.
        // Exact entry, else the same orb at the other size, else upside down
        // (speed mirrored) - better than the sim's own orb, which does nothing.
        OrbFact const* fp = nullptr;
        double sign = 1.0;
        for (int alt : {0, 2, 1, 3}) {
            auto it = m_orbTable.find(key ^ alt);
            if (it != m_orbTable.end() && it->second.n > 0) {
                fp = &it->second;
                sign = (alt & 1) ? -1.0 : 1.0;
                break;
            }
        }
        if (fp) {
            OrbFact const& f = *fp;
            bool const flip = f.flips * 2 > f.n;
            bool const want = flip ? !udBefore : udBefore;
            if (p->m_isUpsideDown != want) p->flipGravity(want, true);
            p->m_yVelocity = sign * f.vySum / f.n;
            p->m_isOnGround = false;
            m_orbTableUses++;
        } else if (r->m_objectType == GameObjectType::GravityRing || r->m_objectType == GameObjectType::GreenRing) {
            // Not learned yet (the first real use teaches it): at least the
            // gravity flip blue / green orbs always do.
            p->flipGravity(!udBefore, true);
        }
        if (p == m_simPlayer1.data() && m_simOrbStep < 0) {
            m_simOrbStep = m_simTick;
            m_simOrbVyBefore = before;
            m_simOrbVy = p->m_yVelocity;
        }
    }
    void noteRealOrb(PlayerObject* p, RingObject* r, bool arg, double before, double after, int key, bool flipped) {
        orbModelLoad();
        if (r && !realUsedOrb(r)) m_realUsedOrbs.push_back(r);
        // Learn what this orb really does (every real orb use counts).
        if (r && isModelledOrb(r->m_objectType) && std::isfinite(after)) {
            OrbFact& f = m_orbTable[key];
            bool const fresh = f.n == 0;
            double const avg = fresh ? after : f.vySum / f.n;
            if (!fresh && std::fabs(after - avg) > std::max(1.0, std::fabs(avg) * 0.25)) {
                // Very different from before (another orb setting / speed): start over.
                f = OrbFact{};
            }
            if (f.n < 50) {
                f.vySum += after;
                f.n++;
                if (flipped) f.flips++;
            }
            m_orbTableDirty = true;
            if (fresh && cfg().hudVerbose) pushLog(fmt::format("learned: {} gives y-speed {:.2f}{}", orbKeyName(key), after,
                                           flipped ? " and flips gravity" : ""), COL_GREEN, false);
        }
        (void)arg; // (the sim no longer calls ringJump: 1.26.2)
        if (!m_orbCalArmed) return;
        PlayLayer* pl = PlayLayer::get();
        if (!pl || p != pl->m_player1) return;
        m_orbCalReal = true;
        m_realOrbCalls++;
        if (m_realOrbT < 0.0) {
            m_realOrbT = m_gameTime;
            m_realOrbVyBefore = before;
            m_realOrbVy = after;
            m_realOrbType = r ? static_cast<int>(r->m_objectType) : -1;
            m_realOrbMini = p->m_vehicleSize < 0.99f;
        }
    }
    void orbCalPoll(PlayerObject* p) {
        if (!m_orbCalArmed || !p) return;
        if (p->m_isDead) { orbCalDrop(); return; }
        float err[ORB_MODELS];
        for (int k = 0; k < ORB_MODELS; k++) {
            auto r = m_orbTr[k].poll(m_gameTime, p->getPositionX(), p->getPositionY());
            if (!r) return; // not there yet (all start together)
            err[k] = r->err;
        }
        m_orbCalArmed = false;
        if (!m_orbCalReal) return; // you didn't actually use an orb: says nothing
        m_orbCalN++;
        for (int k = 0; k < ORB_MODELS; k++) m_orbCalErr[k] += std::min(err[k], 40.f);
        int const before = m_orbModel;
        m_orbModel = bestOrbModel();
        Mod::get()->setSavedValue<std::string>("orb-model-v2",
            fmt::format("{},{:.3f},{:.3f},{:.3f}", m_orbCalN, m_orbCalErr[0], m_orbCalErr[1], m_orbCalErr[2]));
        // Logged only when something's off (1.25.2: these were most of the log).
        bool const orbOff = err[m_orbModel] > 2.f || m_orbModel != before;
        if (orbOff) {
            pushLog(fmt::format("orb prediction {:.1f}px off ({} / {} / {}: {:.1f} / {:.1f} / {:.1f}px){}",
                                err[m_orbModel], orbModelName(0), orbModelName(1), orbModelName(2),
                                err[0], err[1], err[2], m_orbModel != before ? " - switched model" : ""),
                    COL_YELLOW, false);
        }
        // What the orb itself did: when (step after your click) and the y-speed
        // it gave, in the game vs in the prediction.
        float const dt = simDt();
        int const realStep = static_cast<int>(std::floor((m_realOrbT - m_orbCalT0) / dt + 1e-6));
        int const k = m_orbModel;
        if (orbOff && (realStep != m_calOrbStep[k] || std::fabs(m_realOrbVy - m_calOrbVy[k]) > 0.05))
        pushLog(fmt::format("orb jump [{}{}]: game used it on tick +{} -> y-speed {:.2f} (was {:.2f}, {} call{}) | "
                            "predicted tick +{} -> y-speed {:.2f}",
                            m_realOrbType >= 0 ? objectName(static_cast<GameObjectType>(m_realOrbType)) : "orb",
                            m_realOrbMini ? ", mini" : "", realStep, m_realOrbVy, m_realOrbVyBefore, m_realOrbCalls,
                            m_realOrbCalls == 1 ? "" : "s", m_calOrbStep[k], m_calOrbVy[k]),
                COL_YELLOW, false);
        orbTableSave();
    }

    void onReset() {
        m_deathReported = false;
        clearBuffer();
        m_lastPressAt = m_lastReleaseAt = -1.0; // a respawn breaks the rhythm
        m_tracker.invalidate();
        m_realUsedOrbs.clear();
        orbCalDrop();
        orbTableSave();
        m_trackPending = false;
        m_physicalHolding = false;
        m_gameHolding = false;
        m_lastInputTime = -1e9;
        m_streamLast = -1e9;
        m_streamShift = 0.0;
        m_streak = 0;
        m_movers.clear(); // positions from before a respawn mean nothing now
        m_outcome.clear(); // respawned: the last input is settled (death already counted)
        m_p1LastT = -1.0;
        m_liveWaited = false;
    }

    bool isSimulating() const { return m_simulating; }
    // Real player hit a pad: remember the strength GD used for that pad type.
    void learnPad(int type, float bump) {
        if (type < 0 || type >= 64) return;
        PadLearn& p = m_pads[type];
        if (!p.seen) {
            p.seen = true;
            p.bump = bump;
            m_padsDirty = true;
        } else if (std::fabs(p.bump - bump) > 1e-3f && !p.unstable) {
            p.unstable = true;
            m_padsDirty = true;
        }
    }

    // Player and orb hitboxes overlap right now (with a little slack: GD's orb
    // area is a bit more generous than the box).
    static bool overlapsNow(PlayerObject* p, GameObject* o) {
        if (!p || !o) return false;
        CCRect const& b = o->getObjectRect();
        if (b.size.width < 1.f || b.size.height < 1.f || b.size.width > 260.f || b.size.height > 520.f)
            return true; // no usable box: trust GD's contact
        // the player's box from its position (its cached rect can be a step old)
        float const ph = 15.f * std::clamp(p->m_vehicleSize, 0.3f, 1.3f);
        float const px = p->getPositionX(), py = p->getPositionY();
        float const pad = 4.f;
        return px - ph - pad <= b.origin.x + b.size.width && b.origin.x - pad <= px + ph &&
               py - ph - pad <= b.origin.y + b.size.height && b.origin.y - pad <= py + ph;
    }

    bool simOrbAvailable(RingObject* r) const {
        if (!r || realUsedOrb(r) || static_cast<GameObject*>(r) == m_simOrbObj) return false;
        return r->isVisible() && !r->m_isGroupDisabled && !r->m_isDisabled;
    }

    void snapshotObj(GameObject* o) {
        for (auto const& e : m_objRestore) if (e.first == o) return;
        m_objRestore.emplace_back(o, o->m_isActivated);
    }

    bool realUsedOrb(GameObject* o) const {
        return std::find(m_realUsedOrbs.begin(), m_realUsedOrbs.end(), o) != m_realUsedOrbs.end();
    }

    // GD lists this orb as touched by the player right now.
    static bool touchingOrbObj(PlayerObject* p, RingObject* r) {
        if (!p || !r || !p->m_touchingRings) return false;
        unsigned const n = p->m_touchingRings->count();
        for (unsigned i = 0; i < n; i++) {
            if (p->m_touchingRings->objectAtIndex(i) == r) return true;
        }
        return false;
    }

    // The modelled orb a (real) player is touching right now, if any.
    static RingObject* touchingOrb(PlayerObject* p) {
        if (!p || !p->m_touchingRings) return nullptr;
        unsigned const n = p->m_touchingRings->count();
        for (unsigned i = 0; i < n; i++) {
            auto* r = typeinfo_cast<RingObject*>(p->m_touchingRings->objectAtIndex(i));
            if (r && isModelledOrb(r->m_objectType)) return r;
        }
        return nullptr;
    }

    // UFO / swing press while already touching an orb: GD uses the orb for that
    // press on the same tick, and there's no flap (1.24.1). The sim used to
    // flap first and use the orb a tick later, starting the orb arc late and
    // high - "no fix ... hits object at +22px, after using an orb".
    bool orbPress(PlayerObject* s1, PlayerObject* s2) {
        if (!m_orbPredict || m_simOrbUsed) return false;
        if (!(s1->m_isBird || s1->m_isSwing)) return false;
        RingObject* r = m_prevRing[0];
        if (!r || static_cast<GameObject*>(r) == m_simOrbObj) return false;
        if (realUsedOrb(r)) return false;     // you already used it this attempt
        if (!overlapsNow(s1, r)) return false; // a stale "touching" entry
        m_objRestore.emplace_back(r, r->m_isActivated);
        m_simOrbUsed = true;
        m_simOrbObj = r;
        simRingJump(s1, r);
        if (s2 && m_prevRing[1] && m_prevRing[1] != r) simRingJump(s2, m_prevRing[1]);
        m_simHeld = true;          // the button is down, but the press went into the orb
        m_simPressTick = m_simTick;
        return true;
    }

    // GD's collision says a sim player overlaps this orb (exact hitbox).
    void simRingTouched(PlayerObject* player, RingObject* ring) {
        int const idx = (player == m_simPlayer2.data()) ? 1 : 0;
        m_gdRings = true;
        if (idx == 0 && !m_simFirstRing && !realUsedOrb(ring) && isModelledOrb(ring->m_objectType)) {
            m_simFirstRing = ring;
            m_simFirstRingTick = m_simTick;
            m_simFirstRingLast = m_simTick;
        } else if (idx == 0 && ring == m_simFirstRing && m_simTick == m_simFirstRingLast + 1) {
            m_simFirstRingLast = m_simTick;
        }
        if (!m_tickRing[idx] && static_cast<GameObject*>(ring) != m_simOrbObj) m_tickRing[idx] = ring;
    }

    // After a sim player's collision step: use the orb GD reported, under the
    // same rules as before (pressed - a fresh click in UFO / swing - within the
    // click's intent window, one orb per test).
    void applyGdRing(PlayerObject* player, int idx) {
        RingObject* ring = m_tickRing[idx];
        m_tickRing[idx] = nullptr;
        if (!ring || m_simOrbUsed || !m_orbPredict || !isModelledOrb(ring->m_objectType)) return;
        bool const clickOnly = player->m_isBird || player->m_isSwing;
        bool const freshPress = m_simHeld && m_simPressTick >= 0 && m_simTick - m_simPressTick <= m_orbClick;
        bool const pressed = clickOnly ? freshPress : (m_simHeld || player->m_jumpBuffered);
        int const since = m_simTick - m_simDelay;
        if (!pressed || since < 0 || since > m_orbIntent) return;
        m_objRestore.emplace_back(ring, ring->m_isActivated);
        m_simOrbUsed = true;
        m_simOrbObj = ring;
        simRingJump(player, ring);
    }

    bool isSimPlayer(PlayerObject* p) const {
        return p && (p == m_simPlayer1.data() || p == m_simPlayer2.data());
    }
    bool isFiringBuffered() const { return m_firingBuffered; }

    // True if `player` is one of our sim players (swallow the death).
    // True if `player` is one of our sim players (their "death" is swallowed).
    bool checkSimulationDeath(PlayerObject* player, GameObject* killer = nullptr) {
        if (m_simulating && (player == m_simPlayer1.data() || player == m_simPlayer2.data())) {
            if (!m_simDead) {
                m_simKillType = killer ? static_cast<int>(killer->m_objectType) : -1;
                m_simKillMover = false;
                m_simKillDy = player->getPositionY() - m_simStartY;
            }
            m_simDead = true;
            return true;
        }
        return false;
    }

    // The real player really died (called once per death; see the hook and
    // onPlayerUpdate). GD can call destroyPlayer every tick while touching a
    // hazard (practice mode, noclip-style mods), which is not a death.
    void onRealDeath() {
        if (m_deathReported) return;
        m_deathReported = true;
        if (cfg().learnSpots) {
            finishOutcome(m_outcome.death(m_gameTime, m_outcomeWindow));
            saveSpots(); // a death is a natural pause: write what was learned
        }
        if (m_tracker.active()) {
            AccCount& a = m_accByMode[m_trackMode];
            a.checks++;
            a.deaths++;
        }
        m_tracker.died(); // a pending check predicted you'd be alive
        reportDeath();
        onReset();
        m_deathReported = true; // onReset clears it; stay reported until respawn
    }

    // Called from the collision hook while simulating: only solid/hazard-type
    // objects are collided with; state-changing objects may end the lookahead
    // (see checkStateChange for exactly when).
    gd::vector<GameObject*>& filterForSim(PlayerObject* player, gd::vector<GameObject*>* vec, int count) {
        m_scratch.clear();
        if (!vec) return m_scratch;
        int const n = std::min<int>(count, static_cast<int>(vec->size()));
        SimTrack const& tr = m_track[(player == m_simPlayer2.data()) ? 1 : 0];
        for (int i = 0; i < n; i++) {
            GameObject* obj = (*vec)[i];
            if (!obj) continue;
            switch (classify(obj->m_objectType)) {
                case ObjClass::Collide:
                    if ((m_simModern || m_simMovers) && isHazard(obj->m_objectType)) {
                        // Hazards: measure how close the path comes (clearance) and,
                        // if it moves, test it where it will be instead of where it is.
                        if (hazardForSim(player, obj)) continue; // handled here, not by GD
                    }
                    m_scratch.push_back(obj);
                    break;
                case ObjClass::StateChange:
                    // Orbs the sim can use go to GD's own collision as well, which
                    // reports the exact tick the player overlaps them (its real
                    // orb hitbox) through playerTouchedRing (1.23.0).
                    if (m_orbPredict && isModelledOrb(obj->m_objectType) && obj != m_simOrbObj) {
                        // GD marks an orb activated as soon as a player TOUCHES it.
                        // Remember its state before GD sees it, so the test leaves
                        // it exactly as it was (1.25.0: it used to be "restored"
                        // to activated, and every later test ignored that orb).
                        snapshotObj(obj);
                        m_scratch.push_back(obj);
                    }
                    if (!m_simTrunc.valid) checkStateChange(player, obj, tr);
                    break;
                default:
                    break;
            }
        }
        return m_scratch;
    }

    static bool isHazard(GameObjectType t) {
        return t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard;
    }

    static ibcore::Box toBox(CCRect const& r) {
        return ibcore::Box{r.origin.x, r.origin.y, r.size.width, r.size.height};
    }

    // A moving hazard whose position we may predict ourselves: visible, not
    // toggled off, not a no-touch object, axis-aligned (its box is exact).
    // Anything else stays with GD's own test at its current position.
    static bool predictableMover(GameObject* obj) {
        if (!obj->isVisible() || obj->m_isGroupDisabled || obj->m_isDisabled || obj->m_isNoTouch) return false;
        float const rot = std::fmod(std::fabs(obj->getRotation()), 90.f);
        return rot < 0.01f || rot > 89.99f;
    }

    // One hazard in one sim step. Returns true if the hazard was tested here
    // (a mover at its predicted position) and must not go to GD's check.
    bool hazardForSim(PlayerObject* player, GameObject* obj) {
        float dx = 0.f, dy = 0.f;
        bool moved = false;
        if (m_simMovers && !m_movers.empty()) {
            auto it = m_movers.find(obj);
            if (it != m_movers.end() && it->second.moving(m_simNow, MOVER_FRESH, MOVER_MIN_SPEED) && predictableMover(obj)) {
                it->second.offsetAt(m_simNow + static_cast<double>(m_simTick + 1) * m_simDtCur, dx, dy);
                moved = true;
            }
        }
        if (!moved && !m_simModern) return false; // static hazard: GD's own test (no clearance outside mini cube)
        ibcore::Box hb = toBox(obj->getObjectRect());
        hb.x += dx;
        hb.y += dy;
        ibcore::Box const pb = toBox(player->getObjectRect());
        float const gap = ibcore::boxGap(pb, hb);
        if (!moved) {
            if (gap >= 0.f) m_tickClr = std::min(m_tickClr, gap);
            return false;
        }
        m_simMover = true;
        if (gap < 0.f && !m_simDead) {      // touches it where it will be
            m_simDead = true;
            m_simKillType = static_cast<int>(obj->m_objectType);
            m_simKillMover = true;
            m_simKillDy = player->getPositionY() - m_simStartY;
        }
        else m_tickClr = std::min(m_tickClr, gap);
        return true;
    }

    // Real game, P1's collision step: sample every hazard near you so moving
    // ones get a velocity. Pointers are only used as keys.
    void sampleMovers(PlayLayer* pl, PlayerObject* player, gd::vector<GameObject*>* vec, int count) {
        // Off since 1.26.0: it logged every nearby spike / orb on every physics
        // step (240x a second) and in practice never predicted anything
        // ("moving hazards: 0 tests predicted them") - pure CPU each frame.
        if (!MOVERS_ON) return;
        if (!m_enabled || !vec || !pl || player != pl->m_player1) return;
        // Only the current engine (mini cube) uses moving hazards: skip the
        // per-step bookkeeping everywhere else.
        {
            ModeInfo m;
            m.add(pl->m_player1);
            if (m.holdBased()) return; // ship / wave don't use it
        }
        int const n = std::min<int>(count, static_cast<int>(vec->size()));
        double const t = m_gameTime;
        for (int i = 0; i < n; i++) {
            GameObject* obj = (*vec)[i];
            if (!obj || !(isHazard(obj->m_objectType) || isOrb(obj->m_objectType))) continue;
            m_movers[obj].sample(obj->m_positionX, obj->m_positionY, t, MOVER_MAX_GAP, MOVER_TELEPORT);
        }
        if (m_movers.size() > MOVER_MAX_TRACKED) {
            for (auto it = m_movers.begin(); it != m_movers.end();) {
                if (t - it->second.t > MOVER_MAX_GAP) it = m_movers.erase(it);
                else ++it;
            }
            if (m_movers.size() > MOVER_MAX_TRACKED) m_movers.clear();
        }
    }

    // ---- spot memory helpers ---------------------------------------------------
    static uint32_t spotKey(std::string const& modeName, bool push, bool upsideDown) {
        uint32_t h = 2166136261u; // FNV-1a
        for (char ch : modeName) { h ^= static_cast<uint8_t>(ch); h *= 16777619u; }
        h ^= (push ? 1u : 0u) | (upsideDown ? 2u : 0u);
        h *= 16777619u;
        return h;
    }

    // Match tolerance: x about half a tick of travel.
    ibcore::SpotTol spotTol() const {
        ibcore::SpotTol t;
        t.x = std::clamp(0.5f * m_p1Speed * getValidDt(), 0.3f, 4.f);
        return t;
    }

    ibcore::Spot spotNow(PlayLayer* pl, bool push, std::string const& modeName) const {
        PlayerObject* p = pl->m_player1;
        ibcore::Spot s;
        s.x = p->getPositionX();
        s.y = p->getPositionY();
        s.vy = static_cast<float>(p->m_yVelocity);
        s.key = spotKey(modeName, push, p->m_isUpsideDown);
        s.anyKey = spotKey("any", push, p->m_isUpsideDown); // same press, any gamemode
        return s;
    }

    void finishOutcome(std::optional<ibcore::OutcomeTracker::Done> const& d) {
        if (!d) return;
        m_spots.add(d->spot, d->good, spotTol());
        m_spotsDirty = true;
    }

    // ---- spot memory on disk (one small text file per level) -------------------
    void loadSpots(PlayLayer* pl) {
        namespace fs = std::filesystem;
        m_spotFile.clear();
        m_spotsLoaded = 0;
        m_spotsDirty = false;
        GJGameLevel* lvl = pl ? pl->m_level : nullptr;
        if (!lvl) return;
        int const id = lvl->m_levelID.value();
        std::string const name = lvl->m_levelName;
        m_spotLevelName = name;
        std::string file;
        if (id > 0) {
            file = fmt::format("online-{}.txt", id);
        } else {
            // local / editor levels have no id: name + a hash so renames don't collide
            uint32_t h = 2166136261u;
            for (char ch : name) { h ^= static_cast<uint8_t>(ch); h *= 16777619u; }
            file = fmt::format("local-{:08x}.txt", h);
        }
        std::error_code ec;
        fs::path const dir = Mod::get()->getSaveDir() / "spots";
        fs::create_directories(dir, ec);
        m_spotFile = dir / file;
        std::ifstream in(m_spotFile, std::ios::binary);
        if (!in) return;
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        m_spots.deserialize(text);
        // Learn from attempts is mini cube only: drop spots older versions saved
        // for UFO and other modes (1.21-1.24.0), so nothing outside mini cube can
        // ever use them (1.25.9).
        {
            uint32_t ok[4];
            int n = 0;
            for (bool push : {true, false})
                for (bool ud : {false, true}) ok[n++] = spotKey("mini cube", push, ud);
            size_t const gone = m_spots.keepIf([&](ibcore::Spot const& sp) {
                return sp.key == ok[0] || sp.key == ok[1] || sp.key == ok[2] || sp.key == ok[3];
            });
            if (gone > 0) m_spotsDirty = true; // rewrite the file without them
        }
        m_spotsLoaded = m_spots.size();
    }

    void saveSpots() {
        if (!m_spotsDirty || m_spotFile.empty()) return;
        m_spotsDirty = false;
        std::ofstream out(m_spotFile, std::ios::binary | std::ios::trunc);
        if (out) out << m_spots.serialize();
    }

public:
    // Stage tab "Forget this level": wipe what was learned here (and its file).
    void forgetSpots() {
        m_spots.clear();
        m_outcome.clear();
        m_spotsLoaded = 0;
        m_spotsDirty = false;
        if (!m_spotFile.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_spotFile, ec);
        }
        Notification::create("Input Buffer - forgot what it learned on this level", NotificationIcon::None, 1.5f)->show();
    }
    bool inLevelWithSpots() const { return !m_spotFile.empty(); }

    // An input reached the game (raw or buffered): it is now the one that gets
    // the credit or the blame.
    void rememberInput(PlayLayer* pl, bool push) {
        if (!cfg().learnSpots || !pl || !pl->m_player1 || pl->m_player1->m_isDead) return;
        ModeInfo mode;
        mode.add(pl->m_player1);
        if (!modernEngine(mode)) return; // learn from attempts: mini cube only (UFO off again in 1.24.1)
        // Presses only. A death in ship / wave gets blamed on whatever input came
        // last, which is often a release that wasn't the problem; learned
        // "bad releases" then held your good releases back.
        if (!push) return;
        // A cube-type press in the air doesn't jump (until you land): it isn't a
        // spot to repeat. (Orb clicks in the air are left out too, on purpose.)
        bool const cubeOnly = mode.cubeLike && !(mode.ship || mode.wave || mode.ufo || mode.swing || mode.robot);
        if (push && cubeOnly && !pl->m_player1->m_isOnGround) return;
        finishOutcome(m_outcome.input(spotNow(pl, push, mode.name()), m_gameTime));
        m_outcomeLand = !mode.noLanding();
        m_outcomeWindow = m_outcomeLand ? SPOT_WINDOW_LAND_MAX : SPOT_WINDOW_HOLD;
        m_outcomeAir = false;
        m_outcomeLandAt = -1.0;
    }

    // Every real P1 step: speed for the tolerances, and settle the pending input.
    void spotStep(PlayerObject* p) {
        float const x = p->getPositionX();
        if (m_p1LastT >= 0.0) {
            double const gap = m_gameTime - m_p1LastT;
            if (gap > 0.05 && gap < 3.0) {
                float const v = static_cast<float>((x - m_p1LastX) / gap);
                if (std::fabs(v) < 200.f) {
                    m_p1Speed = std::fabs(v);
                    if (std::fabs(v) > 0.1f) m_p1Dir = v >= 0.f ? 1.f : -1.f;
                }
                m_p1LastX = x;
                m_p1LastT = m_gameTime;
            }
        } else {
            m_p1LastX = x;
            m_p1LastT = m_gameTime;
        }
        if (!m_outcome.pending()) return;
        if (m_outcomeLand) {
            if (!p->m_isOnGround) { m_outcomeAir = true; m_outcomeLandAt = -1.0; }
            else if (m_outcomeAir && m_outcomeLandAt < 0.0) m_outcomeLandAt = m_gameTime;
            if (m_outcomeLandAt >= 0.0 && m_gameTime - m_outcomeLandAt > SPOT_LAND_GRACE) {
                finishOutcome(m_outcome.poll(m_gameTime, -1.0)); // landed and stayed alive: good
                return;
            }
        }
        finishOutcome(m_outcome.poll(m_gameTime, m_outcomeWindow));
    }

    // How much room this gamemode's paths need (px), from its measured error.
    float clearTarget(std::string const& mode) const {
        auto it = m_accByMode.find(mode);
        if (it == m_accByMode.end()) return CLEAR_DEFAULT_PX;
        return ibcore::clearanceTarget(it->second.checks, it->second.errSum, CLEAR_MIN_CHECKS,
                                       CLEAR_DEFAULT_PX, CLEAR_MIN_PX, CLEAR_MAX_PX, CLEAR_ERR_SCALE);
    }

    // Called after every real P1 update (full tick or CBF sub-step) with its length.
    void setFrameDelta(PlayLayer* pl, float dt) {
        float const warp = pl->m_gameState.m_timeWarp;
        if (warp > 0.01f && warp < 10.f) m_warp = warp;
        float const d = (warp > 0.f) ? dt / warp : dt;
        if (!(d > 0.f && d < 2.0f)) return;
        m_lastStepD = d;
        m_gameTime += d; // every step counts, however short
        // Where we are inside the current physics tick (1.23.3). Click Between
        // Frames splits a tick at your click; the pieces add up to one tick.
        {
            float const nom = getValidDt();
            m_stepAccum += d;
            if (m_stepAccum >= nom * 0.98f) m_stepAccum -= nom;
            if (m_stepAccum < nom * 0.02f || m_stepAccum < 0.f) m_stepAccum = 0.f;
            m_phase = std::clamp(m_stepAccum / nom, 0.f, 1.f);
        }
        if (d < 0.0005f) return; // too short to say anything about the nominal tick
        m_dtRing[m_dtIdx] = d;
        m_dtIdx = (m_dtIdx + 1) % DT_RING;
        // Click-Between-Frames can insert shorter sub-steps; the nominal tick is the longest recent one.
        float mx = 0.f;
        for (float v : m_dtRing) mx = std::max(mx, v);
        m_frameDt = mx;
    }

    // Called once per rendered frame: smoothed wall-clock frame time for the auto budget.
    void onFrame() {
        auto const now = Clock::now();
        if (m_haveFrame || m_lastFrameAt.time_since_epoch().count() != 0) {
            float const us = std::chrono::duration<float, std::micro>(now - m_lastFrameAt).count();
            // Ignore pauses, loading hitches and the like.
            if (us > 500.f && us < 100000.f) {
                m_frameUs = m_haveFrame ? (m_frameUs * 0.95f + us * 0.05f) : us;
                m_haveFrame = true;
            }
        }
        m_lastFrameAt = now;
    }

    // ------------------------------------------------------------------------
    //  Hotkeys
    // ------------------------------------------------------------------------
    // Set the stage (0 = off, 1-4, 5 = extreme). Written into the setting, so
    // the settings screen, F6 and the window always agree.
    void setStage(int stage) {
        stage = std::clamp(stage, 0, STAGE_COUNT - 1);
        Mod::get()->setSettingValue<std::string>("stage", STAGES[stage].setting);
        invalidateCfg();
        refreshFlags();
        clearBuffer();
        Notification::create(fmt::format("Input Buffer - {}", STAGES[stage].name), NotificationIcon::None, 1.2f)->show();
        log::info("[InputBuffer] {}", STAGES[stage].name);
        refreshHud(PlayLayer::get(), true);
    }

    int currentStage() { return cfg().stage; }

    // Cycle key (F6): Stage 1 -> 2 -> 3 -> 4 -> Extreme -> Custom -> Off -> Stage 1.
    // Reads the stage directly, so it can never lose track of where it is.
    void cyclePreset() {
        int const cur = Config::load().stage;
        int const next = (cur == STAGE_COUNT - 1) ? 0 : cur + 1;
        setStage(next);
    }

    // ------------------------------------------------------------------------
    //  Data for the window (BufferWindow)
    // ------------------------------------------------------------------------
    using Row = std::pair<std::string, ccColor3B>;

    // Newest first. Safe passes / irrelevant releases only if `showMinor`.
    std::vector<Row> historyRows(bool showMinor) const {
        std::vector<Row> rows;
        rows.reserve(m_history.size() + 1);
        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
            if (it->minor && !showMinor) continue;
            // '\x02' time '\x1f' tag '\x1f' text: the window draws three columns.
            std::string line = fmt::format("\x02{:.1f}s\x1f{}\x1f{}", it->t, it->tag, it->text);
            if (it->repeat > 1) line += fmt::format("  x{}", it->repeat);
            rows.emplace_back(std::move(line), it->color);
        }
        return rows; // empty: the window shows what will appear here
    }

    void clearHistory() { m_history.clear(); m_logVersion++; }
    // Latest Log line (for the in-game pill).
    bool lastLog(std::string& tag, std::string& text, ccColor3B& color) const {
        if (m_history.empty()) return false;
        HistEntry const& e = m_history.back();
        tag = e.tag;
        text = e.text;
        color = e.color;
        return true;
    }
    // Bumped on every log change: the window only rebuilds the Log when it moved.
    unsigned long long logVersion() const { return m_logVersion; }
    unsigned long long m_logVersion = 0;

    // One-line status for the window footer.
    std::string statusLine() {
        Config const& c = cfg();
        PlayLayer* pl = PlayLayer::get();
        std::string out = pl ? statusText(pl, c) : headerText(c);
        auto const& a = m_tracker.stats();
        if (a.checks > 0) out += fmt::format(" | pred {:.1f}%", a.percent());
        return out;
    }

    std::string headerLine() { return headerText(cfg()); }
    ccColor3B headerLineColor() { return headerColor(cfg()); }

    // Stats tab (1.25.8): short sections of "label<TAB>value" rows - the window
    // draws the label on the left (muted) and the value right-aligned in the
    // row's colour. Rows without a tab are section titles / notes. Everything
    // that's only for debugging is listed under DETAILS, shown with "Log: show
    // safe clicks" on.
    std::vector<Row> statsRows() {
        Config const& c = cfg();
        Config const e = effectiveCfg(c);
        auto const& st = m_stats;
        auto d = [&](Decision x) { return st.byDecision[static_cast<int>(x)]; };
        std::vector<Row> rows;
        // '\x01' first = a section title: the window draws it bold (bigFont) and white.
        auto head = [&](std::string const& t) {
            if (!rows.empty() && !rows.back().first.empty()) rows.emplace_back("", COL_GREY); // a gap between sections
            rows.emplace_back("\x01" + t, COL_WHITE);
        };
        auto kv = [&](std::string const& label, std::string const& value, ccColor3B col = COL_WHITE) {
            rows.emplace_back(label + "\t" + value, col);
        };
        auto note = [&](std::string const& t, ccColor3B col = COL_GREY) { rows.emplace_back("  " + t, col); };
        auto pctCol = [](double p) { return p >= 95.0 ? COL_GREEN : p >= 80.0 ? COL_YELLOW : COL_ORANGE; };

        PlayLayer* pl = PlayLayer::get();

        // ---- this level (its name)
        {
            std::string lvl;
            if (pl && pl->m_level) lvl = std::string(pl->m_level->m_levelName);
            if (lvl.empty()) lvl = m_spotLevelName;
            head(lvl.empty() ? std::string("This level") : lvl);
        }
        int const raw = d(Decision::PassTruncated) + d(Decision::PassNoFix) + d(Decision::PassSpam) + d(Decision::PassBudget);
        kv("Clicks", fmt::format("{}", st.clicks));
        kv("Moved to the right tick", fmt::format("{}", d(Decision::Buffered)), d(Decision::Buffered) ? COL_GREEN : COL_WHITE);
        kv("Already on time", fmt::format("{}", d(Decision::PassSafe) + d(Decision::PassEscapable)));
        kv("Went in as you made them", fmt::format("{}", raw));
        kv("Deaths", fmt::format("{}", st.deaths), st.deaths ? COL_WHITE : COL_GREY);
        {
            int best = -1, bestN = 0;
            for (int i = 0; i < static_cast<int>(DeathReason::Count); i++) {
                if (st.byDeath[i] > bestN) { bestN = st.byDeath[i]; best = i; }
            }
            if (best >= 0) kv("Most deaths", fmt::format("{} ({})", deathText(static_cast<DeathReason>(best)), bestN), COL_GREY);
        }
        if (m_hookMissing) {
            note(fmt::format("{} clicks passed raw: the physics hook isn't running", m_hookMissing), COL_ORANGE);
        } else if (m_inputsSeen == 0 && pl) {
            note("No clicks reached the buffer yet. If you've been clicking, another mod takes them first.");
        }

        // ---- prediction
        head("Prediction");
        auto const& a = m_tracker.stats();
        if (a.checks > 0) {
            kv("Right 100ms after a click", fmt::format("{:.0f}% of {}", a.percent(), a.checks), pctCol(a.percent()));
        } else {
            kv("Right 100ms after a click", c.accuracy ? "no checks yet" : "off", COL_GREY);
        }
        {
            long long n = 0, ex = 0;
            for (auto const& [name, o] : m_oneStep) { n += o.n; ex += o.exact; }
            if (n > 0) {
                double const p = 100.0 * static_cast<double>(ex) / static_cast<double>(n);
                kv("Exact after one tick", fmt::format("{:.0f}% of {}", p, n), pctCol(p));
            }
        }
        for (auto const& [name, ac] : m_accByMode) {
            if (ac.checks == 0) continue;
            double const pct = 100.0 * ac.hits / ac.checks;
            kv("  " + name, fmt::format("{:.0f}%, {:.1f}px off", pct, ac.errSum / ac.checks), pctCol(pct));
        }

        // ---- rhythm
        head("Your rhythm (60fps frames)");
        {
            bool any = false;
            for (int w : {W_CUBE, W_SHIP, W_WAVE}) {
                if (m_holdLen[w].count() == 0 && m_gapLen[w].count() == 0) continue;
                any = true;
                bool const learning = m_holdLen[w].count() < RHYTHM_MIN_SAMPLES;
                kv(fmt::format("  {}", winLabel(w)),
                   fmt::format("hold {:.1f}f, gap {:.1f}f{}", rhythm(m_holdLen[w], DEFAULT_TAP_FRAMES),
                               rhythm(m_gapLen[w], DEFAULT_GAP_FRAMES), learning ? " (learning)" : ""),
                   learning ? COL_GREY : COL_WHITE);
            }
            if (!any) note("learning from your clicks...");
        }

        // ---- windows
        head("Windows (how early a click can be)");
        for (int w = 0; w < W_COUNT; w++) {
            bool const autoOn = c.autoWindow && m_autoW[w] > 0.f;
            // What clicks really get: Extreme widens every window to at least 20
            // frames (12 for ship / wave) - this showed the base 8f / 5f (1.25.9).
            float f = e.window[w];
            if (c.extreme) {
                bool const holdW = w == W_SHIP || w == W_SHIP_REL || w == W_WAVE || w == W_WAVE_REL;
                f = std::max(f, static_cast<float>(holdW ? c.extremeHoldCap : c.extremeWindow) * BASE_DT);
            }
            kv(fmt::format("  {}", winLabel(w)), fmt::format("{:g}f{}", std::round(f * 100.f) / 100.f, autoOn ? " (auto)" : ""),
               autoOn ? COL_GREEN : COL_WHITE);
        }

        // ---- performance
        head("Performance");
        long long const avg = st.simClicks > 0 ? st.sumUs / st.simClicks : 0;
        kv("Prediction per click", fmt::format("avg {:.1f}ms, max {:.1f}ms", avg / 1000.0, st.maxUs / 1000.0));
        if (m_haveFrame) {
            float const frameMs = m_frameUs / 1000.f;
            float const worst = static_cast<float>(std::max<long long>(st.maxUs, m_liveUsMax)) / 1000.f;
            kv("Frame", fmt::format("{:.1f}ms ({} fps){}", frameMs, static_cast<int>(std::lround(1e6f / m_frameUs)),
                                     m_cbfLoaded ? ", CBF" : ""));
            kv("Worst click cost", fmt::format("{:.0f}% of a frame", frameMs > 0.f ? 100.f * worst / frameMs : 0.f),
               worst > frameMs * 0.5f ? COL_ORANGE : COL_GREEN);
        }
        if (m_simStepsTotal > 0) {
            double const perStep = static_cast<double>(m_simUsTotal) / static_cast<double>(m_simStepsTotal);
            kv("Level weight", perStep < 3.0 ? "light" : perStep < 8.0 ? "medium" : "heavy",
               perStep < 8.0 ? COL_WHITE : COL_ORANGE);
        }
        if (m_planFires + m_planDrifts > 0) {
            kv("Held clicks fired on plan", fmt::format("{} ({} re-checked)", m_planFires, m_planDrifts),
               m_planDrifts > m_planFires ? COL_YELLOW : COL_GREEN);
        }
        if (m_cheapClicks > 0) kv("Clicks finished with quick checks", fmt::format("{}", m_cheapClicks), COL_YELLOW);
        if (m_watchdogFires > 0) kv("Inputs released by the watchdog", fmt::format("{}", m_watchdogFires), COL_ORANGE);
        if (m_directFires > 0 || m_directMode) kv("Applied directly (another input mod)", fmt::format("{}", m_directFires), COL_YELLOW);

        // ---- learned spots (mini cube)
        if (c.learnSpots && m_spots.size() > 0) {
            head("Learn from attempts (mini cube)");
            kv("Spots on this level", fmt::format("{} good, {} bad", m_spots.goodCount(), m_spots.badCount()));
        }

        // ---- details (debugging), only with "Log: show safe clicks"
        if (!c.hudVerbose) {
            rows.emplace_back("", COL_GREY);
            note("More detail: turn on \"Log: show safe clicks\" (Stage tab).", COL_DIM);
            return rows;
        }
        rows.emplace_back("", COL_GREY);
        head("Details");
        for (auto const& [k, f] : m_orbTable) {
            if (f.n == 0) continue;
            kv("Orb learned: " + orbKeyName(k), fmt::format("{:.2f}{}", f.vySum / f.n, f.flips * 2 > f.n ? ", flips" : ""), COL_GREY);
        }
        kv("Raw: orb / portal cut", fmt::format("{}", d(Decision::PassTruncated)), COL_GREY);
        kv("Raw: no fix in the window", fmt::format("{}", d(Decision::PassNoFix)), COL_GREY);
        kv("Raw: spam guard / CPU", fmt::format("{} / {}", d(Decision::PassSpam), d(Decision::PassBudget)), COL_GREY);
        for (int i = 0; i < static_cast<int>(DeathReason::Count); i++) {
            if (st.byDeath[i] == 0) continue;
            kv(std::string("Death: ") + deathText(static_cast<DeathReason>(i)), fmt::format("{}", st.byDeath[i]), COL_GREY);
        }
        if (a.checks > 0) kv("Accuracy: died first / skipped", fmt::format("{} / {}", a.deaths, a.dropped), COL_GREY);
        for (auto const& [name, ac] : m_accByMode) {
            if (ac.checks == 0) continue;
            kv("Off by (" + name + ")", fmt::format("x {:+.1f}, y {:+.1f}px, room {:.1f}px", ac.dxSum / ac.checks,
                                                    ac.dySum / ac.checks, clearTarget(name)), COL_GREY);
        }
        for (auto const& [name, o] : m_oneStep) {
            if (o.n == 0) continue;
            double const pct = 100.0 * o.exact / o.n;
            kv("One tick (" + name + ")", fmt::format("{:.0f}% of {}, worst {:.1f}px", pct, o.n, o.worst), pctCol(pct));
        }
        kv("Orb contact from the game", m_gdRings ? "yes" : "not seen yet", COL_GREY);
        if (m_orbCalN > 0) {
            kv("Orb press model", fmt::format("{} ({:.1f}px)", orbModelName(m_orbModel), m_orbCalErr[m_orbModel] / m_orbCalN), COL_GREY);
        }
        kv("Size portals in predictions", fmt::format("{}", m_simSizeSwitches), COL_GREY);
        kv("Near misses avoided", fmt::format("{}", m_cleanWaits), COL_GREY);
        kv("Moving hazards", fmt::format("{} tracked, {} hit", m_movers.size(), m_moverDeaths), COL_GREY);
        if (m_liveChecks > 0) {
            kv("Held-click re-checks", fmt::format("{}, avg {:.1f}ms", m_liveChecks, (m_liveUsSum / m_liveChecks) / 1000.0), COL_GREY);
        }
        if (c.learnSpots && !m_spotFile.empty()) {
            kv("Spots file", fmt::format("spots/{}", m_spotFile.filename().string()), COL_GREY);
        }
        for (int t = 0; t < 64; t++) {
            if (!m_pads[t].seen) continue;
            kv(fmt::format("Pad: {}", objectName(static_cast<GameObjectType>(t))),
               m_pads[t].unstable ? "varies (not predicted)" : "learned", m_pads[t].unstable ? COL_ORANGE : COL_GREY);
        }
        return rows;
    }

    // Clear learned timing now (menu button).
    void resetCalibration() {
        auto* mod = Mod::get();
        for (int i = 0; i < W_COUNT; i++) {
            m_calib[i].clear();
            m_autoW[i] = 0.f;
            mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], "");
        }
        m_calibDirty = false;
        Notification::create("Input Buffer - calibration cleared", NotificationIcon::None, 1.0f)->show();
    }

    // A setting was changed from the menu.
    void settingsChanged() {
        invalidateCfg();
        refreshFlags();
        clearBuffer();
        if (PlayLayer* pl = PlayLayer::get()) refreshHud(pl, true);
    }

    // ------------------------------------------------------------------------
    //  Prediction context: everything one test of "press / release now (or
    //  after d ticks)" needs. Built at click time and again every tick by the
    //  live re-planner, so both use exactly the same rules.
    // ------------------------------------------------------------------------
    struct Ctx {
        PlayLayer*    pl = nullptr;
        PlayerObject* p1 = nullptr;
        PlayerObject* p2 = nullptr;
        bool  simP2 = false;
        bool  push = true;
        ModeInfo mode;
        float dt = BASE_DT, sdt = BASE_DT, s = 1.f;
        int   horizon = 0;
        bool  noLanding = false;
        bool  tapModel = false;
        int   tapTicks = 0;
        bool  canEscape = false;
        bool  landRule = false;
        int   window = 0;   // stage window for this input, ticks
        bool  modern = false; // current engine (mini cube) vs 1.10.2 behaviour
    };

    bool makeCtx(PlayLayer* pl, bool push, Config const& c, Ctx& x) {
        x = Ctx{};
        x.pl = pl;
        bool const isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool const twoPlayer = pl->m_level && pl->m_level->m_twoPlayerMode;
        x.p1 = pl->m_player1;
        x.p2 = isDual ? pl->m_player2 : nullptr;
        x.simP2 = isDual && !twoPlayer;
        if (!x.p1 || x.p1->m_isDead || x.p1->m_isPlatformer || (x.p2 && x.p2->m_isPlatformer)) return false;
        x.push = push;
        x.mode.add(x.p1);
        x.mode.add(x.p2);
        x.dt  = getValidDt();
        x.sdt = simDt();
        x.s   = BASE_DT / x.dt;
        x.horizon = std::clamp(static_cast<int>(std::lround(x.mode.baseHorizon() * x.s * m_horizonScale)),
                               MIN_HORIZON, MAX_SIM_STEPS);
        x.noLanding = x.mode.noLanding();
        x.window = x.mode.windowTicks(effectiveCfg(c), push, x.dt);

        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_orbIntent  = scaleTicks(ORB_INTENT_BASE, x.s);
        m_orbClick   = scaleTicks(ORB_CLICK_BASE, x.s);

        // Cube / ball / spider press: in 2.2 holding re-jumps on landing, so a sim
        // that never lets go predicts bunny-hops you don't do. Let go after your
        // usual tap length instead (measured, default ~67 ms).
        bool const onlyLanding = x.mode.cubeLike &&
                                 !(x.mode.ship || x.mode.wave || x.mode.ufo || x.mode.swing || x.mode.robot);
        x.tapModel = c.realTaps && push && onlyLanding;
        x.tapTicks = std::max(scaleTicks(2, x.s), static_cast<int>(std::lround(
                         rhythm(m_holdLen[W_CUBE], DEFAULT_TAP_FRAMES) / x.dt)));

        // Later inputs change the path in these modes ("could you still save it").
        x.canEscape = x.mode.wave || x.mode.ship || x.mode.robot || x.mode.ufo || x.mode.swing;
        // Cube / ball / spider / robot: a click is judged on reaching the ground
        // again; a death after that landing is your next jump's job.
        x.landRule = !x.noLanding;
        x.modern = modernEngine(x.mode);
        return true;
    }

    long long   m_simSizeSwitches = 0;   // size portals passed inside predictions
    int         m_simChainStop = 0;      // > 0: a test ends (alive) this many ticks after reaching the next orb (unused since 1.25.9)

    SimResult runSim(Ctx const& x, int delay, FollowUp const& f = FollowUp{}, int h = -1) {
        int const hz = (h > 0) ? std::min(h, x.horizon) : x.horizon;
        m_simModern = x.modern;                         // near-miss clearance: current engine only
        m_simMovers = MOVERS_ON && (x.modern || !x.mode.holdBased()); // moving spikes / orbs (off since 1.26.0)
        if (x.tapModel && f.at[0] < 0) {
            FollowUp t;
            t.at[0] = delay + x.tapTicks;
            t.held[0] = false;
            return simulate(x.pl, x.p1, x.p2, x.simP2, x.push, delay, hz, x.noLanding, x.sdt, x.s, t);
        }
        return simulate(x.pl, x.p1, x.p2, x.simP2, x.push, delay, hz, x.noLanding, x.sdt, x.s, f);
    }

    // "Could the player still save this themselves?" for a test `r` that died.
    //  * cube / ball / spider / robot: died after landing again -> yes (next jump).
    //  * ship / wave / robot / UFO / swing: a death more than 100 ms out only
    //    counts if no follow-up input (a release / press, a tap, or a full
    //    cycle at your rhythm) at a few sensible moments avoids it.
    bool savableLater(Ctx const& x, int delay, SimResult const& r, int samples, bool& budgetHit) {
        if (r.end != SimEnd::Died) return false;
        if (x.landRule && r.landTick >= 0 && r.ticks > r.landTick + scaleTicks(LAND_GRACE_BASE, x.s)) return true;
        if (!x.canEscape) return false;
        int const deadTicks = r.ticks;
        int commit = scaleTicks(ESCAPE_COMMIT_BASE, x.s); // deaths before this are the click's fault
        int react  = scaleTicks(ESCAPE_FOLLOW_BASE, x.s); // earliest follow-up we assume
        // Ship / wave: your next input comes at YOUR rhythm (median hold after a
        // press, gap after a release), often well under 50 ms in wave. A death
        // after that is the next input's job (it gets its own check), the same
        // idea as cube's landing rule. Before, a wave press you'd have released
        // in time looked deadly and got delayed - late presses and shifted
        // releases on the bigger stages.
        // (Wave in both engines: spam taps are short, and judging them against a
        // 50 ms hold delayed normal taps by a tick or two - the wobble. Ship:
        // current engine only.)
        if (x.mode.wave || (x.modern && x.mode.ship)) {
            WinIdx const g = primaryWin(x.mode, true);
            float const nextFrames = x.push ? rhythm(m_holdLen[g], DEFAULT_TAP_FRAMES)
                                            : rhythm(m_gapLen[g], DEFAULT_GAP_FRAMES);
            int const next = std::max(1, static_cast<int>(std::lround(nextFrames / x.dt)));
            react  = std::clamp(next, scaleTicks(2, x.s), react);
            commit = std::clamp(next + scaleTicks(WAVE_GRACE_BASE, x.s), scaleTicks(6, x.s), commit);
        }
        int const tapGap = scaleTicks(1, x.s);
        int const escHorizon = deadTicks + commit;              // survive 100 ms past where it died
        // Cheap mode (out of time on a heavy level): no follow-up tests, assume a
        // death after the commit time is yours to steer out of (what the full
        // check concludes almost every time).
        if (samples < 0) return deadTicks > commit;
        bool const push = x.push;
        ModeInfo const& mode = x.mode;
        for (int const k : ibcore::escapeSamples(deadTicks, commit, react, scaleTicks(3, x.s), samples)) {
            FollowUp f;
            int const at = delay + k;
            if (push && (mode.ufo || mode.swing) && !(mode.ship || mode.wave || mode.robot)) {
                f.at[0] = at;          f.held[0] = false; // release...
                f.at[1] = at + tapGap; f.held[1] = true;  // ...and tap again
            } else {
                f.at[0] = at;          f.held[0] = !push; // let go (or press again after a release)
            }
            SimResult re = runSim(x, delay, f, escHorizon);
            if (re.end == SimEnd::OutOfBudget) { budgetHit = true; return false; }
            if (re.end == SimEnd::Survived || re.end == SimEnd::Truncated) return true;

            // Ship / wave: one input rarely saves a long stretch; try a full
            // cycle with your real rhythm (let go, then press again - or the
            // other way round after a release).
            if ((mode.ship || mode.wave) && f.at[1] < 0) {
                WinIdx const g = primaryWin(mode, true);
                float const second = push ? rhythm(m_gapLen[g], DEFAULT_GAP_FRAMES)
                                          : rhythm(m_holdLen[g], DEFAULT_TAP_FRAMES);
                f.at[1] = at + std::max(scaleTicks(2, x.s), static_cast<int>(std::lround(second / x.dt)));
                f.held[1] = push;
                SimResult rc = runSim(x, delay, f, escHorizon);
                if (rc.end == SimEnd::OutOfBudget) { budgetHit = true; return false; }
                if (rc.end == SimEnd::Survived || rc.end == SimEnd::Truncated) return true;
            }
        }
        return false;
    }

    // Verdict for a test `rd` of the input delayed by d. `r0Ticks`: how far the
    // undelayed input got (a cut-short path must get clearly further to count).
    ibcore::Verdict verdictFor(Ctx const& x, int d, SimResult const& rd, int r0Ticks, bool& budgetHit, int samples = 2) {
        switch (rd.end) {
            case SimEnd::OutOfBudget: return ibcore::Verdict::Budget;
            case SimEnd::Survived:    return ibcore::Verdict::Good;
            case SimEnd::Truncated:
                // Safe until an orb / portal we can't see past, and clearly further than frame 0.
                return rd.ticks > r0Ticks + scaleTicks(6, x.s) ? ibcore::Verdict::Partial : ibcore::Verdict::Bad;
            case SimEnd::Died:
            default: {
                bool const esc = savableLater(x, d, rd, samples, budgetHit);
                if (budgetHit) return ibcore::Verdict::Budget;
                return esc ? ibcore::Verdict::Good : ibcore::Verdict::Bad;
            }
        }
    }

    // ------------------------------------------------------------------------
    //  Input interception (called from GJBaseGameLayer::handleButton)
    //  returns true  -> let the game handle the input now
    //          false -> swallow it (it is buffered and will be re-fired)
    // ------------------------------------------------------------------------
    bool interceptInput(PlayLayer* pl, bool push, bool isPlayer1) {
        bool const isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool const isTwoPlayerControl = pl->m_level && pl->m_level->m_twoPlayerMode;
        // Which inputs are "yours"? GD (and keybind / remap mods) can deliver a
        // jump with either player flag, depending on the key. So:
        //   * two-player levels: only player-1 inputs are buffered; player 2 is
        //     another person and stays raw;
        //   * one-player dual: a player-2 event right after a matching player-1
        //     event is GD's mirror of it and follows it; otherwise it's yours;
        //   * everything else: any flag is yours (mouse, space, W, up, custom keys).
        if (!isPlayer1) {
            if (isTwoPlayerControl) return true;
            bool const mirror = isDual && m_lastPrimaryTick == m_tick && m_lastPrimaryPush == push;
            if (mirror) return !m_queue.active(); // held with P1's input, else straight through
        }
        m_lastPrimaryTick = m_tick;
        m_lastPrimaryPush = push;

        // Same state twice (key auto-repeat, or a second jump key while one is
        // held): GD sees it as it would without the mod, but it is never a new
        // click for the buffer, and never jumps ahead of a held input.
        if (push == m_physicalHolding) {
            // Swallow only while an input is actually waiting to fire (so it
            // can't jump ahead of it). Otherwise let it through: the "held"
            // state may be stale (a release made in the pause menu never
            // reaches us), and swallowing then ate the player's next real press.
            if (m_queue.pending()) return false;
            m_gameHolding = push;
            return true;
        }

        // Always record the true physical state first.
        m_physicalHolding = push;
        m_inputsSeen++;

        // A pending accuracy check assumed no further input: drop it (a release
        // only counts where releasing changes the path).
        {
            PlayerObject* rp = pl->m_player1;
            bool const relMatters = rp && (rp->m_isDart || rp->m_isShip || rp->m_isRobot);
            if (push || relMatters) {
                m_tracker.invalidate();
                m_trackPending = false;
            }
        }

        Config const& c = cfg();
        m_active   = allowedHere(c, pl);
        m_enabled  = m_active;
        m_logDebug = c.debug;
        m_logCsv   = c.csv;
        if (!m_enabled) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }
        // The hook that fires held inputs isn't running (another mod replaced
        // it, or the game is stalled): never hold an input we couldn't re-send.
        if (!stepsRunning()) {
            if (m_queue.pending()) flushQueue(pl);
            clearBuffer();
            m_gameHolding = push;
            if (m_hookMissing++ == 0) {
                pushLog("NOTE: the physics hook isn't running, so the buffer can't hold inputs - they go straight "
                        "through. Another mod (TPS / physics bypass, speedhack) is probably replacing it.",
                        COL_ORANGE, false);
            }
            return true;
        }

        PlayerObject* p1 = pl->m_player1;
        PlayerObject* p2 = isDual ? pl->m_player2 : nullptr;
        if (!p1 || p1->m_isDead) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }

        // Platformer inputs depend on held directions the sim does not model.
        if (p1->m_isPlatformer || (p2 && p2->m_isPlatformer)) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }

        ModeInfo mode;
        mode.add(p1);
        mode.add(p2);

        // Wave stream: inputs closer together than STREAM_GAP_FRAMES are one
        // stream (spam). A pause ends it and drops the steady shift.
        bool inStream = false;
        if (mode.wave) {
            inStream = (m_gameTime - m_streamLast) < STREAM_GAP_FRAMES;
            m_streamLast = m_gameTime;
        }
        if (!inStream) m_streamShift = 0.0;

        // Rhythm: hold length (press -> release) and gap (release -> press), per mode.
        {
            WinIdx const g = primaryWin(mode, true);
            if (push) {
                if (m_lastReleaseAt >= 0.0) {
                    double const gap = m_gameTime - m_lastReleaseAt;
                    if (gap > 0.0 && gap < 60.0) m_gapLen[g].add(static_cast<float>(gap));
                }
                m_lastPressAt = m_gameTime;
            } else {
                if (m_lastPressAt >= 0.0) {
                    double const hold = m_gameTime - m_lastPressAt;
                    if (hold > 0.0 && hold < 60.0) m_holdLen[g].add(static_cast<float>(hold));
                }
                m_lastReleaseAt = m_gameTime;
            }
        }

        float const dt  = getValidDt(); // nominal tick, 60fps-frame units (time warp removed)
        float const s = BASE_DT / dt;   // real ticks per 240-TPS base tick

        int horizon = static_cast<int>(std::lround(mode.baseHorizon() * s * m_horizonScale));
        horizon = std::clamp(horizon, MIN_HORIZON, MAX_SIM_STEPS);

        int const windowTicks = mode.windowTicks(effectiveCfg(c), push, dt);

        ClickNote base;
        base.push = push;
        base.windowTicks = windowTicks;
        base.horizon = horizon;

        auto pass = [&](Decision d) {
            m_gameHolding = push;
            if (d != Decision::PassRelease && d != Decision::PassModeOff) rememberInput(pl, push);
            ClickNote n = base;
            n.d = d;
            note(c, mode, dt, n);
            return true;
        };
        // 1. An input is already held back: queue this one behind it so taps are never eaten.
        if (m_queue.active()) {
            // Keep order and spacing: replay it after the held input with the same shift.
            m_queue.push(push, m_gameTime, static_cast<double>(dt) * 0.999);
            // e.g. the release of a press that already fired: it gets the live check too
            if (!m_live && m_queue.pending()) {
                m_live = true;
                m_liveState.needInit = true;
            }
            m_lastInputTime = m_gameTime;
            ClickNote n = base;
            n.d = Decision::Queued;
            n.delay = m_queue.pending()
                ? std::max(0, static_cast<int>(std::ceil((m_queue.nextAt() - m_gameTime) / dt - 1e-3)))
                : static_cast<int>(std::lround(m_queue.shift() / dt));
            note(c, mode, dt, n);
            return false;
        }

        // 1b. Gamemode switched off in the Modes tab: raw input.
        {
            std::string const id = modeId(p1);
            if (!modeOn(id) || (p2 && !modeOn(modeId(p2)))) return pass(Decision::PassModeOff);
        }

        // 2. Releases only matter in hold-based modes (and don't count as spam elsewhere).
        if (!push && !mode.releaseMatters()) {
            return pass(Decision::PassRelease);
        }

        // 2b. Wave spam: keep ONE steady shift for the whole stream. Jitter came
        //     from every input being judged on its own - one delayed 3 ticks,
        //     the next raw, the next 2 - so your holds came out uneven. Now an
        //     input in a stream that already has a shift gets exactly the same
        //     shift, on a fixed schedule (no live re-timing), until you pause.
        if (inStream && m_streamShift > TIME_EPS) {
            Ctx sx;
            if (makeCtx(pl, push, c, sx)) {
                // Keep the stream's shift exactly (it only resets after a pause):
                // shrinking it 1 tick per tap shortened a gap by a tick each time.
                int const S = std::max(0, static_cast<int>(std::lround(m_streamShift / dt)));
                int const budgetUs0 = clickBudgetUs(c);
                m_hasDeadline = budgetUs0 > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs0);
                m_recordPath = false;
                auto okAt = [&](int d) {
                    SimResult const r = runSim(sx, d);
                    bool bh = false;
                    ibcore::Verdict const v = verdictFor(sx, d, r, -1000000, bh);
                    return v == ibcore::Verdict::Good || v == ibcore::Verdict::Partial;
                };
                // On time first (1.25.9): if this input is safe with no delay, the
                // stream drops its shift right here - before, one delay stayed on
                // every input until you paused (input lag for the whole burst).
                int chosen = -1;
                if (okAt(0)) chosen = 0;
                else if (okAt(S)) chosen = S;
                m_hasDeadline = false;
                if (chosen == 0) {
                    m_streamShift = 0.0;
                    return pass(Decision::PassSafe); // stream wound down to no shift
                }
                if (chosen > 0) {
                    m_streamShift = static_cast<double>(chosen) * dt;
                    double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
                    m_queue.start(push, m_gameTime, std::max(0.0, gridStart + static_cast<double>(chosen) * dt - m_gameTime));
                    m_live = false; // fixed schedule: the stream keeps its rhythm
                    ClickNote n = base;
                    n.d = Decision::Buffered;
                    n.delay = chosen;
                    n.stream = true;
                    note(c, mode, dt, n);
                    return false;
                }
                // neither keeps you alive: judge this one normally below
            }
        }

        // 3. Streak-based spam guard (real-time interval, off in Extreme).
        if (mode.spamGuarded() && !c.extreme) {
            double const interval = m_gameTime - m_lastInputTime;
            if (interval < static_cast<double>(c.spamFrames)) m_streak++;
            else m_streak = 0;
            m_lastInputTime = m_gameTime;
            if (m_streak >= c.spamStreak) return pass(Decision::PassSpam);
        } else {
            m_streak = 0;
            m_lastInputTime = m_gameTime;
        }

        // 4. Window shorter than one physics tick (e.g. 0.5f at 60 TPS): nothing to do.
        if (windowTicks <= 0) return pass(Decision::PassNoWindow);
        int const maxDelay = windowTicks;

        // CPU budget: the manual cap, further capped to a share of the frame time
        // when Auto is on (extreme gets a bigger share since it searches more).
        int const budgetUs = clickBudgetUs(c);
        m_lastBudgetUs = budgetUs;

        auto const t0 = Clock::now();
        m_hasDeadline = budgetUs > 0;
        if (m_hasDeadline) m_deadline = t0 + std::chrono::microseconds(std::max(budgetUs, R0_BUDGET_MIN_US));

        bool const holdBased = mode.holdBased();
        m_recordPath = c.accuracy || c.overlay;

        Ctx x;
        if (!makeCtx(pl, push, c, x)) return pass(Decision::PassNoWindow);
        bool budgetHit = false;
        auto sim = [&](int delay, FollowUp const& f = FollowUp{}, int h = -1) { return runSim(x, delay, f, h); };
        auto escapable = [&](int delay, SimResult const& r, int samples) { return savableLater(x, delay, r, samples, budgetHit); };

        // 5. Frame 0 (immediate input).
        SimResult r0 = sim(0);
        std::vector<ibcore::Point2> path0;
        if (m_recordPath) path0 = m_path;

        bool discard = (r0.end == SimEnd::OutOfBudget); // result not trustworthy -> raw input
        // The rest (escape check, search) gets a fresh budget of its own, so a
        // slow first test doesn't leave it nothing.
        m_hasDeadline = budgetUs > 0;
        if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
        budgetHit = discard;
        bool escape0 = false;
        bool liveFallback = false; // search ran out of CPU time on a deadly click: re-plan live
        // Out of time on a deadly click: instead of giving up (raw), finish with
        // cheap checks (no follow-up tests) under a hard cap. The original
        // Predictive Buffer never gave up at all - that's why it kept buffering
        // on Grief / Slaughterhouse where this one went "raw: CPU budget".
        bool cheap = false;
        auto enterCheap = [&]() {
            cheap = true;
            budgetHit = false;
            m_cheapClicks++;
            m_hasDeadline = true;
            m_deadline = t0 + std::chrono::microseconds(CLICK_HARD_CAP_US);
        };
        int bestDelay = 0;
        int evals = 0;
        if (r0.end == SimEnd::Died) {
            escape0 = escapable(0, r0, 3);
            if (budgetHit) { // ran out asking "could you save it": decide it cheaply instead
                enterCheap();
                escape0 = savableLater(x, 0, r0, -1, budgetHit);
            }
            // Ran out of time asking "could you save it yourself?": the click is
            // deadly as made, so hold it and let the live check (one cheap test
            // per tick) decide, instead of letting a deadly click through.
            // (Ship / wave: raw, as before 1.12.1 - holding a wave press back
            // isn't "safe", it just keeps you flying the other way.)
            if (budgetHit) { if (x.modern) liveFallback = true; else discard = true; }
        }

        // How good is the click if delayed by d ticks? (shared by the search and the probe)
        auto eval = [&](int d) -> ibcore::Verdict {
            SimResult rd = sim(d);
            if (rd.end == SimEnd::OutOfBudget && !cheap) {
                enterCheap();
                rd = sim(d);
            }
            // one "could you still save it" sample per candidate (the first
            // test gets three): the search tries many delays.
            ibcore::Verdict v = verdictFor(x, d, rd, r0.ticks, budgetHit, cheap ? -1 : 1);
            if (v == ibcore::Verdict::Budget && !cheap && rd.end != SimEnd::OutOfBudget) {
                enterCheap();
                v = verdictFor(x, d, rd, r0.ticks, budgetHit, -1);
            }
            return v;
        };

        // 6. Frame 0 is doomed: find the delay to use.
        int earliest = 0;
        int planTick = 0; // > 0: follow the plan (see below)
        if (r0.end == SimEnd::Died && !escape0 && !discard) {
            ibcore::SearchParams sp;
            sp.maxDelay = maxDelay;
            // Every tick near the click (narrow safe gaps live there), then a
            // stride over the rest of the window.
            sp.dense    = scaleTicks(DENSE_SCAN_BASE, s);
            sp.stride   = std::max(1, (std::max(0, maxDelay - sp.dense) + MAX_CANDIDATES - 1) / MAX_CANDIDATES);
            sp.guard    = c.cbfGuard;
            sp.margin   = (c.safetyTicks > 0 && !mode.wave) ? scaleTicks(c.safetyTicks, s) : 0;
            sp.aim      = (c.extreme && !holdBased) ? c.extremeAim : 0.f;
            ibcore::SearchResult sr = ibcore::searchDelay(sp, eval);
            evals = sr.evals;
            if (sr.budget) budgetHit = true;
            if (sr.delay == 0 && sr.budget) {
                // ran out before finding anything: cube-type -> decide live; ship / wave -> raw
                if (x.modern) liveFallback = true; else discard = true;
            }
            bestDelay = sr.delay;
            earliest = sr.earliest;
            // Follow the plan (1.26.1, not mini cube - it keeps its per-tick
            // checks: near-miss waits, learned spots): fire on the tick the
            // per-tick re-check would have fired on - the first safe tick with a
            // safe tick after it (wave: the first safe tick) - instead of
            // re-predicting every tick to find it again.
            if (!x.modern && sr.full && !sr.budget && sr.guarded > 0) {
                planTick = mode.wave ? sr.earliest : sr.guarded;
                bestDelay = planTick;
            }
        }

        // 7. CPU time used. (The lookahead is never shortened to save time: a
        //    short lookahead can't see the next obstacle and calls deadly clicks
        //    "safe". Out of time = this one click goes raw, nothing else.)
        long long const usedUs = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
        // 7b. Nothing in the window saves this click: look further, only to tell
        //     you how early it was (death replay, auto windows). Never applied.
        int needed = -1;   // -1 not probed, 0 nothing found, >0 ticks it needed
        int probeMax = 0;
        // Quick search found nothing: in cube / UFO / robot / ball / spider /
        // swing the click is held and checked EVERY tick instead (the search
        // only checks every tick for the first 12, then every few ticks, so a
        // narrow safe moment far from an early click was stepped over and the
        // click was called "too late"). Ship / wave: holding a press isn't safe.
        // (Not UFO / swing either: in Amethyst's UFO part the prediction was wrong
        // about both pressing and not pressing, so holding the click there made
        // things worse than letting it through. 1.19.2)
        // 1.22.2: REMOVED at the player's request (held clicks felt wrong).
        // Kept as a constant so the code around it stays simple.
        bool const rescueOk = false;
        if (r0.end == SimEnd::Died && !escape0 && !discard && !liveFallback && bestDelay == 0 && !rescueOk &&
            (c.hud || c.debug || c.csv || c.autoWindow)) {
            probeMax = std::min(MAX_SIM_STEPS / 2, std::max(maxDelay * 2, scaleTicks(PROBE_MIN_BASE, s)));
            if (probeMax > maxDelay) {
                bool const savedBudgetHit = budgetHit;
                m_hasDeadline = budgetUs > 0;
                // Log-only ("click was Nt too early"): kept cheap - 1/8 of the
                // budget, 8 coarse candidates, no dense scan.
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(std::max(300, budgetUs / 8));
                ibcore::SearchParams pp;
                pp.minDelay = maxDelay + 1;
                pp.maxDelay = probeMax;
                pp.stride   = std::max(1, (probeMax - maxDelay + 7) / 8);
                pp.dense    = 0;
                pp.guard    = false;
                ibcore::SearchResult pr = ibcore::searchDelay(pp, eval);
                needed = pr.earliest;
                budgetHit = savedBudgetHit;
            }
        }
        m_hasDeadline = false;

        // 8. Decide.
        Decision dec;
        if (discard)                          dec = Decision::PassBudget;
        else if (liveFallback && r0.end == SimEnd::Died) dec = Decision::Buffered; // plan +1, the live check decides
        else if (r0.end == SimEnd::Truncated) dec = Decision::PassTruncated;
        else if (r0.end == SimEnd::Died && escape0) dec = Decision::PassEscapable;
        else if (r0.end == SimEnd::Died)      dec = (bestDelay > 0) ? Decision::Buffered : Decision::PassNoFix;
        else                                  dec = Decision::PassSafe;

        // 8b. Learned spots: what really happened on earlier attempts beats the
        //     prediction. A click on a spot that killed you before is held and
        //     re-planned live; a "no fix" click with a spot ahead that worked
        //     before is held for it.
        int spotNote = 0;
        if (c.learnSpots && push && x.modern && maxDelay > 0 && m_spots.size() > 0 && !discard) {
            ibcore::Spot const here = spotNow(pl, push, mode.name());
            ibcore::SpotTol const tol = spotTol();
            if (x.modern && (dec == Decision::PassSafe || dec == Decision::PassEscapable) &&
                m_spots.at(here.x, here.y, here.vy, here.key, tol) == ibcore::SpotHit::Bad) {
                dec = Decision::Buffered;
                bestDelay = std::max(1, bestDelay);
                spotNote = 1;
            } else if (dec == Decision::PassNoFix || dec == Decision::PassTruncated) {
                // An early click the prediction can't line up (blind past a
                // portal / orb, or no safe moment found), with a spot ahead where
                // a press WORKED on an earlier attempt - in any gamemode, e.g. an
                // orb right after a UFO portal: hold it for that spot.
                // Short holds only (1.23.1): holding a press changes your path (in
                // UFO it's a flap that doesn't happen), so after a long hold you
                // reach the spot lower than on the attempt that worked.
                int const holdCap = std::min(maxDelay, scaleTicks(SPOT_HOLD_MAX_BASE, s));
                float const reach = m_p1Speed * dt * static_cast<float>(holdCap);
                float d = m_spots.goodAhead(here.x, here.y, here.key, m_p1Dir, reach, tol, here.anyKey);
                if (d > 0.f && m_p1Speed > 0.01f) {
                    // ...and only if not pressing until then doesn't kill you first.
                    int const k = std::clamp(static_cast<int>(std::lround(d / (m_p1Speed * dt))), 1, holdCap);
                    m_hasDeadline = budgetUs > 0;
                    if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
                    bool const rec = m_recordPath;
                    m_recordPath = false;
                    SimResult const wait = sim(k);
                    m_recordPath = rec;
                    m_hasDeadline = false;
                    if (wait.end == SimEnd::Died && wait.deathStep >= 0 && wait.deathStep < k) d = -1.f;
                }
                if (d > 0.f && m_p1Speed > 0.01f) {
                    dec = Decision::Buffered;
                    bestDelay = std::clamp(static_cast<int>(std::lround(d / (m_p1Speed * dt))), 1, maxDelay);
                    spotNote = 2;
                    m_spotTargetX = here.x + d * m_p1Dir;
                    m_spotTargetDir = m_p1Dir;
                }
            }
        }

        // 8b1 / 8b2 (orb aim, portal hold) removed in 1.25.9: they held or moved
        // clicks and made the game feel laggy. Orbs are still predicted exactly
        // (learned orb speeds, size portals); nothing holds a click for one.

        // 8c. Rescue: hold it and check every tick for the first safe moment.
        bool rescue = false;
        if (dec == Decision::PassNoFix && rescueOk) {
            dec = Decision::Buffered;
            bestDelay = maxDelay; // no plan: "couldn't check" falls back to the window end
            rescue = true;
        }

        // Path of the chosen delay (orb note, accuracy check, overlay). One more
        // test, after the budget window: only when something will use it.
        std::vector<ibcore::Point2> pathB;
        bool viaOrb = false;
        // (Only for the path overlay now: the live check supplies the accuracy
        // path when the click fires, so this was one wasted full test per click.)
        if (dec == Decision::Buffered && c.overlay) {
            sim(bestDelay);
            viaOrb = c.orbPredict && m_simOrbUsed;
            if (m_recordPath) pathB = m_path;
        }

        ClickNote n = base;
        n.d = dec;
        n.us = usedUs;
        n.r0 = r0;
        n.delay = bestDelay;
        n.evals = evals;
        n.orb = viaOrb;
        n.needed = needed;
        n.rescue = rescue;
        note(c, mode, dt, n);
        m_last.needed = needed;
        m_last.probe = probeMax;
        m_last.window = windowTicks;

        // Auto windows: how much later did this click need to be?
        WinIdx const widx = primaryWin(mode, push);
        if (dec == Decision::Buffered && earliest > 0) {
            recordSave(c, widx, static_cast<float>(earliest) * dt);
        } else if (dec == Decision::PassNoFix) {
            recordMiss(c, widx, needed > 0 ? static_cast<float>(needed) * dt : -1.f);
        }

        // Accuracy: compare this prediction with reality 100 ms after the click lands.
        int const checkSteps = scaleTicks(ACCURACY_CHECK_BASE, s);
        m_trackPending = false;
        if (c.accuracy && dec == Decision::PassSafe && r0.end == SimEnd::Survived && !path0.empty()) {
            m_tracker.start(m_gameTime, dt, path0, checkSteps - 1, ACCURACY_TOL_PX);
            m_trackMode = mode.name();
        } else if (c.accuracy && dec == Decision::Buffered && bestDelay > 0 && !pathB.empty()) {
            // Starts when the held click actually fires (aligned to the real step).
            m_trackPending = true;
            m_pendingPath = pathB;
            m_pendingDelay = bestDelay;
            m_pendingCheck = bestDelay + checkSteps - 1;
            m_pendingMode = mode.name();
        }


        if (spotNote == 1) pushLog(fmt::format("{} held: this spot killed you on an earlier attempt", push ? "PRESS" : "REL"), COL_CYAN, false);
        if (spotNote == 2) pushLog(fmt::format("{} held for a spot that worked on an earlier attempt (+{}t)", push ? "PRESS" : "REL", bestDelay), COL_CYAN, false);
        if (spotNote) m_spotSaves++;

        if (dec == Decision::Buffered) {
            // Times on the real tick grid: the current tick started `grid` ago,
            // and delay d means "at the start of the d-th tick from now" - what
            // the sim tested. (Before 1.23.3 a mid-tick click fired up to a
            // whole tick later than planned with Click Between Frames.)
            double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
            auto at = [&](double ticks) { return gridStart + ticks * dt; };
            // Follow the plan: one prediction now, recording where you'll be on
            // every tick until it fires (and after) - replaces a full prediction
            // per tick while it's held.
            m_planTrust = false;
            if (planTick > 0 && planTick == bestDelay && !liveFallback && !rescue && spotNote == 0) {
                bool const rec = m_recordPath;
                m_recordPath = true;
                m_hasDeadline = false;
                SimResult const pr = sim(planTick);
                m_recordPath = rec;
                if (pr.end != SimEnd::OutOfBudget && static_cast<int>(m_path.size()) >= planTick) {
                    m_planPath = m_path;
                    m_planGrid = gridStart;
                    m_planTrust = true;
                }
            }
            m_queue.start(push, m_gameTime, std::max(0.0, at(bestDelay) - m_gameTime));
            m_live = true;
            // If the live check keeps refusing until the click-time plan, fire ON
            // the plan: the search found it safe, and in a narrow gap even one
            // tick later runs you into the obstacle.
            // No plan to fall back on (out of CPU / memory hold): give up after a
            // few ticks instead of holding it for the whole window.
            double latest = at(std::max(1, scaleTicks(LIVE_GIVEUP_BASE, s))) + TIME_EPS;
            if (!liveFallback && spotNote != 1 && bestDelay > 0) {
                latest = at(bestDelay) + TIME_EPS; // exactly on the plan
            }
            // Held for a learned spot: fire on the spot, or 2 ticks after its
            // estimated arrival at the latest (the estimate uses your speed).
            m_liveSpotHold = (spotNote == 2);
            if (spotNote == 2) latest = at(bestDelay + scaleTicks(2, s)) + TIME_EPS;
            if (rescue) {
                // No plan to fall back on: the whole window is checked tick by
                // tick. But if doing nothing kills you inside the window, fire
                // 2 ticks before that at the latest - a held click must never
                // just sit there while you die.
                latest = -1.0;
                m_hasDeadline = budgetUs > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
                bool const rec = m_recordPath;
                m_recordPath = false;
                SimResult const idle = sim(maxDelay); // press only at the window end = idle until then
                m_recordPath = rec;
                m_hasDeadline = false;
                if (idle.end == SimEnd::Died && idle.deathStep >= 0 && idle.deathStep < maxDelay) {
                    int const lastChance = std::max(1, idle.deathStep - scaleTicks(2, s));
                    latest = at(lastChance) + TIME_EPS;
                }
            }
            m_liveRescue = rescue; // first safe tick, no margin

            m_liveState.startClick(m_gameTime, dt, at(bestDelay), at(maxDelay + 1) + TIME_EPS, latest);
            m_liveArrive = m_gameTime;
            m_livePlanTicks = bestDelay;
            m_liveWaited = false;
            m_liveWaitWhy = 0;
            for (int& v : m_refuse) v = 0;
            m_refuseHave = false;
            m_last.liveTicks = -1;
            m_last.liveReason = ibcore::FireReason::None;
            m_trackPending = false; // the live check supplies the exact path when it fires
            if (mode.wave) m_streamShift = static_cast<double>(bestDelay) * dt; // updated when it really fires
            return false;
        }

        if (mode.wave) m_streamShift = 0.0; // went in raw: the stream has no shift now
        m_gameHolding = push;
        return true;
    }

    // ------------------------------------------------------------------------
    //  Live re-planning step (before a physics step, while an input is held)
    // ------------------------------------------------------------------------
    void liveStep(PlayLayer* pl) {
        double const now = m_gameTime;
        float const dt = getValidDt();
        Config const& c = cfg();
        bool const wasFirst = m_liveState.first && !m_liveState.needInit;

        std::vector<ibcore::Point2> path;
        std::string modeName;
        float simScale = 1.f;
        bool waveMode = false;
        SimEnd testedEnd = SimEnd::Survived;

        // "Is firing it now safe?" - one test from the real current state.
        //  * The click you made (head = first) gets everything: learned spots,
        //    near-miss waits, waiting for a spot that worked before.
        //  * Inputs queued behind it (usually your release) only get the plain
        //    safety check, so your hold lengths / rhythm stay yours.
        auto check = [&](bool push) -> ibcore::LiveVerdict {
            // Following the plan: on the predicted path, just wait for the plan
            // tick (it fires there through m_liveState.latest) - no prediction.
            if (push && m_planTrust && m_liveState.first) {
                if (!planDrifted(pl, now, dt)) {
                    m_planTicksSaved++;
                    return ibcore::LiveVerdict::Unsafe; // "not yet"; the plan tick fires it
                }
                m_planTrust = false; // something the prediction didn't know: check for real
                m_planDrifts++;
                if (c.hudVerbose) pushLog("held click: you left the predicted path - checking every tick", COL_GREY, false);
            }
            Ctx x;
            if (!makeCtx(pl, push, c, x)) return ibcore::LiveVerdict::OnSchedule;
            modeName = x.mode.name();
            simScale = x.s;
            waveMode = x.mode.wave;
            if (!push && !x.mode.releaseMatters()) return ibcore::LiveVerdict::OnSchedule;
            bool const head = m_liveState.first;
            // Held for a learned spot: fire exactly when you reach it (no
            // prediction - it may be blind there), else keep waiting.
            if (head && push && m_liveSpotHold) {
                // Reached (or just passed) the spot's position: fire now. By
                // position, not an exact-tick match, so a check that lands a
                // fraction of a tick off (CBF sub-steps) can't skip it.
                float const px = pl->m_player1->getPositionX();
                float const tolX = spotTol().x;
                if ((px - m_spotTargetX) * m_spotTargetDir >= -tolX) {
                    m_spotFires++;
                    return ibcore::LiveVerdict::Proven;
                }
                return ibcore::LiveVerdict::Unsafe;
            }
            PlayerObject* const p1 = pl->m_player1;
            // Cube-type press in the air does nothing until you land: never
            // "proven" there (a learned air spot must not eat the click).
            bool const airPress = push && x.mode.cubeLike && !(x.mode.ship || x.mode.wave || x.mode.ufo ||
                                                              x.mode.swing || x.mode.robot) && !p1->m_isOnGround;

            // Learned spots first: firing on a spot that worked before repeats it;
            // a spot that killed you before is skipped whatever the sim says.
            ibcore::Spot spotHere;
            ibcore::SpotTol spotT;
            bool const useSpots = x.modern && head && push && c.learnSpots &&
                                  m_spots.size() > 0; // presses only; mini cube
            if (useSpots) {
                spotHere = spotNow(pl, push, modeName);
                spotT = spotTol();
                ibcore::SpotHit const h = m_spots.at(spotHere.x, spotHere.y, spotHere.vy, spotHere.key, spotT);
                if (h == ibcore::SpotHit::Good && !airPress) { m_spotFires++; return ibcore::LiveVerdict::Proven; }
                if (h == ibcore::SpotHit::Bad)  { m_spotBlocks++; m_refuse[RF_SPOT]++; return ibcore::LiveVerdict::Unsafe; }
            }

            // The next input you already made (queued, exact) or, while you still
            // hold, at least how long you've held so far. Ticks after this one fires.
            int knownGap = -1;
            {
                double const opp = m_queue.nextOppositeAt();
                if (opp >= 0.0) knownGap = std::max(1, static_cast<int>(std::lround((opp - m_queue.nextAt()) / dt)));
            }
            if (x.tapModel) {
                // Cube: holding through a landing re-jumps in 2.2, so use your real
                // hold, not the typical tap.
                int hold = knownGap;
                if (hold < 0 && m_physicalHolding) hold = static_cast<int>(std::lround((now - m_liveArrive) / dt));
                if (hold > 0) x.tapTicks = std::max(x.tapTicks, hold);
            }
            // Ship / wave / robot: your release (or next press) is part of the path.
            bool const useGap = x.modern && knownGap > 0 && !x.tapModel && x.mode.releaseMatters();
            auto simAt = [&](int d) {
                if (!useGap) return runSim(x, d);
                FollowUp f;
                f.at[0] = d + knownGap;
                f.held[0] = !push;
                return runSim(x, d, f);
            };
            auto goodPath = [&](SimResult const& rk) {
                return rk.end == SimEnd::Survived ||
                       (x.landRule && rk.end == SimEnd::Died && rk.landTick >= 0 &&
                        rk.ticks > rk.landTick + scaleTicks(LAND_GRACE_BASE, x.s));
            };

            // A re-check runs every tick while a click is held: smaller share of
            // the frame than a click gets.
            int const budget = liveBudgetUs(c);
            m_hasDeadline = budget > 0;
            if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
            m_recordPath = c.accuracy || c.overlay;
            SimResult r = simAt(0);
            if (m_recordPath) path = m_path;
            testedEnd = r.end;
            bool budgetHit = false;
            ibcore::Verdict const v = verdictFor(x, 0, r, -1000000, budgetHit);
            auto probe = [&](int d) {
                bool const rec = m_recordPath;
                m_recordPath = false; // keep the path of the "now" test
                SimResult rk = simAt(d);
                m_recordPath = rec;
                return rk;
            };

            // Safe now, but a spot that worked before is coming within the window:
            // wait for it (repeat what you know works).
            if (head && v == ibcore::Verdict::Good && useSpots && !airPress && m_p1Speed > 0.01f && m_liveState.deadline >= 0.0) {
                float const reach = m_p1Speed * static_cast<float>(m_liveState.deadline - now - 2.0 * dt);
                float const ahead = reach > 0.f
                    ? m_spots.goodAhead(spotHere.x, spotHere.y, spotHere.key, m_p1Dir, reach, spotT) : -1.f;
                if (ahead > 0.f) {
                    // Only if waiting for it is safe too (it may belong to a later
                    // jump: waiting must not skip the obstacle in front of you).
                    int const k = std::max(1, static_cast<int>(std::lround(ahead / (m_p1Speed * dt))));
                    if (goodPath(probe(k))) {
                        m_hasDeadline = false;
                        m_liveWaitWhy = 2;
                        m_refuse[RF_AHEAD]++;
                        return ibcore::LiveVerdict::Unsafe; // not yet
                    }
                }
            }
            // Safe, but only just (a near miss the real game may not repeat)?
            // If a clearly cleaner moment comes within a few steps, wait for it.
            // Presses only: release timing is your rhythm (wave / ship corridors are
            // always "close", so waiting there just shifts your release).
            if (x.modern && head && push && v == ibcore::Verdict::Good && !budgetHit) {
                float const target = clearTarget(modeName);
                int const look = scaleTicks(CLEAN_LOOK_BASE, x.s);
                double const deadline = m_liveState.deadline;
                int const k = ibcore::cleanerAhead(r.clear, target, CLEAN_GAIN_PX, look, [&](int d) {
                    ibcore::CleanProbe cp;
                    // leave room for the safe-in-a-row rule before the window ends
                    if (deadline >= 0.0 && now + static_cast<double>(d + 2) * dt >= deadline) { cp.stop = true; return cp; }
                    SimResult const rk = probe(d);
                    if (rk.end == SimEnd::OutOfBudget) { cp.stop = true; return cp; }
                    cp.good = goodPath(rk);
                    cp.clear = rk.clear;
                    return cp;
                });
                if (k > 0) {
                    m_hasDeadline = false;
                    m_cleanWaits++;
                    m_liveWaited = true;
                    m_liveWaitWhy = 1;
                    m_refuse[RF_CLEAN]++;
                    return ibcore::LiveVerdict::Unsafe; // not yet: a cleaner moment is coming
                }
            }
            m_hasDeadline = false;
            if (head && v == ibcore::Verdict::Bad) {
                if (r.end == SimEnd::Truncated) m_refuse[RF_CUT]++;
                else m_refuse[RF_DIED]++;
                if (!m_refuseHave && r.end == SimEnd::Died) { m_refuseFirst = r; m_refuseHave = true; }
            }
            switch (v) {
                case ibcore::Verdict::Good:    return ibcore::LiveVerdict::Safe;
                case ibcore::Verdict::Partial: return ibcore::LiveVerdict::SafeCut;
                case ibcore::Verdict::Budget:  return ibcore::LiveVerdict::Unknown;
                default:                       return ibcore::LiveVerdict::Unsafe;
            }
        };
        // The click itself keeps a one-tick margin off the deadly edge (not in wave).
        // (Rescue: the first safe tick, no margin - the safe moment can be 1 tick wide.)
        auto need = [&](bool first) { return (first && !waveMode && !m_liveRescue && c.safetyTicks > 0) ? 2 : 1; };
        // A queued input (your release, or a press made while one was held)
        // fires exactly on its scheduled time: same spacing from the held
        // input as you made it, no re-check. Re-checking held wave releases up
        // to the whole window and made them late.
        auto windowTime = [&](bool) { return 0.0; };

        auto fired = ibcore::liveStep(m_queue, m_liveState, now, TIME_EPS, check, need, windowTime);
        if (c.overlay && !path.empty()) drawPreview(pl, path, previewColor(testedEnd));
        if (fired) {
            fireButton(pl, *fired);
            if (wasFirst) {
                int const firedTicks = static_cast<int>(std::lround((now - m_liveArrive) / dt));
                m_last.liveTicks = firedTicks;
                if (m_streamShift > TIME_EPS || modeName.find("wave") != std::string::npos) {
                    m_streamShift = std::max(0.0, now - m_liveArrive); // the shift it really got
                }
                m_last.liveReason = m_liveState.reason;
                bool const late = m_liveState.reason == ibcore::FireReason::Deadline;
                // Followed the plan to its tick: the normal case now, not a warning.
                bool const followed = m_planTrust && m_liveState.reason == ibcore::FireReason::Plan;
                if (followed) m_planFires++;
                m_planTrust = false;
                // Always shown: when the held click really went in, and why.
                bool const onPlan = !followed && m_liveState.reason == ibcore::FireReason::Plan;
                pushLog(fmt::format("{} fired +{}t ({}){}{}", *fired ? "PRESS" : "REL", firedTicks,
                                    fireReasonText(m_liveState.reason),
                                    m_liveWaited ? ", waited for a cleaner path" : "",
                                    ((late || onPlan) && c.hudVerbose) ? " | live: " + refuseSummary() : std::string()),
                        (late || onPlan) ? COL_ORANGE : COL_GREEN, false);
                // Accuracy: that path is exactly what should happen now.
                if (c.accuracy && !path.empty()) {
                    m_tracker.start(now, dt, std::move(path), scaleTicks(ACCURACY_CHECK_BASE, simScale) - 1, ACCURACY_TOL_PX);
                    m_trackMode = modeName;
                }
            }
        }
        if (!m_queue.pending()) m_live = false;
    }

    // "said no 14x: died 12 (dies 9t after firing, landed -), memory 2"
    std::string refuseSummary() const {
        static char const* names[RF_COUNT] = {"died", "orb/portal", "memory", "near miss", "learned spot ahead"};
        int total = 0;
        std::string parts;
        for (int i = 0; i < RF_COUNT; i++) {
            if (!m_refuse[i]) continue;
            total += m_refuse[i];
            if (!parts.empty()) parts += ", ";
            parts += fmt::format("{} {}", names[i], m_refuse[i]);
            if (i == RF_DIED && m_refuseHave) {
                SimResult const& f = m_refuseFirst;
                std::string what = f.killType >= 0 ? objectName(static_cast<GameObjectType>(f.killType)) : "a block edge / squish (no object)";
                if (f.killMover) what = "MOVING " + what;
                parts += fmt::format(" (dies {}t after firing, {}, hit {} at {:+.0f}px height)", f.ticks,
                                     f.landTick >= 0 ? fmt::format("landed at {}t", f.landTick) : std::string("never landed"),
                                     what, f.killDy);
            }
        }
        if (total == 0) return "no refusals";
        return fmt::format("said no {}x: {}", total, parts);
    }

    static char const* fireReasonText(ibcore::FireReason r) {
        switch (r) {
            case ibcore::FireReason::Safe:     return "safe";
            case ibcore::FireReason::SafeCut:  return "safe up to an orb / portal";
            case ibcore::FireReason::Schedule: return "on plan, couldn't check";
            case ibcore::FireReason::Proven:   return "spot that worked before";
            case ibcore::FireReason::Deadline: return "window ended, nothing safe found";
            case ibcore::FireReason::Plan:     return "on plan";
            default:                           return "?";
        }
    }

    // ------------------------------------------------------------------------
    //  Runs before every P1 physics step (full tick or CBF sub-step)
    // ------------------------------------------------------------------------
    void onPlayerUpdate(PlayLayer* pl, PlayerObject* player) {
        if (player != pl->m_player1) return;
        m_tick++;
        m_lastStepWall = Clock::now();

        // Dead (and not reported through the hook): report once. Alive again
        // after a death without a level reset (some respawn paths): re-arm.
        if (player->m_isDead) {
            if (!m_deathReported) onRealDeath();
            return;
        }
        if (m_deathReported) m_deathReported = false;

        // Housekeeping a few times per frame, not every step.
        if ((m_tick & 7) == 0) {
            refreshFlags();
            refreshHud(pl, false);
        }
        if ((m_tick & 3) == 0) updatePreview(pl);

        if (!m_enabled) {
            clearBuffer();
            reconcile(pl);
            return;
        }

        ensureSimPlayers(pl);
        // Diagnostics only (1.26.0): a prediction every 8 ticks. It reads 100%
        // in every mode now; only with "Log: show safe clicks" on.
        if (cfg().hudVerbose) oneStepCheck(pl, player);
        spotStep(player);
        // Accuracy: has a prediction reached its checkpoint?
        if (m_tracker.active() && pl->m_player1) {
            if (auto r = m_tracker.poll(m_gameTime, pl->m_player1->getPositionX(), pl->m_player1->getPositionY())) {
                AccCount& a = m_accByMode[m_trackMode];
                a.checks++;
                a.errSum += std::min(r->err, 20.f); // one wild miss shouldn't decide the target alone
                a.dxSum += std::clamp(r->dx, -20.f, 20.f);
                a.dySum += std::clamp(r->dy, -20.f, 20.f);
                if (r->hit) a.hits++;
                else notePredictionMiss(r->err, r->dx, r->dy);
            }
        }

        if (m_queue.pending() && m_live) {
            // Once per physics tick: CBF adds short extra steps, and re-checking on
            // each of them doubled the work in a frame (chop). A held click whose
            // plan / window end comes due still goes in on time (checked first).
            double const sinceCheck = m_gameTime - m_lastLiveAt;
            bool const due = (m_liveState.latest >= 0.0 && m_gameTime + TIME_EPS >= m_liveState.latest) ||
                             (m_liveState.deadline >= 0.0 && m_gameTime + TIME_EPS >= m_liveState.deadline) ||
                             m_liveState.needInit;
            if (due || sinceCheck >= 0.6 * static_cast<double>(getValidDt())) {
                m_lastLiveAt = m_gameTime;
                auto const t0 = Clock::now();
                liveStep(pl);
                long long const us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
                m_liveUsSum += us;
                m_liveChecks++;
                m_liveUsMax = std::max(m_liveUsMax, us);
            }
        } else if (m_queue.pending()) {
            // At most one queued input per physics step, right before the step runs.
            if (auto due = m_queue.poll(m_gameTime, TIME_EPS)) {
                fireButton(pl, *due);
                if (m_trackPending) {
                    // Align the prediction to the step the held click really landed on.
                    m_tracker.start(m_gameTime - static_cast<double>(m_pendingDelay) * getValidDt(), getValidDt(),
                                    std::move(m_pendingPath), m_pendingCheck, ACCURACY_TOL_PX);
                    m_pendingPath.clear();
                    m_trackPending = false;
                    m_trackMode = m_pendingMode;
                }
            }
        } else {
            reconcile(pl);
        }
    }
};

TrajectoryBufferSim s_sim;

// ----------------------------------------------------------------------------
//  9. The window: draggable, live, tabs Log / Stats / Settings.
//
//    * Drag it by the title bar; its position is remembered.
//    * Shown / hidden with the window key (Left Alt by default).
//      Contents refresh on their own a few times a second.
//    * While a level is actually running it is display-only: it never takes
//      your clicks (no swallowed jumps). Interact with it while paused, or
//      outside a level. The mouse wheel scrolls it any time.
// ----------------------------------------------------------------------------
// Window key name for on-screen hints ("Left Alt"), from the Window Key
// setting. cocos key codes are Windows virtual-key codes.
std::string windowKeyName() {
    auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
    if (binds.empty()) return "the Window Key";
    int const vk = static_cast<int>(binds.front().key);
    switch (vk) {
        case 0xA4: return "Left Alt";
        case 0xA5: return "Right Alt";
        case 0x12: return "Alt";
        case 0xA0: return "Left Shift";
        case 0xA1: return "Right Shift";
        case 0x10: return "Shift";
        case 0xA2: return "Left Ctrl";
        case 0xA3: return "Right Ctrl";
        case 0x11: return "Ctrl";
        case 0x09: return "Tab";
        case 0x2D: return "Insert";
        case 0x2E: return "Delete";
        case 0x24: return "Home";
        case 0x23: return "End";
        case 0x21: return "Page Up";
        case 0x22: return "Page Down";
        case 0xC0: return "`";
        default: break;
    }
    if (vk >= 0x70 && vk <= 0x87) return fmt::format("F{}", vk - 0x70 + 1);
    if ((vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A)) return std::string(1, static_cast<char>(vk));
    return "the Window Key";
}

constexpr char const* AUTHOR_NAME    = "AlQuerez";
constexpr char const* AUTHOR_DISCORD = "AlQuerez#3900";

class BufferWindow : public CCLayer {
public:
    enum Tab { TAB_LOG, TAB_STATS, TAB_SETTINGS, TAB_MODES, TAB_COUNT };
    static bool isList(Tab t) { return t == TAB_LOG || t == TAB_STATS; }

    static BufferWindow* instance() { return s_instance; }

    // Create the window if it should be shown and isn't. It lives on Geode's
    // overlay (the layer notifications use), so it stays across scenes: menus,
    // level select, levels, pause.
    static void ensure() {
        CCNode* host = OverlayManager::get();
        if (!host) return;
        if (s_instance && s_instance->getParent() == host) {
            // Window Size / Opacity changed in the settings: rebuild it.
            bool const stale = std::fabs(Config::load().hudScale - s_builtScale) > 1e-3f ||
                               std::fabs(windowOpacity() - s_builtAlpha) > 1e-3f;
            if (!stale) {
                if (!s_instance->isVisible()) {
                    s_instance->setVisible(true);
                    s_instance->popIn();
                }
                return;
            }
        }
        if (s_instance) s_instance->removeFromParent(); // from an older version / scene
        auto* w = new BufferWindow();
        if (w->init()) {
            w->autorelease();
            host->addChild(w, 100);
        } else {
            delete w;
        }
    }

    // Is this key the Window Key? Compared by key only (Shift held or not),
    // and left / right / generic Shift all count for a Shift binding.
    static bool isWindowKey(enumKeyCodes key) {
        auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
        auto shiftLike = [](enumKeyCodes k) { return k == KEY_Shift || k == KEY_LeftShift || k == KEY_RightShift; };
        for (Keybind const& b : binds) {
            if (b.key == key) return true;
            if (b.key == KEY_LeftMenu && key == KEY_Alt) return true;
            if (b.key == KEY_LeftShift && key == KEY_Shift) return true;
            if (b.key == KEY_Shift && shiftLike(key)) return true;
        }
        return false;
    }

    // Window key: flip "window shown".
    static void toggle() {
        auto* mod = Mod::get();
        // Shown in settings but hidden on screen (e.g. after "All settings"): just bring it back.
        if (mod->getSavedValue<bool>("window-shown", true) && s_instance && !s_instance->isVisible()) {
            ensure();
            return;
        }
        bool const on = !mod->getSavedValue<bool>("window-shown", true);
        mod->setSavedValue<bool>("window-shown", on);
        s_sim.settingsChanged();
        if (on) ensure();
        else if (s_instance) s_instance->setVisible(false);
    }

protected:
    static inline BufferWindow* s_instance = nullptr;

    // Layout (window-local units; the whole window is scaled by "Window Size").
    static constexpr float W       = 320.f;
    static constexpr float H       = 214.f;
    static constexpr float TITLE_H = 18.f;
    static constexpr float TABS_H  = 16.f;
    static constexpr float FOOT_H  = 14.f;
    static constexpr float LINE_H  = 11.f;
    static constexpr float PAD     = 6.f;
    static constexpr float SCROLL_W = 12.f;  // right strip of list tabs: arrows + scrollbar
    static constexpr float LOG_TIME_W = 24.f; // log: time column (right-aligned to this x)
    static constexpr float LOG_TEXT_X = 62.f; // log: where the text starts (after the tag)
    static constexpr int   PRIO_WINDOW = -510; // window body: swallows touches inside it
    static constexpr int   PRIO_MENUS  = -520; // its buttons: ahead of the body

    // Palette (1.25.7): midnight blue fading up into purple, violet accents.
    static inline const ccColor3B C_FRAME   = {9, 8, 24};
    static inline const ccColor4B C_GRAD_TOP = {48, 24, 86, 235};   // body gradient: purple at the top...
    static inline const ccColor4B C_GRAD_BOT = {11, 13, 38, 235};   // ...midnight at the bottom
    static inline const ccColor3B C_CARD    = {28, 20, 58};
    static inline const ccColor3B C_BTN     = {50, 38, 92};         // button / tab box
    static inline const ccColor3B C_BTN_SEL = {128, 76, 236};       // selected tab / stage
    static inline const ccColor3B C_BTN_HOT = {196, 72, 168};       // unsaved Apply
    static inline const ccColor3B C_PILL    = {36, 26, 70};
    static inline const ccColor3B C_LAV     = {198, 166, 255};      // card titles
    static inline const ccColor3B C_MUTED   = {168, 158, 205};
    static inline const ccColor3B C_DIM     = {120, 110, 160};

    Tab   m_tab = TAB_LOG;
    int   m_offset = 0;            // first visible row
    CCNode*        m_body = nullptr;     // the full window (everything below is in here)
    CCNode*        m_pill = nullptr;     // small pill shown instead while a level runs
    CCDrawNode*    m_pillBg = nullptr;
    CCLabelBMFont* m_pillStage = nullptr;
    CCLabelBMFont* m_pillTag = nullptr;
    CCLabelBMFont* m_pillText = nullptr;
    unsigned long long m_pillVersion = ~0ull;
    std::string    m_pillStageText;
    bool           m_compactNow = false; // currently showing the pill
    bool           m_compactOn = true;   // "Small while playing" (cached)
    float          m_compactCheck = 0.f;
    CCNode*        m_tabSlots = nullptr; // the four tab boxes (static)
    CCDrawNode*    m_tabHi = nullptr;    // the selected tab's highlight (slides)
    bool           m_tabHiPlaced = false;
    float          m_alpha = 1.f;        // Window Opacity setting (panels only, text stays solid)
    static inline float s_builtScale = -1.f, s_builtAlpha = -1.f;
    int   m_rowCount = 0;
    bool  m_dragging = false;
    bool  m_scrollDrag = false;
    float m_dragAccum = 0.f;
    float m_refreshAt = 0.f;       // seconds until the next content refresh
    bool  m_interactive = true;

    CCLabelBMFont* m_header = nullptr;
    CCLayerGradient* m_accent = nullptr;   // line under the title
    CCDrawNode*    m_headerPill = nullptr;
    CCNode*        m_stripes = nullptr;
    CCLayerColor*  m_scrollTrack = nullptr;
    CCLayerColor*  m_scrollThumb = nullptr;
    CCLabelBMFont* m_footer = nullptr;
    CCMenu* m_titleMenu = nullptr; // close button
    CCMenu* m_tabMenu = nullptr;
    CCMenu* m_listMenu = nullptr;  // scroll arrows / list buttons
    CCMenu* m_settingsMenu = nullptr;
    CCNode* m_settingsLabels = nullptr;
    CCNode* m_cards = nullptr;     // card panels behind the Stage / Modes pages
    std::vector<CCLabelBMFont*> m_rows;
    std::vector<CCLabelBMFont*> m_rowsR; // right-aligned values ("label<TAB>value" rows)
    std::vector<CCLabelBMFont*> m_rowsH; // bold section titles (rows starting with '\x01') / log tags
    std::vector<CCLabelBMFont*> m_rowsT; // log time column
    std::vector<std::pair<std::string, ccColor3B>> m_data;

    ~BufferWindow() override {
        if (s_instance == this) s_instance = nullptr;
    }

    static float contentTop()    { return H - TITLE_H - TABS_H - 2.f; }
    static float contentBottom() { return FOOT_H + 2.f; }
    static int   visibleRows()   { return static_cast<int>((contentTop() - contentBottom()) / LINE_H); }
    static float trackTop()      { return contentTop() - 12.f; }
    static float trackBottom()   { return contentBottom() + 12.f; }

    bool init() override {
        if (!CCLayer::init()) return false;
        s_instance = this;
        setContentSize({W, H});
        ignoreAnchorPointForPosition(false);
        setAnchorPoint({0.f, 0.f});

        Config const c = Config::load();
        // Window Size 1.0 = half the original size.
        setScale(0.5f * std::clamp(c.hudScale, 0.5f, 2.0f));
        m_alpha = windowOpacity();
        s_builtScale = c.hudScale;
        s_builtAlpha = m_alpha;
        auto op = [&](float a) { return static_cast<GLubyte>(std::clamp(a * m_alpha, 0.f, 255.f)); };

        // Everything lives in m_body (centred anchor, so it can pop in from the
        // middle); the in-game pill is a sibling.
        m_body = CCNode::create();
        m_body->setContentSize({W, H});
        m_body->setAnchorPoint({0.5f, 0.5f});
        m_body->setPosition({W / 2.f, H / 2.f});
        this->addChild(m_body, 0);

        // Frame, then the body: a vertical gradient, purple at the top fading
        // into midnight blue at the bottom (inset so the frame's round corners
        // stay visible).
        m_body->addChild(panel(W, H, C_FRAME, op(245.f), 7.f), 0);
        ccColor4B gTop = C_GRAD_TOP, gBot = C_GRAD_BOT;
        gTop.a = op(gTop.a);
        gBot.a = op(gBot.a);
        auto* body = CCLayerGradient::create(gTop, gBot);
        body->setContentSize({W - 6.f, H - TITLE_H - FOOT_H - 1.f});
        body->setPosition({3.f, FOOT_H});
        m_body->addChild(body, 0);

        // Accent line under the title: violet into pink.
        m_accent = CCLayerGradient::create(ccColor4B{150, 90, 255, 230}, ccColor4B{235, 90, 200, 230}, CCPoint{1.f, 0.f});
        m_accent->setContentSize({W - 16.f, 1.f});
        m_accent->setPosition({8.f, H - TITLE_H});
        m_body->addChild(m_accent, 1);

        // Title + "by AlQuerez".
        // White title (goldFont's yellow is baked into the font, so it can't be tinted white).
        auto* title = CCLabelBMFont::create("Input Buffer", "bigFont.fnt");
        title->setScale(0.38f);
        title->setColor({255, 255, 255});
        title->setAnchorPoint({0.f, 0.5f});
        title->setPosition({PAD + 2.f, H - TITLE_H / 2.f + 0.5f});
        m_body->addChild(title, 2);
        // "V2" badge right after the title: violet pill, white text.
        float const titleEnd = PAD + 2.f + title->getContentSize().width * 0.38f;
        float const badgeW = 15.f, badgeH = 9.f;
        auto* badge = panel(badgeW, badgeH, C_BTN_SEL, 255, 3.f);
        badge->setPosition({titleEnd + 4.f, H - TITLE_H / 2.f - badgeH / 2.f + 0.5f});
        m_body->addChild(badge, 2);
        auto* v2 = CCLabelBMFont::create("V2", "bigFont.fnt");
        v2->setScale(0.2f);
        v2->setColor({255, 255, 255});
        v2->setPosition({titleEnd + 4.f + badgeW / 2.f, H - TITLE_H / 2.f + 0.5f});
        m_body->addChild(v2, 3);
        auto* by = CCLabelBMFont::create(fmt::format("by {}", AUTHOR_NAME).c_str(), "chatFont.fnt");
        by->setScale(0.42f);
        by->setColor(C_DIM);
        by->setAnchorPoint({0.f, 0.5f});
        by->setPosition({titleEnd + 4.f + badgeW + 5.f, H - TITLE_H / 2.f});
        m_body->addChild(by, 2);

        // Status pill (stage, or what the buffer is doing) on the right.
        m_headerPill = panel(60.f, 12.f, C_PILL, 240);
        m_headerPill->setAnchorPoint({1.f, 0.5f});
        m_headerPill->setPosition({W - 21.f, H - TITLE_H / 2.f});
        m_body->addChild(m_headerPill, 1);
        m_header = CCLabelBMFont::create("", "bigFont.fnt");
        m_header->setScale(0.26f);
        m_header->setAnchorPoint({1.f, 0.5f});
        m_header->setPosition({W - 26.f, H - TITLE_H / 2.f});
        m_body->addChild(m_header, 2);

        // Tabs: four static boxes, a highlight that slides to the selected one,
        // and (in buildTabs) clickable items over them - the whole box clicks.
        {
            m_tabSlots = CCNode::create();
            m_body->addChild(m_tabSlots, 1);
            float const y = H - TITLE_H - TABS_H / 2.f - 1.f;
            for (int i = 0; i < TAB_COUNT; i++) {
                auto* slot = panel(tabSegW(), TABS_H - 3.f, C_BTN, op(200.f));
                slot->setPosition({tabCenterX(i) - tabSegW() / 2.f, y - (TABS_H - 3.f) / 2.f});
                m_tabSlots->addChild(slot);
            }
            m_tabHi = panel(tabSegW(), TABS_H - 3.f, C_BTN_SEL, 255);
            m_body->addChild(m_tabHi, 2);
        }

        // Card panels (Stage / Modes) sit under the page content.
        m_cards = CCNode::create();
        m_body->addChild(m_cards, 1);

        // Zebra stripes behind the list rows.
        m_stripes = CCNode::create();
        m_body->addChild(m_stripes, 1);
        for (int i = 1; i < visibleRows(); i += 2) {
            auto* st = CCLayerColor::create(ccColor4B{170, 130, 255, 12}, W - 2.f * PAD - SCROLL_W + 2.f, LINE_H);
            st->setPosition({PAD - 2.f, contentTop() - static_cast<float>(i + 1) * LINE_H + 1.f});
            m_stripes->addChild(st);
        }

        // Scrollbar: a faint track and a thumb sized to how much of the list is visible.
        m_scrollTrack = CCLayerColor::create(ccColor4B{255, 255, 255, 18}, 2.f, trackTop() - trackBottom());
        m_scrollTrack->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackBottom()});
        m_body->addChild(m_scrollTrack, 1);
        m_scrollThumb = CCLayerColor::create(ccColor4B{176, 136, 255, 210}, 2.f, 10.f);
        m_scrollThumb->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackBottom()});
        m_body->addChild(m_scrollThumb, 2);

        // Footer: a faint line, then the status text.
        auto* footLine = CCLayerColor::create(ccColor4B{150, 110, 255, 45}, W - 16.f, 1.f);
        footLine->setPosition({8.f, FOOT_H});
        m_body->addChild(footLine, 1);
        m_footer = CCLabelBMFont::create("", "chatFont.fnt");
        m_footer->setScale(0.45f);
        m_footer->setAnchorPoint({0.f, 0.5f});
        m_footer->setPosition({PAD + 2.f, FOOT_H / 2.f});
        m_footer->setColor(C_MUTED);
        m_body->addChild(m_footer, 2);

        m_titleMenu = makeMenu();
        // Close: a white X in a small box (GD's close button is a green sprite).
        boxButton(m_titleMenu, "X", {W - 11.f, H - TITLE_H / 2.f}, 12.f, 12.f, C_BTN, ccColor3B{255, 255, 255}, 0.3f, [] {
            BufferWindow::toggle();
            Notification::create(fmt::format("Input Buffer hidden - press {} to show it again", windowKeyName()),
                                 NotificationIcon::None, 2.0f)->show();
        });

        m_tabMenu = makeMenu();
        m_listMenu = makeMenu();
        m_settingsMenu = makeMenu();
        m_settingsLabels = CCNode::create();
        m_body->addChild(m_settingsLabels, 3);

        // Row label pool.
        for (int i = 0; i < visibleRows(); i++) {
            auto* l = CCLabelBMFont::create("", "chatFont.fnt");
            l->setAnchorPoint({0.f, 1.f});
            l->setScale(0.5f);
            l->setPosition({PAD + 1.f, contentTop() - static_cast<float>(i) * LINE_H});
            m_body->addChild(l, 2);
            m_rows.push_back(l);
            auto* r = CCLabelBMFont::create("", "chatFont.fnt");
            r->setAnchorPoint({1.f, 1.f});
            r->setScale(0.5f);
            r->setPosition({W - PAD - SCROLL_W - 2.f, contentTop() - static_cast<float>(i) * LINE_H});
            m_body->addChild(r, 2);
            m_rowsR.push_back(r);
            auto* h = CCLabelBMFont::create("", "bigFont.fnt");
            h->setAnchorPoint({0.f, 0.5f});
            h->setScale(0.3f);
            h->setColor({255, 255, 255});
            h->setPosition({PAD + 1.f, contentTop() - static_cast<float>(i) * LINE_H - LINE_H / 2.f});
            m_body->addChild(h, 2);
            m_rowsH.push_back(h);
            auto* tl = CCLabelBMFont::create("", "chatFont.fnt");
            tl->setAnchorPoint({1.f, 1.f});
            tl->setScale(0.42f);
            tl->setColor(C_DIM);
            tl->setPosition({PAD + LOG_TIME_W, contentTop() - static_cast<float>(i) * LINE_H - 0.5f});
            m_body->addChild(tl, 2);
            m_rowsT.push_back(tl);
        }

        buildPill();
        restorePosition();
        setTouchEnabled(true);
        // (mouse wheel: BufMouse hook -> takeScroll, so it's handled before anything behind the window)
        scheduleUpdate();
        int const lastTab = std::clamp(static_cast<int>(Mod::get()->getSavedValue<int>("last-tab", 0)), 0, TAB_COUNT - 1);
        showTab(static_cast<Tab>(lastTab));
        popIn();
        return true;
    }

    static float windowOpacity() {
        return static_cast<float>(std::clamp(Mod::get()->getSettingValue<double>("window-opacity"), 0.3, 1.0));
    }

    // Little pop when the window appears (scale from 94% with an ease-out).
    void popIn() {
        if (!m_body) return;
        m_body->stopAllActions();
        m_body->setScale(0.94f);
        m_body->runAction(CCEaseOut::create(CCScaleTo::create(0.12f, 1.f), 2.f));
    }

    // ---- in-game pill: "EXTREME  BUFFER press +6t" while a level runs -------------
    static constexpr float PILL_H = 15.f;
    void buildPill() {
        m_pill = CCNode::create();
        m_pill->setVisible(false);
        this->addChild(m_pill, 5);
        m_pillBg = panel(80.f, PILL_H, C_PILL, static_cast<GLubyte>(std::clamp(235.f * m_alpha, 0.f, 255.f)));
        m_pill->addChild(m_pillBg, 0);
        m_pillStage = CCLabelBMFont::create("", "bigFont.fnt");
        m_pillStage->setAnchorPoint({0.f, 0.5f});
        m_pillStage->setColor({255, 255, 255});
        m_pill->addChild(m_pillStage, 1);
        m_pillTag = CCLabelBMFont::create("", "bigFont.fnt");
        m_pillTag->setAnchorPoint({0.f, 0.5f});
        m_pill->addChild(m_pillTag, 1);
        m_pillText = CCLabelBMFont::create("", "chatFont.fnt");
        m_pillText->setAnchorPoint({0.f, 0.5f});
        m_pillText->setColor(C_MUTED);
        m_pill->addChild(m_pillText, 1);
    }

    // Only rebuilt when the stage or the latest log line changed.
    void refreshPill() {
        std::string stage = s_sim.headerLine();
        if (stage.rfind("INPUT BUFFER  ", 0) == 0) stage = stage.substr(14);
        unsigned long long const v = s_sim.logVersion();
        if (stage == m_pillStageText && v == m_pillVersion) return;
        m_pillStageText = stage;
        m_pillVersion = v;
        std::string tag, text;
        ccColor3B col = COL_WHITE;
        bool const have = s_sim.lastLog(tag, text, col);
        if (text.size() > 26) text = text.substr(0, 25) + "..";
        float const y = PILL_H / 2.f;
        float x = 6.f;
        m_pillStage->setString(stage.c_str());
        m_pillStage->setScale(0.24f);
        m_pillStage->setPosition({x, y});
        x += m_pillStage->getContentSize().width * 0.24f + 7.f;
        m_pillTag->setString(have ? tag.c_str() : "");
        m_pillTag->setScale(0.19f);
        m_pillTag->setColor(logTagColor(tag));
        m_pillTag->setPosition({x, y});
        if (have) x += m_pillTag->getContentSize().width * 0.19f + 4.f;
        m_pillText->setString(have ? text.c_str() : "");
        m_pillText->setScale(0.42f);
        m_pillText->setPosition({x, y - 0.5f});
        if (have) x += m_pillText->getContentSize().width * 0.42f;
        float const w = std::max(40.f, x + 6.f);
        drawRound(m_pillBg, w, PILL_H, C_PILL, static_cast<GLubyte>(std::clamp(235.f * m_alpha, 0.f, 255.f)));
        m_pill->setPosition({0.f, H - PILL_H}); // the window's top-left corner
    }

    // Rounded box, drawn as an exact polygon (1.25.8). The 9-slice texture
    // came out square on small boxes in the game, so every button looked like a
    // block; a drawn shape has the same round corners at any size.
    static void drawRound(CCDrawNode* dn, float w, float h, ccColor3B col, GLubyte opacity, float radius = -1.f) {
        dn->clear();
        float r = radius >= 0.f ? radius : std::min(4.f, std::min(w, h) * 0.34f);
        r = std::clamp(r, 0.f, std::min(w, h) / 2.f);
        constexpr int SEG = 6; // points per quarter circle
        std::vector<CCPoint> pts;
        pts.reserve(4 * (SEG + 1));
        struct Corner { float cx, cy, a0; };
        Corner const cs[4] = {{w - r, h - r, 0.f}, {r, h - r, 90.f}, {r, r, 180.f}, {w - r, r, 270.f}};
        for (auto const& c : cs) {
            for (int j = 0; j <= SEG; j++) {
                float const ang = (c.a0 + 90.f * static_cast<float>(j) / SEG) * 3.14159265f / 180.f;
                pts.push_back(CCPoint{c.cx + r * std::cos(ang), c.cy + r * std::sin(ang)});
            }
        }
        // CCDrawNode blends premultiplied (GL_ONE, GL_ONE_MINUS_SRC_ALPHA).
        float const al = opacity / 255.f;
        ccColor4F const fill{col.r / 255.f * al, col.g / 255.f * al, col.b / 255.f * al, al};
        dn->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
        dn->setContentSize({w, h});
    }
    static CCDrawNode* panel(float w, float h, ccColor3B col, GLubyte opacity, float radius = -1.f) {
        auto* dn = CCDrawNode::create();
        dn->setAnchorPoint({0.f, 0.f});
        drawRound(dn, w, h, col, opacity, radius);
        return dn;
    }

    // A card on a page: rounded panel + small caps title, optional hint on the right.
    void card(float x, float y, float w, float h, char const* titleText, std::string const& hint = {}) {
        auto* p = panel(w, h, C_CARD, static_cast<GLubyte>(std::clamp(175.f * m_alpha, 0.f, 255.f)), 5.f);
        p->setPosition({x, y});
        m_cards->addChild(p);
        if (titleText && *titleText) {
            auto* t = CCLabelBMFont::create(titleText, "bigFont.fnt");
            t->setScale(0.22f);
            t->setColor(C_LAV);
            t->setAnchorPoint({0.f, 0.5f});
            t->setPosition({x + 6.f, y + h - 6.f});
            m_settingsLabels->addChild(t);
        }
        if (!hint.empty()) {
            auto* hl = CCLabelBMFont::create(hint.c_str(), "chatFont.fnt");
            hl->setScale(0.4f);
            hl->setColor(C_DIM);
            hl->setAnchorPoint({1.f, 0.5f});
            hl->setPosition({x + w - 6.f, y + h - 6.f});
            m_settingsLabels->addChild(hl);
        }
    }

    CCMenu* makeMenu() {
        auto* m = CCMenu::create();
        m->ignoreAnchorPointForPosition(false);
        m->setAnchorPoint({0.f, 0.f});
        m->setPosition({0.f, 0.f});
        m->setContentSize({W, H});
        m->setTouchPriority(PRIO_MENUS);
        m_body->addChild(m, 4);
        return m;
    }

    // ---- position ------------------------------------------------------------
    void restorePosition() {
        auto const win = CCDirector::sharedDirector()->getWinSize();
        float const sw = W * getScale(), sh = H * getScale();
        auto* mod = Mod::get();
        float const x = static_cast<float>(mod->getSavedValue<double>("window-x", win.width - sw - 6.0));
        float const y = static_cast<float>(mod->getSavedValue<double>("window-y", win.height - sh - 6.0));
        setPosition(clampPos({x, y}));
    }

    CCPoint clampPos(CCPoint p) {
        auto const win = CCDirector::sharedDirector()->getWinSize();
        float const sw = W * getScale(), sh = H * getScale();
        p.x = std::clamp(p.x, 0.f, std::max(0.f, win.width - sw));
        p.y = std::clamp(p.y, 0.f, std::max(0.f, win.height - sh));
        return p;
    }

    void savePosition() {
        auto* mod = Mod::get();
        mod->setSavedValue<double>("window-x", getPositionX());
        mod->setSavedValue<double>("window-y", getPositionY());
    }

    // ---- buttons: a rounded box, the WHOLE box is clickable ---------------------
    static float textWidth(char const* text, float scale) {
        auto* l = CCLabelBMFont::create(text, "bigFont.fnt");
        return l->getContentSize().width * scale;
    }

    CCMenuItemSpriteExtra* boxButton(CCMenu* menu, std::string const& text, CCPoint center, float w, float h,
                                     ccColor3B fill, ccColor3B textCol, float textScale, std::function<void()> fn,
                                     GLubyte opacity = 235) {
        auto* node = CCNode::create();
        node->setContentSize({w, h});
        node->addChild(panel(w, h, fill, opacity));
        auto* l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        l->setScale(textScale);
        limitNodeWidth(l, w - 6.f, textScale, textScale * 0.5f);
        l->setColor(textCol);
        l->setPosition({w / 2.f, h / 2.f + 0.5f});
        node->addChild(l);
        auto* item = CCMenuItemExt::createSpriteExtra(node, [fn = std::move(fn)](CCMenuItemSpriteExtra*) { fn(); });
        item->setSizeMult(1.06f);
        item->setPosition(center);
        menu->addChild(item);
        return item;
    }

    // A box sized to its text.
    CCMenuItemSpriteExtra* boxButtonFit(CCMenu* menu, char const* text, CCPoint center, ccColor3B textCol,
                                        std::function<void()> fn, float textScale = 0.24f, float h = 12.f,
                                        ccColor3B fill = C_BTN) {
        float const w = textWidth(text, textScale) + 12.f;
        return boxButton(menu, text, center, w, h, fill, textCol, textScale, std::move(fn));
    }

    // ---- tabs: four equal boxes, the highlight slides to the selected one ------------
    static constexpr float TAB_GAP = 3.f;
    static float tabSegW() {
        return (W - 2.f * PAD - TAB_GAP * static_cast<float>(TAB_COUNT - 1)) / static_cast<float>(TAB_COUNT);
    }
    static float tabCenterX(int i) { return PAD + tabSegW() / 2.f + static_cast<float>(i) * (tabSegW() + TAB_GAP); }

    void buildTabs() {
        m_tabMenu->removeAllChildren();
        char const* names[TAB_COUNT] = {"Log", "Stats", "Stage", "Modes"};
        float const y = H - TITLE_H - TABS_H / 2.f - 1.f;
        for (int i = 0; i < TAB_COUNT; i++) {
            bool const sel = i == static_cast<int>(m_tab);
            // invisible box (opacity 0) over the static slot: the whole box clicks
            auto* item = boxButton(m_tabMenu, names[i], {tabCenterX(i), y}, tabSegW(), TABS_H - 3.f, C_BTN,
                                   sel ? ccColor3B{255, 255, 255} : C_MUTED, 0.27f,
                                   [this, i] { showTab(static_cast<Tab>(i)); }, 0);
            item->setSizeMult(1.02f);
        }
        if (m_tabHi) {
            CCPoint const to{tabCenterX(static_cast<int>(m_tab)) - tabSegW() / 2.f, y - (TABS_H - 3.f) / 2.f};
            m_tabHi->stopAllActions();
            if (!m_tabHiPlaced) {
                m_tabHi->setPosition(to);
                m_tabHiPlaced = true;
            } else {
                m_tabHi->runAction(CCEaseOut::create(CCMoveTo::create(0.12f, to), 2.5f));
            }
        }
    }

    void showTab(Tab t) {
        m_tab = t;
        m_offset = 0;
        m_forceRefresh = true;
        Mod::get()->setSavedValue<int>("last-tab", static_cast<int>(t));
        buildTabs();
        bool const list = isList(t);
        for (auto* l : m_rows) l->setVisible(list);
        for (auto* l : m_rowsR) l->setVisible(list);
        for (auto* l : m_rowsH) l->setVisible(list);
        for (auto* l : m_rowsT) l->setVisible(list);
        if (m_stripes) m_stripes->setVisible(list);
        if (m_scrollTrack) m_scrollTrack->setVisible(false); // updateScrollbar shows them when the list scrolls
        if (m_scrollThumb) m_scrollThumb->setVisible(false);
        m_listMenu->setVisible(list);
        m_settingsMenu->setVisible(!list);
        m_settingsLabels->setVisible(!list);
        m_cards->setVisible(!list);
        m_listMenu->removeAllChildren();
        m_settingsMenu->removeAllChildren();
        m_settingsLabels->removeAllChildren();
        m_cards->removeAllChildren();
        if (list) buildListButtons();
        else if (t == TAB_MODES) buildModes();
        else buildSettings();
        refresh();
    }

    void buildListButtons() {
        float const x = W - PAD - SCROLL_W / 2.f;
        boxButton(m_listMenu, "^", {x, contentTop() - 5.f}, 10.f, 10.f, C_BTN, C_MUTED, 0.26f,
                  [this] { scrollBy(-visibleRows() + 1); });
        boxButton(m_listMenu, "v", {x, contentBottom() + 5.f}, 10.f, 10.f, C_BTN, C_MUTED, 0.26f,
                  [this] { scrollBy(visibleRows() - 1); });
        if (m_tab == TAB_LOG) {
            // In the footer, out of the tab bar.
            boxButtonFit(m_listMenu, "clear", {W - PAD - 15.f, FOOT_H / 2.f}, COL_ORANGE, [this] {
                s_sim.clearHistory();
                m_forceRefresh = true;
                refresh();
            }, 0.22f, 10.f);
        }
    }

    // ---- list content -----------------------------------------------------------
    void scrollBy(int rows) {
        int const maxOff = std::max(0, m_rowCount - visibleRows());
        m_offset = std::clamp(m_offset + rows, 0, maxOff);
        drawRows();
    }

    // Only what changed is rebuilt (1.26.0): text labels are rebuilt glyph by
    // glyph, and doing all of them 4x a second while you play cost frame time.
    std::string m_shownHeader, m_shownFooter;
    unsigned long long m_shownLogVersion = ~0ull;
    float m_statsAge = 0.f;
    bool  m_forceRefresh = true;

    void refresh() {
        bool const force = m_forceRefresh;
        m_forceRefresh = false;
        std::string head = s_sim.headerLine();
        if (head.rfind("INPUT BUFFER  ", 0) == 0) head = head.substr(14); // the title already says it
        if (force || head != m_shownHeader) {
            m_shownHeader = head;
            m_header->setString(head.c_str());
            m_header->setColor({255, 255, 255});
            m_header->setScale(0.26f);
            limitNodeWidth(m_header, W - 170.f, 0.26f, 0.15f);
            if (m_headerPill) {
                float const hw = m_header->getContentSize().width * m_header->getScale() + 10.f;
                drawRound(m_headerPill, hw, 12.f, C_PILL, 240);
            }
        }
        std::string const foot = s_sim.statusLine();
        if (force || foot != m_shownFooter) {
            m_shownFooter = foot;
            m_footer->setString(foot.c_str());
            m_footer->setScale(0.45f);
            // room for "clear" on the Log tab
            limitNodeWidth(m_footer, W - 2.f * PAD - (m_tab == TAB_LOG ? 34.f : 4.f), 0.45f, 0.25f);
        }
        if (!isList(m_tab)) return;
        if (m_tab == TAB_LOG) {
            if (!force && s_sim.logVersion() == m_shownLogVersion) return; // nothing new
            m_shownLogVersion = s_sim.logVersion();
        } else {
            // Stats: about once a second while a level runs, 4x a second otherwise.
            if (!force && levelRunning() && m_statsAge < 1.f) return;
            m_statsAge = 0.f;
        }
        Config const c = Config::load();
        m_data = (m_tab == TAB_LOG) ? s_sim.historyRows(c.hudVerbose) : s_sim.statsRows();
        if (m_tab == TAB_LOG && m_data.empty()) {
            // Empty log: say what will show up here, and how to hide the window.
            m_data.emplace_back("Nothing logged yet.", COL_WHITE);
            m_data.emplace_back("Buffered clicks, when they fired, and deaths show up here while you play.", C_MUTED);
            m_data.emplace_back(fmt::format("{} shows / hides this window. Drag it by the title bar.", windowKeyName()), C_MUTED);
        }
        m_rowCount = static_cast<int>(m_data.size());
        scrollBy(0);
    }

    void drawRows() {
        float const full = W - 2.f * PAD - SCROLL_W - 3.f;
        for (size_t i = 0; i < m_rows.size(); i++) {
            size_t const idx = static_cast<size_t>(m_offset) + i;
            auto* l = m_rows[i];
            auto* r = m_rowsR[i];
            auto* h = m_rowsH[i];
            auto* tl = m_rowsT[i];
            float const rowY = contentTop() - static_cast<float>(i) * LINE_H;
            h->setString("");
            tl->setString("");
            l->setPosition({PAD + 1.f, rowY});
            h->setPosition({PAD + 1.f, rowY - LINE_H / 2.f});
            if (idx < m_data.size()) {
                std::string const& text = m_data[idx].first;
                size_t const tab = text.find('\t');
                if (!text.empty() && text[0] == '\x02') {
                    // log line: time | TAG | text
                    size_t const a = text.find('\x1f');
                    size_t const b = (a == std::string::npos) ? std::string::npos : text.find('\x1f', a + 1);
                    if (b != std::string::npos) {
                        std::string const tag = text.substr(a + 1, b - a - 1);
                        tl->setString(text.substr(1, a - 1).c_str());
                        tl->setScale(0.42f);
                        h->setString(tag.c_str());
                        h->setColor(logTagColor(tag));
                        h->setScale(0.2f);
                        h->setPosition({PAD + LOG_TIME_W + 5.f, rowY - LINE_H / 2.f});
                        l->setString(text.substr(b + 1).c_str());
                        l->setColor(m_data[idx].second);
                        l->setScale(0.5f);
                        l->setPosition({PAD + LOG_TEXT_X, rowY});
                        limitNodeWidth(l, full - LOG_TEXT_X + 1.f, 0.5f, 0.25f);
                    } else {
                        l->setString(text.substr(1).c_str());
                    }
                    r->setString("");
                } else if (!text.empty() && text[0] == '\x01') {
                    h->setColor({255, 255, 255});
                    // section title: bold, white
                    l->setString("");
                    r->setString("");
                    h->setString(text.substr(1).c_str());
                    h->setScale(0.3f);
                    limitNodeWidth(h, full, 0.3f, 0.15f);
                } else if (tab == std::string::npos) {
                    // plain row (log lines, section titles, notes)
                    l->setString(text.c_str());
                    l->setColor(m_data[idx].second);
                    l->setScale(0.5f);
                    limitNodeWidth(l, full, 0.5f, 0.25f);
                    r->setString("");
                } else {
                    // label (muted) on the left, value (row colour) on the right
                    r->setString(text.substr(tab + 1).c_str());
                    r->setColor(m_data[idx].second);
                    r->setScale(0.5f);
                    limitNodeWidth(r, full * 0.55f, 0.5f, 0.25f);
                    float const valueW = r->getContentSize().width * r->getScale();
                    l->setString(text.substr(0, tab).c_str());
                    l->setColor(C_MUTED);
                    l->setScale(0.5f);
                    limitNodeWidth(l, std::max(40.f, full - valueW - 8.f), 0.5f, 0.25f);
                }
            } else {
                l->setString("");
                r->setString("");
            }
        }
        updateScrollbar();
    }

    // Thumb size = visible share of the list; position = scroll offset.
    void updateScrollbar() {
        if (!m_scrollThumb || !m_scrollTrack) return;
        int const vis = visibleRows();
        bool const scrollable = m_rowCount > vis;
        m_scrollTrack->setVisible(isList(m_tab) && scrollable);
        m_scrollThumb->setVisible(isList(m_tab) && scrollable);
        if (!scrollable) return;
        float const trackH = trackTop() - trackBottom();
        // (track spans between the two arrow boxes)
        float const thumbH = std::max(8.f, trackH * static_cast<float>(vis) / static_cast<float>(m_rowCount));
        int const maxOff = std::max(1, m_rowCount - vis);
        float const t = static_cast<float>(m_offset) / static_cast<float>(maxOff); // 0 = top
        m_scrollThumb->setContentSize({2.f, thumbH});
        m_scrollThumb->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackTop() - thumbH - t * (trackH - thumbH)});
    }

    // ---- stage page ------------------------------------------------------------
    // What each stage means, in plain words.
    static std::string stageBlurb(int st) {
        switch (st) {
            case 0:  return "Raw input. The buffer does nothing.";
            case 1:  return "Saves clicks up to 1 frame early (1f @60fps).";
            case 2:  return "Saves clicks up to 2 frames early.";
            case 3:  return "4 frames early (3 for ship / wave).";
            case 4:  return "8 frames early (5 for ship / wave).";
            case 5:  return "20 frames early (12 for ship / wave).";
            case 6: {
                CustomFrames const f = CustomFrames::load();
                return fmt::format("Your frames: cube {:g}, ship {:g}, wave {:g} (Extreme engine).", f.cube, f.ship, f.wave);
            }
            default: return "";
        }
    }

    // Custom frames being edited (applied with the Apply button).
    CustomFrames m_customEdit = CustomFrames::load();

    // A toggle + label pair on a card.
    void addToggle(float x, float y, char const* label, char const* key, bool def) {
        bool const on = Mod::get()->getSavedValue<bool>(key, def);
        std::string const k = key;
        auto* tog = CCMenuItemExt::createTogglerWithStandardSprites(0.4f, [k, def](CCMenuItemToggler*) {
            auto* mod = Mod::get();
            mod->setSavedValue<bool>(k, !mod->getSavedValue<bool>(k, def));
            s_sim.settingsChanged();
        });
        tog->toggle(on);
        tog->setPosition({x + 8.f, y});
        m_settingsMenu->addChild(tog);
        auto* l = CCLabelBMFont::create(label, "bigFont.fnt");
        l->setScale(0.25f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({x + 19.f, y});
        m_settingsLabels->addChild(l);
    }

    //  Stage tab, three cards:
    //    STAGE    [Off][1][2][3][4][Extreme][Custom]   name + what it does
    //    CUSTOM FRAMES   Cube [-] 6 [+]  Ship ...  [Apply]
    //    OPTIONS  toggles, forget level, all settings
    //  and the credit line under them.
    void buildSettings() {
        int const cur = s_sim.currentStage();
        float const cx = PAD, cw = W - 2.f * PAD;

        // -- STAGE card (117..176)
        card(cx, 117.f, cw, 59.f, "STAGE", "F6 cycles stages");
        char const* labels[] = {"Off", "1", "2", "3", "4", "Extreme", "Custom"};
        float const by = 152.f;
        constexpr float BTN_H = 15.f, BTN_GAP = 4.f, BTN_TEXT = 0.28f;
        std::vector<float> widths;
        float total = 0.f;
        for (int i = 0; i < STAGE_COUNT; i++) {
            widths.push_back(std::max(20.f, textWidth(labels[i], BTN_TEXT) + 14.f));
            total += widths.back();
        }
        total += BTN_GAP * static_cast<float>(STAGE_COUNT - 1);
        float x = std::max(cx + 4.f, (W - total) / 2.f);
        for (int i = 0; i < STAGE_COUNT; i++) {
            bool const sel = (i == cur);
            boxButton(m_settingsMenu, labels[i], {x + widths[i] / 2.f, by}, widths[i], BTN_H, sel ? C_BTN_SEL : C_BTN,
                      sel ? ccColor3B{255, 255, 255} : C_MUTED, BTN_TEXT, [this, i] {
                          s_sim.setStage(i);
                          showTab(TAB_SETTINGS);
                      }, sel ? 255 : 220);
            x += widths[i] + BTN_GAP;
        }
        auto* name = CCLabelBMFont::create(STAGES[cur].name, "bigFont.fnt");
        name->setScale(0.3f);
        name->setColor({255, 255, 255});
        limitNodeWidth(name, cw - 12.f, 0.3f, 0.18f);
        name->setPosition({W / 2.f, 137.f});
        m_settingsLabels->addChild(name);
        auto* blurb = CCLabelBMFont::create(stageBlurb(cur).c_str(), "chatFont.fnt");
        blurb->setScale(0.48f);
        blurb->setColor(C_MUTED);
        limitNodeWidth(blurb, cw - 12.f, 0.48f, 0.28f);
        blurb->setPosition({W / 2.f, 125.5f});
        m_settingsLabels->addChild(blurb);

        // -- CUSTOM FRAMES card (80..114)
        card(cx, 80.f, cw, 34.f, "CUSTOM FRAMES", "frames @60fps, 0.5 steps");
        buildCustomRow(92.f);

        // -- OPTIONS card (30..77): title row has the two buttons, then 2x2 toggles
        card(cx, 30.f, cw, 47.f, "OPTIONS");
        float const colL = cx + 4.f, colR = W / 2.f + 2.f;
        boxButton(m_settingsMenu, "forget level", {W - PAD - 108.f, 70.f}, 62.f, 11.f, C_BTN, COL_ORANGE, 0.21f, [] {
            if (s_sim.inLevelWithSpots()) s_sim.forgetSpots();
            else Notification::create("Open the level first (pause it), then press forget level", NotificationIcon::None, 1.8f)->show();
        });
        boxButton(m_settingsMenu, "all settings", {W - PAD - 38.f, 70.f}, 62.f, 11.f, C_BTN, C_LAV, 0.21f, [this] {
            setVisible(false); // out of the way of the settings popup; comes back with the window key
            openSettingsPopup(Mod::get());
        });
        addToggle(colL, 55.f, "Show paths", "show-paths", false);
        addToggle(colR, 55.f, "Learn from attempts", "learn-spots", true);
        addToggle(colL, 39.f, "Log: show safe clicks", "log-safe-clicks", false);
        addToggle(colR, 39.f, "Small while playing", "compact-play", true);

        // -- credit line (between the cards and the footer)
        auto* credit = CCLabelBMFont::create(
            fmt::format("Discord: {}     {}: show / hide this window", AUTHOR_DISCORD, windowKeyName()).c_str(),
            "chatFont.fnt");
        credit->setScale(0.45f);
        credit->setColor(C_DIM);
        limitNodeWidth(credit, W - 2.f * PAD, 0.45f, 0.25f);
        credit->setPosition({W / 2.f, 22.f});
        m_settingsLabels->addChild(credit);
    }

    // ---- modes page ------------------------------------------------------------------
    //  Which gamemodes the buffer works in. Two cards; each mode has a normal
    //  and a mini toggle. Off = clicks in that mode go through raw.
    void buildModes() {
        float const top = contentTop() - 10.f;
        float const colW = (W - 2.f * PAD - 6.f) / 2.f;
        float const colX[2] = {PAD, PAD + colW + 6.f};
        int const perCol = (MODE_TOGGLE_COUNT + 1) / 2;
        float const cardBottom = top - 18.f - static_cast<float>(perCol - 1) * 24.f - 12.f;
        for (int c = 0; c < 2; c++) card(colX[c], cardBottom, colW, top + 7.f - cardBottom, nullptr);
        auto caption = [&](char const* text, float x, float y, ccColor3B col, float scale, bool left = false) {
            auto* l = CCLabelBMFont::create(text, "bigFont.fnt");
            l->setScale(scale);
            l->setColor(col);
            if (left) l->setAnchorPoint({0.f, 0.5f});
            l->setPosition({x, y});
            m_settingsLabels->addChild(l);
        };
        for (int c = 0; c < 2; c++) {
            caption("normal", colX[c] + colW - 52.f, top, C_LAV, 0.2f);
            caption("mini", colX[c] + colW - 20.f, top, C_LAV, 0.2f);
        }
        for (int i = 0; i < MODE_TOGGLE_COUNT; i++) {
            int const col = i / perCol, row = i % perCol;
            float const x = colX[col];
            float const y = top - 18.f - static_cast<float>(row) * 24.f;
            caption(MODE_TOGGLES[i].label, x + 8.f, y, COL_WHITE, 0.3f, true);
            for (int mini = 0; mini < 2; mini++) {
                std::string const id = mini ? fmt::format("mini-{}", MODE_TOGGLES[i].id) : std::string(MODE_TOGGLES[i].id);
                auto* tog = CCMenuItemExt::createTogglerWithStandardSprites(0.42f, [id](CCMenuItemToggler*) {
                    auto* mod = Mod::get();
                    mod->setSavedValue<bool>("mode-on-" + id, !mod->getSavedValue<bool>("mode-on-" + id, true));
                    s_sim.settingsChanged();
                });
                tog->toggle(modeOn(id));
                tog->setPosition({x + colW - (mini ? 20.f : 52.f), y});
                m_settingsMenu->addChild(tog);
            }
        }
        auto setAll = [this](bool on) {
            auto* mod = Mod::get();
            for (auto const& m : MODE_TOGGLES) {
                mod->setSavedValue<bool>(fmt::format("mode-on-{}", m.id), on);
                mod->setSavedValue<bool>(fmt::format("mode-on-mini-{}", m.id), on);
            }
            s_sim.settingsChanged();
            showTab(TAB_MODES);
        };
        float const by = std::min(cardBottom - 12.f, contentBottom() + 12.f);
        boxButton(m_settingsMenu, "all on", {PAD + 24.f, by}, 44.f, 13.f, C_BTN, COL_GREEN, 0.24f, [setAll] { setAll(true); });
        boxButton(m_settingsMenu, "all off", {PAD + 74.f, by}, 44.f, 13.f, C_BTN, COL_ORANGE, 0.24f, [setAll] { setAll(false); });
        auto* hint = CCLabelBMFont::create("Off = clicks in that mode go through raw", "chatFont.fnt");
        hint->setScale(0.45f);
        hint->setColor(C_DIM);
        hint->setAnchorPoint({1.f, 0.5f});
        hint->setPosition({W - PAD - 4.f, by});
        m_settingsLabels->addChild(hint);
    }

    // ---- custom stage row ------------------------------------------------------------
    //  "Cube [-] 6 [+]  Ship [-] 4 [+]  Wave [-] 4 [+]  [Apply]"
    //  +/- change the numbers here; Apply saves them and switches to Custom.
    void buildCustomRow(float y) {
        CustomFrames const saved = CustomFrames::load();
        bool const dirty = std::fabs(saved.cube - m_customEdit.cube) > 1e-3f ||
                           std::fabs(saved.ship - m_customEdit.ship) > 1e-3f ||
                           std::fabs(saved.wave - m_customEdit.wave) > 1e-3f;
        bool const onCustom = s_sim.currentStage() == STAGE_CUSTOM;

        // One "Name [-] value [+]" group, 0.5-frame steps.
        auto group = [&](char const* label, float& value, float gx) {
            auto* l = CCLabelBMFont::create(label, "bigFont.fnt");
            l->setScale(0.25f);
            l->setAnchorPoint({0.f, 0.5f});
            l->setPosition({gx, y});
            m_settingsLabels->addChild(l);
            float const bx = gx + 33.f;
            float* v = &value;
            boxButton(m_settingsMenu, "-", {bx, y}, 12.f, 12.f, C_BTN, C_LAV, 0.3f, [this, v] {
                *v = std::max(CUSTOM_MIN_FRAMES, *v - 0.5f);
                showTab(TAB_SETTINGS);
            });
            auto* num = CCLabelBMFont::create(fmt::format("{:g}", value).c_str(), "bigFont.fnt");
            num->setScale(0.3f);
            num->setColor(COL_WHITE);
            num->setPosition({bx + 14.f, y});
            m_settingsLabels->addChild(num);
            boxButton(m_settingsMenu, "+", {bx + 28.f, y}, 12.f, 12.f, C_BTN, C_LAV, 0.3f, [this, v] {
                *v = std::min(CUSTOM_MAX_FRAMES, *v + 0.5f);
                showTab(TAB_SETTINGS);
            });
        };
        group("Cube", m_customEdit.cube, PAD + 8.f);
        group("Ship", m_customEdit.ship, PAD + 80.f);
        group("Wave", m_customEdit.wave, PAD + 152.f);

        // Apply: save and switch to Custom. Highlighted while there are unsaved changes.
        char const* applyText = dirty ? "Apply*" : (onCustom ? "Applied" : "Apply");
        float const aw = 52.f;
        boxButton(m_settingsMenu, applyText, {W - PAD - 6.f - aw / 2.f, y}, aw, 14.f,
                  dirty ? C_BTN_HOT : (onCustom ? C_BTN : C_BTN_SEL), ccColor3B{255, 255, 255}, 0.26f, [this] {
                      m_customEdit.save();
                      s_sim.setStage(STAGE_CUSTOM); // reloads the config with the new frames
                      showTab(TAB_SETTINGS);
                  });
    }

    // ---- live update -------------------------------------------------------------
    static bool levelRunning() {
        PlayLayer* pl = PlayLayer::get();
        return pl && !pl->m_isPaused;
    }

    void update(float dt) override {
        // Display-only while the level runs: buttons off so clicks stay jumps.
        bool const interactive = !levelRunning();
        if (interactive != m_interactive) {
            m_interactive = interactive;
            for (CCMenu* m : {m_titleMenu, m_tabMenu, m_listMenu, m_settingsMenu}) m->setEnabled(interactive);
            m_dragging = m_scrollDrag = false;
        }
        // Small pill while a level runs ("Small while playing"), the full window
        // when paused / in menus. The hidden window costs nothing per frame.
        m_compactCheck -= dt;
        if (m_compactCheck <= 0.f) { // the setting, re-read twice a second (not every frame)
            m_compactCheck = 0.5f;
            m_compactOn = Mod::get()->getSavedValue<bool>("compact-play", true);
        }
        bool const compact = m_compactOn && levelRunning();
        if (compact != m_compactNow) {
            m_compactNow = compact;
            m_body->setVisible(!compact);
            m_pill->setVisible(compact);
            if (compact) {
                m_pillVersion = ~0ull; // redraw it now
            } else {
                m_forceRefresh = true;
                popIn();
            }
        }
        m_refreshAt -= dt;
        m_statsAge += dt;
        if (m_compactNow) {
            if (m_refreshAt <= 0.f && isVisible()) {
                m_refreshAt = 0.25f;
                refreshPill();
            }
            return;
        }
        if (m_refreshAt <= 0.f && isVisible()) {
            m_refreshAt = 0.25f;
            refresh();
        }
    }

    // ---- touches: drag by the title bar, drag-scroll the list ---------------------
    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, PRIO_WINDOW, true);
    }

    bool insideLocal(CCPoint p) const { return p.x >= 0.f && p.y >= 0.f && p.x <= W && p.y <= H; }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!isVisible() || levelRunning()) return false;
        CCPoint const p = convertToNodeSpace(touch->getLocation());
        if (!insideLocal(p)) return false;
        m_dragging = p.y >= H - TITLE_H;
        m_scrollDrag = !m_dragging && isList(m_tab) && p.y > contentBottom() && p.y < contentTop();
        m_dragAccum = 0.f;
        return true; // swallow: nothing behind the window gets this touch
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        CCPoint const d = touch->getDelta();
        if (m_dragging) {
            setPosition(clampPos({getPositionX() + d.x, getPositionY() + d.y}));
        } else if (m_scrollDrag) {
            m_dragAccum += d.y / getScale();
            while (m_dragAccum >= LINE_H)  { m_dragAccum -= LINE_H; scrollBy(1); }
            while (m_dragAccum <= -LINE_H) { m_dragAccum += LINE_H; scrollBy(-1); }
        }
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (m_dragging) savePosition();
        m_dragging = m_scrollDrag = false;
    }

    void ccTouchCancelled(CCTouch* t, CCEvent* e) override { ccTouchEnded(t, e); }

public:
    // Mouse wheel over the window (1.25.9): the window gets it FIRST and it
    // stops there - before, a level list behind the window scrolled too.
    // Called from the CCMouseDispatcher hook; true = handled, don't pass it on.
    static bool takeScroll(float y) {
        BufferWindow* w = s_instance;
        if (!w || !w->isVisible() || !w->getParent() || w->m_compactNow) return false;
        CCPoint const p = w->convertToNodeSpace(getMousePos());
        if (!w->insideLocal(p)) return false;
        if (isList(w->m_tab) && y != 0.f) w->scrollBy(y > 0.f ? 3 : -3);
        return true; // over the window: nothing behind it scrolls
    }
};

void windowEnsure() { BufferWindow::ensure(); }

} // namespace

#ifdef GEODE_IS_WINDOWS
// Window Key on Windows: read the key itself every frame. Input mods (Click
// Between Frames) take over the keyboard and Shift on its own never reaches
// GD's key handling, so events can't be relied on. Only while GD is the
// focused window, never while typing in a text box.
class WindowKeyPoller : public CCNode {
    bool m_wasDown = false;

    static bool gdFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // cocos key codes are Windows virtual-key codes for normal keys.
    static bool bindDown() {
        auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
        for (Keybind const& b : binds) {
            int const vk = static_cast<int>(b.key);
            if (vk <= 0 || vk >= 0xFF) continue;
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }

public:
    static WindowKeyPoller* create() {
        auto* p = new WindowKeyPoller();
        if (p->init()) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        scheduleUpdate();
        return true;
    }
    void update(float) override {
        bool const down = gdFocused() && bindDown();
        if (down && !m_wasDown && !CCIMEDispatcher::sharedDispatcher()->hasDelegate()) BufferWindow::toggle();
        m_wasDown = down;
    }
};
#elif defined(GEODE_IS_MACOS)
// Window Key (macOS): from GD's key handling, in every scene. (Android has no
// keyboard: the pause menu's Input Buffer button shows / hides the window.)
class $modify(BufKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double time) {
        if (down && !repeat && !CCIMEDispatcher::sharedDispatcher()->hasDelegate() && BufferWindow::isWindowKey(key)) {
            BufferWindow::toggle();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, time);
    }
};
#endif

#ifdef GEODE_IS_DESKTOP
// Mouse wheel: the window first when the cursor is over it (1.25.9). Desktop
// only - phones have no mouse (the list scrolls by dragging there).
class $modify(BufMouse, CCMouseDispatcher) {
    bool dispatchScrollMSG(float y, float x) {
        if (BufferWindow::takeScroll(y)) return true;
        return CCMouseDispatcher::dispatchScrollMSG(y, x);
    }
};
#endif

// Pause menu: an "Input Buffer" button that shows / hides the window (1.26.2).
// Every platform - and the only way on Android, which has no keyboard.
class $modify(BufPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto* spr = ButtonSprite::create("Buffer", "bigFont.fnt", "GJ_button_04.png", 0.6f);
        spr->setScale(0.55f);
        auto* btn = CCMenuItemExt::createSpriteExtra(spr, [](CCMenuItemSpriteExtra*) { BufferWindow::toggle(); });
        btn->setID("input-buffer-button"_spr);
        if (auto* menu = this->getChildByID("right-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            // no such menu (another mod moved things): our own, bottom-left
            auto* m = CCMenu::create();
            m->addChild(btn);
            auto const win = CCDirector::sharedDirector()->getWinSize();
            m->setPosition({45.f, 30.f});
            (void)win;
            this->addChild(m, 10);
        }
    }
};

// ----------------------------------------------------------------------------
//  8. Geode hooks
// ----------------------------------------------------------------------------
$on_mod(Loaded) {
    // Stage cycle: 1 -> 2 -> 3 -> 4 -> Extreme -> Off
    listenForKeybindSettingPresses("cycle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) s_sim.cyclePreset();
        return false; // don't swallow the key
    });
    // Window show / hide (default Left Shift): a key poller on Geode's overlay
    // (Windows) / the keyboard hook (elsewhere), so it works in every scene.
    // 1.12.3: the Window Key setting got a new id (old installs kept F10 from an
    // early version), and the window is switched back on once so it shows up.
    if (!Mod::get()->getSavedValue<bool>("migrated-1123", false)) {
        Mod::get()->setSavedValue<bool>("migrated-1123", true);
        Mod::get()->setSavedValue<bool>("window-shown", true);
    }
#ifdef GEODE_IS_WINDOWS
    queueInMainThread([] {
        if (auto* p = WindowKeyPoller::create()) OverlayManager::get()->addChild(p);
    });
#endif
    log::info("[InputBuffer] loaded");
}


// Show the window in the main menu too (if it's switched on).
class $modify(BufMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (Mod::get()->getSavedValue<bool>("window-shown", true)) {
            Loader::get()->queueInMainThread([] { windowEnsure(); });
        }
        return true;
    }
};

class $modify(BufPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayLayer::destroyPlayer", -1000000);
        (void)self.setHookPriority("PlayLayer::playEndAnimationToPos", -1000000);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        s_sim.init(this);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        s_sim.onReset();
    }

    void pauseGame(bool p0) {
        s_sim.onPause(this);
        PlayLayer::pauseGame(p0);
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (s_sim.checkSimulationDeath(player, gameObject)) return;
        bool const wasDead = player && player->m_isDead;
        PlayLayer::destroyPlayer(player, gameObject);
        // Only a real death counts: noclip-style mods and repeated calls while
        // already dead leave nothing to report.
        if (player && !wasDead && player->m_isDead &&
            (player == m_player1 || player == m_player2)) {
            s_sim.onRealDeath();
        }
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        PlayLayer::playEndAnimationToPos(p0);
    }

    void onQuit() {
        PlayLayer::onQuit();
        s_sim.quit();
    }
};

class $modify(BufHardStreak, HardStreak) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("HardStreak::addPoint", -1000000);
    }

    void addPoint(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        HardStreak::addPoint(p0);
    }
};

class $modify(BufEffectGameObject, EffectGameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EffectGameObject::triggerObject", -1000000);
    }

    void triggerObject(GJBaseGameLayer* p0, int p1, const gd::vector<int>* p2) {
        if (s_sim.isSimulating()) return;
        EffectGameObject::triggerObject(p0, p1, p2);
    }
};
// Orb side effects (effect circle, activation, spawn triggers) must not run
// when a sim player uses an orb during a test.
class $modify(BufRingObject, RingObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("RingObject::spawnCircle", -1000000);
        (void)self.setHookPriority("RingObject::triggerActivated", -1000000);
        (void)self.setHookPriority("RingObject::powerOnObject", -1000000);
    }
    void spawnCircle() {
        if (s_sim.isSimulating()) return;
        RingObject::spawnCircle();
    }
    void triggerActivated(float x) {
        if (s_sim.isSimulating()) return;
        RingObject::triggerActivated(x);
    }
    void powerOnObject(int state) {
        if (s_sim.isSimulating()) return;
        RingObject::powerOnObject(state);
    }
};

class $modify(BufGameObject, GameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GameObject::playShineEffect", -1000000);
    }

    void playShineEffect() {
        if (s_sim.isSimulating()) return;
        GameObject::playShineEffect();
    }
};

class $modify(BufPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayerObject::playSpiderDashEffect", -1000000);
        (void)self.setHookPriority("PlayerObject::incrementJumps", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnScaleCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnPortalCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::ringJump", -1000000);
        (void)self.setHookPriority("PlayerObject::bumpPlayer", -1000000);
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (s_sim.isSimulating()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (s_sim.isSimulating()) return;
        PlayerObject::incrementJumps();
    }

    // Circle effects a hidden sim player could draw (orb / portal / size): never
    // during a test (1.26.2 - a ghost circle showed up next to orbs).
    void spawnCircle() {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnCircle();
    }
    void spawnScaleCircle() {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnScaleCircle();
    }
    void spawnPortalCircle(cocos2d::ccColor3B color, float startRadius) {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnPortalCircle(color, startRadius);
    }

    void update(float dt) {
        if (s_sim.isSimulating()) {
            PlayerObject::update(dt);
            return;
        }
        PlayLayer* pl = PlayLayer::get();
        if (!pl) {
            PlayerObject::update(dt);
            return;
        }
        s_sim.onPlayerUpdate(pl, this);
        PlayerObject::update(dt);
        if (this == pl->m_player1) s_sim.setFrameDelta(pl, dt);
    }

    // Pads: during a test only sim players may be bumped; on a real hit, learn
    // the strength GD passes so the sim can apply the same pad later.
    void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object) {
        if (s_sim.isSimulating()) {
            if (!s_sim.isSimPlayer(this)) return;
        } else {
            PlayLayer* pl = PlayLayer::get();
            if (pl && (this == pl->m_player1 || this == pl->m_player2)) s_sim.learnPad(objectType, bumpMod);
        }
        PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    }

    void ringJump(RingObject* p0, bool p1) {
        // During a test only our sim players may use orbs (that's the orb prediction).
        if (s_sim.isSimulating() && !s_sim.isSimPlayer(this)) return;
        if (s_sim.isSimulating()) {
            PlayerObject::ringJump(p0, p1);
            return;
        }
        double const before = m_yVelocity;
        int const key = p0 ? TrajectoryBufferSim::orbKey(p0, this) : -1;
        bool const ud = m_isUpsideDown;
        PlayerObject::ringJump(p0, p1);
        s_sim.noteRealOrb(this, p0, p1, before, m_yVelocity, key, m_isUpsideDown != ud);
    }
};

class $modify(BufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::canBeActivatedByPlayer", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::collisionCheckObjects", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::playerTouchedRing", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::flipGravity", -1000000);
    }

    // Declared on GJBaseGameLayer (PlayLayer only inherits it).
    void flipGravity(PlayerObject* p0, bool p1, bool p2) {
        if (s_sim.isSimulating()) {
            // A sim player using a blue / green orb or gravity pad: flip only that
            // player, with no level effects. Nothing else may flip during a test.
            if (s_sim.isSimPlayer(p0)) p0->flipGravity(p1, true);
            return;
        }
        GJBaseGameLayer::flipGravity(p0, p1, p2);
    }

    // Once per rendered frame: feeds the frame-time meter used by the auto CPU budget.
    void update(float dt) {
        PlayLayer* pl = PlayLayer::get();
        bool const mine = pl && static_cast<GJBaseGameLayer*>(pl) == this;
        if (mine) s_sim.onFrame();
        GJBaseGameLayer::update(dt);
        if (mine) s_sim.watchdog(pl);
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_sim.isSimulating()) {
            // Orbs may be "touched" by sim players (exact orb contact); nothing
            // else may be activated during a test.
            if (s_sim.isSimPlayer(p0)) {
                if (auto* ring = typeinfo_cast<RingObject*>(p1)) {
                    if (GJBaseGameLayer::canBeActivatedByPlayer(p0, p1)) return true;
                    // GD's per-player "already activated" flags are set by an
                    // earlier test touching the orb and never put back, so only the
                    // FIRST test of a click could use an orb (1.25.0). The sim
                    // decides from what really happened instead: an orb you didn't
                    // use this attempt, and not the one this test already used.
                    return s_sim.simOrbAvailable(ring);
                }
            }
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && !s_sim.isSimulating() && !s_sim.isFiringBuffered()) {
            PlayLayer* pl = PlayLayer::get();
            if (pl && static_cast<GJBaseGameLayer*>(pl) == this) {
                if (!s_sim.interceptInput(pl, down, isPlayer1)) return;
                s_sim.noteInputForCheck();
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* vec, int objectsCount, float dt) {
        if (s_sim.isSimulating()) {
            auto& filtered = s_sim.filterForSim(player, vec, objectsCount);
            GJBaseGameLayer::collisionCheckObjects(player, &filtered, static_cast<int>(filtered.size()), dt);
            return;
        }
        if (PlayLayer* pl = PlayLayer::get(); pl && static_cast<GJBaseGameLayer*>(pl) == this) {
            s_sim.sampleMovers(pl, player, vec, objectsCount);
        }
        GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (s_sim.isSimulating()) {
            if (s_sim.isSimPlayer(player)) s_sim.simRingTouched(player, ring); // exact contact, nothing else
            return;
        }
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};
