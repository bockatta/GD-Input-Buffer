# Changelog

## v2.0.0
- Input Buffer V2: everything since v1.17 in one release - exact predictions (one-tick check 100%), learned orb speeds, size portals predicted, held clicks that follow their plan, much lighter per frame, the new purple window with the in-game pill, the new Log and Stats, Android, iOS and macOS support. Details per step below.
- Build fixes for iOS: the keyboard hook (macOS only) is no longer compiled on phones, and the GitHub build works around a missing include in Geode's current iOS bindings.

## v1.26.2
- Fixed the "random orb": a circle could show up next to an orb. It was the orb effect being drawn by the mod's hidden prediction player. Predictions no longer trigger it, and the jump itself comes from the learned orb speeds as before.
- Android support: the mod is marked for Android, and there's a new "Buffer" button in the pause menu that shows / hides the window. That's the way to open it on a phone, since there's no keyboard. Mouse-wheel scrolling is PC-only; on Android you scroll the Log and Stats by dragging.
- A GitHub Actions build (.github/workflows/multi-platform.yml) builds Windows and Android together into one .geode.

## v1.26.1
- Held clicks follow their plan: when you click, the mod predicts your path once, and while the click is held it just checks you're still on that path, with no new prediction every tick. It fires on the same tick as before. If something unexpected moves you off the path, it goes back to checking every tick. Much less CPU while a click is held. (Mini cube keeps its full per-tick checks.)
- "Small while playing" (Stage tab, on by default): while a level is running you get a small pill with the stage and the latest Log line. The full window comes back when you pause or go to the menus.
- New Log look: time, a coloured tag (BUFFER, FIRED, DEATH, RAW, QUEUED, OK, NOTE) and a short description.
- Log lines are only built when they'll be shown.
- The window pops in when it opens, the tab highlight slides between tabs, and the window reopens on the last tab you used.
- New Window Opacity setting (in All settings) for the window's background. Text stays solid.
- "forget level" and "all settings" moved next to the OPTIONS title.
- A "V2" badge next to the title.

## v1.26.0
- Less work every frame, for less input lag:
  - Turned off moving-object tracking. It recorded every nearby spike and orb 240 times a second and never actually predicted anything.
  - The one-tick check and accuracy tracking (diagnostics) only run with "Log: show safe clicks" on.
  - The window only redraws text that changed, instead of rebuilding everything 4 times a second while you play.
- Cleaner log: the small "live: said no ..." detail after a fired click only shows with "Log: show safe clicks" on.

## v1.25.9
- Removed everything that held or moved your clicks for orbs: orb aim, the early-click orb snap, and aiming through portals. Also removed the gamemode-portal hold. All of these delayed clicks and made the game feel off. Orbs are still predicted correctly; nothing holds a click for one anymore.
- Wave: an input in a burst of spam goes in on time whenever that's safe. Before, one delayed input kept the same delay on every input after it until you paused, which felt like input lag.
- Learn from attempts is fully mini cube only: spots that older versions saved for UFO and other modes are deleted when a level loads.
- Stats: the first section is titled with the level's name, and every section title is bold and white. The Windows section shows what clicks really get. Extreme is 20 frames (12 for ship / wave); Stats was wrongly showing the base 8f and 5f, but Extreme itself was working.
- Scrolling with the mouse wheel over the window now scrolls only the window, not the level list behind it.

## v1.25.8
- Buttons and boxes are actually rounded in the game now. They're drawn as rounded shapes instead of stretched textures, which came out square on small buttons.
- "EXTREME" in the top-right, the stage name and the close X are white. The X is now a box like the other buttons.
- Stats cleaned up: short sections (This level, Prediction, Orbs, Your rhythm, Windows, Performance) with the label on the left and the number lined up on the right. The debugging numbers are under DETAILS, which only shows with "Log: show safe clicks" on.

## v1.25.7
- New colors: the window fades from purple at the top into midnight blue at the bottom, with a violet-to-pink line under the title, violet highlights for the selected tab and stage, and lavender section titles.
- The title is white instead of yellow.
- Every tab and button is now a rounded box, and the whole box is clickable. Before, tabs only reacted when you clicked the word itself. All buttons share this one style.

## v1.25.6
- New window look: the tabs are one even bar, the Stage tab is split into three panels (Stage, Custom frames, Options), the Modes tab has a panel per column, and Log / Stats have a real scrollbar. "clear" moved to the bottom of the window, and an empty Log explains what will show up there.
- "by AlQuerez" next to the title, and "Discord: AlQuerez#3900" plus your window key (Left Alt by default) on the Stage tab.
- Removed the Changes tab. This changelog is still kept up to date.
- Removed the "Early UFO clicks go to the next orb" toggle. That behavior is now always on.
- Shorter orb aim line in the Log: "PRESS aimed at the pink orb: +6t (good on +5..+7t)".

## v1.25.5
- Orb aim reworked after going through it properly:
  - It tests the whole time you overlap the orb, not just its first 10 ticks. At normal speed you're inside an orb for about 40-50 ticks, and the tick that clears the spikes is usually late in that. Your on-time clicks kept landing on the orb's last usable tick, so early clicks were only ever tested on ticks that hit the spikes.
  - In a chain of orbs, a tick counts as good if it gets you to the next orb with a few ticks to spare, because that orb is your next click's job. Before, the right tick still looked deadly, so nothing got aimed.
  - It only aims at a tick that actually gets you through. Before, it could aim at the "least bad" tick and override a better plan.
  - It fires in the middle of the longest run of good ticks, so being one tick off still works.
- The tick picking is unit tested: it finds a 1-tick window anywhere in a 50-tick orb, including at the orb's very edge. It's capped at 2.5ms of CPU per click.
- With "Log: show safe clicks" on, the orb aim line shows how many ticks were tested and what each run of ticks did.

## v1.25.4
- Orb aim uses much less CPU. In UFO it was running on every click and dropped the frame rate (124fps in the last log).
- "PRESS too late for the orb" now only shows when nothing later would keep you alive either.

## v1.25.3
- New toggle on the Stage tab: "Early UFO clicks go to the next orb" (on by default). A UFO or swing click made well before an orb now gets buffered onto that orb, like any other buffered click, as long as you'd reach the orb without pressing, the orb is within your window, and using it keeps you alive. Before, only a click that would kill you right away got aimed, so an early click far from the orb just flapped.
- If you need a flap to reach the orb, the click stays a flap. Turn the toggle off if it ever moves a flap you meant onto an orb.

## v1.25.2
- Cleaner log. The orb check and orb jump lines only show up when the orb prediction is off by more than 2px. "REL raw (no effect here)" and "orb aim: no orb ahead" are gone. Orb aim tests are grouped, e.g. "+0 orb, dies 11t | +1..+10 flap, dies 7..1t".
- A click on an orb's last usable tick that still kills you used to say "PRESS ok (safe 11t)". It now says "PRESS too late for the green orb: using it now dies in 11t, later is a flap - click a bit earlier".

## v1.25.1
- Size portals no longer stop the prediction. In Amethyst's UFO part the orbs sit right next to mini and normal size portals, so every orb click there went in raw ("size portal (mini) 6px ahead"), and none of the orb fixes ever ran. The prediction now switches your size when it passes a size portal. It does this on its own hidden player only and never touches the real portal, which is what broke portals back in v1.20.0.
- An orb that hasn't been learned at a given size or gravity yet uses the same orb's learned jump from the other size or gravity.
- Stats shows how many size portals predictions have gone through.

## v1.25.0
- Found the orb bug that's been there since v1.23.0: only the first prediction of each click could use an orb. When a prediction touched an orb, the game marked it as already used and the mod never cleared that, so every prediction after it (the search for the right tick, orb aim, the live re-checks) was told the orb was gone. A click on the orb worked, because that's the first test, but an early click never found the orb tick and said "no safe moment found" or "too late".
- Predictions now use any orb you haven't really used this attempt, no matter what the game's flags say, and orbs are always put back exactly as they were.
- With "Log: show safe clicks" on, orb aim lists what each tick it tested did, e.g. "+3:orb dies 14t  +4:orb ok".

## v1.24.9
- Orb aim kept saying "no orb ahead" even with your press on the orb. The game marks an orb as activated as soon as you touch it, not when you use it, so the mod skipped the orb you were in. The mod now keeps its own list of the orbs you've really used this attempt.

## v1.24.8
- Orb aim never ran ("held for an orb: 0"). It needs to know when you'd reach the orb if you don't press, but the game only reports touching an orb while the button is held. Now it uses the hitboxes for that; the game still decides which press ticks actually use the orb.
- A press held through a size or gamemode portal now gets aimed at the orb after the portal, as soon as the mod can see past the portal. Before, it fired at the portal hold's limit straight into the spikes.
- With "Log: show safe clicks" on, the Log says why a click wasn't aimed (no orb ahead, or no press tick uses it).

## v1.24.7
- Early UFO / swing clicks for an orb finally get aimed: the mod tests every tick you'd be overlapping the orb and fires your click on the one whose orb jump keeps you alive longest. Before, the normal search could never pick an orb: it wanted safe ticks on both sides of its pick and skipped ticks far from the click, and a frame-perfect orb is only 1-2 ticks wide. The v1.24.4 hold ran out of CPU time every click, so it never triggered.
- Log: "PRESS aimed at the pink orb: +Nt (you overlap it on +A..+Bt)", then "PRESS fired +Nt (on the orb)".
- The orb press prediction follows GD's own order by default (flap step, then the orb). The old orb check results are reset, since they were made while the prediction's orb did nothing.

## v1.24.6
- Found why the first orb never buffered: the prediction's orb did nothing. Pink and green orbs both came out as a normal UFO flap (y-speed 6.87), while the game gave 4.46 (pink) and -10.62 with a gravity flip (green). So every "after using an orb" prediction was wrong.
- Orbs are now learned from the game: every orb you really use records the speed it gives and whether it flips gravity (per orb type, gamemode, size and gravity), and predictions use exactly that. It's saved, so each orb type only needs to be hit once. You'll see "learned: pink orb (ufo) gives y-speed ..." in the Log and the full list in Stats.
- Removed the y-speed correction from v1.24.5 (it was learned from the broken orb).

## v1.24.5
- Orb jumps are measured against the game: after a UFO orb click that went in on time, the Log shows "orb jump [pink orb]: game used it on tick +N -> y-speed A | predicted tick +M -> y-speed B". If the tick matches but the speed doesn't, the prediction learns a correction for that orb type and size and uses it from then on.
- The prediction uses orbs exactly the way GD calls them (same settings as the game's own call).

## v1.24.4
- Early UFO / swing click for an orb: if pressing now would kill you and an unused orb is coming (you reach it without pressing, inside your window, before anything kills you), the click is held and fired on the tick you're touching that orb, exactly like a perfect click. It no longer depends on predicting the orb jump, which was 6-18px off and made the first orb say "no fix". The log shows "PRESS held for the pink orb", then "PRESS fired +Nt (on the orb)".
- Stats > ONE-TICK CHECK shows how many early UFO clicks were held for an orb and fired on it.

## v1.24.3
- UFO / swing orb presses: the prediction now tries three ways GD might handle a press on an orb (orb first with no flap, flap then orb, both together). Every time you click a UFO orb and it goes in on time, all three are compared with where you really end up 10 ticks later, and the closest one is used from then on. It's saved between sessions. You'll see "orb check" lines in the Log, and the result under Stats > ONE-TICK CHECK.
- A press only uses an orb you're really overlapping that hasn't been used yet.
- Death lines say what you hit (spike, saw, block, slope) instead of "object".

## v1.24.2
- Fixed presses near portals being super delayed (+17t, +35t). Only a press at most 4 ticks before a gamemode / size portal is held now (it was anything within 40px, about 30 ticks), and it fires at most 6 ticks after you reach the portal: on the first safe tick, or at that limit. Clicks queued behind it no longer get shifted by a huge amount.

## v1.24.1
- UFO / swing: pressing while touching an orb now uses the orb on that same tick in the prediction, with no flap first (like the game). This was making the orb click look deadly at every timing.
- Learn from attempts is mini cube only again (in UFO it was firing presses far too late at old spots).

## v1.24.0
- A press right before a gamemode or size portal is no longer passed through raw: it's held until you're through the portal, then fired on the first safe tick (the prediction can see again there). Not in ship / wave.

## v1.23.3
- With Click Between Frames, buffered clicks were firing up to a whole tick later than planned (a click lands mid-tick, held clicks fire on tick boundaries). Predictions now start from the exact point inside the tick and held clicks fire on the exact tick that was tested. Matters most for frame-perfect clicks.

## v1.23.2
- Predictions copy all of your player's jump / fall / slope / timer state, not just a few values. The leftover "press waiting" state was still making UFO predictions flap on their first tick.

## v1.23.1
- Fixed predictions starting with an extra UFO flap whenever you were holding (the cause of normal UFO being a few pixels off).
- Learn from attempts: a click is held for a learned spot only a few ticks ahead, and only if waiting is safe (long holds made the UFO fall and press below the orb).

## v1.23.0
- Exact orb contact: the prediction asks the game's own collision when you touch an orb (its real hitbox) instead of estimating it - for frame-perfect orb clicks.
- One-tick self-check in Stats: how often the prediction matches the game exactly after one tick, per gamemode, and by how much it differs. Diagnostics only.

## v1.22.2
- Removed the "no safe moment found" hold: clicks the buffer can't improve go straight through right away, like before v1.19.0.
- Those lines only show in the Log with "Log: show safe clicks" on.

## v1.22.1
- Moving objects that speed up / slow down (eased move triggers) are now predicted - before, only steady movement was, so most moving spikes were treated as standing still. They're also trusted faster.

## v1.22.0
- Moving spikes and orbs are predicted where they will be (not where they are now) in every gamemode except ship and wave. Windows that open when a spike moves out of the way can be buffered now.

## v1.21.1
- UFO gets full learn from attempts: held UFO clicks fire on spots that worked before, and clicks on spots that killed you before are held and re-planned.

## v1.21.0
- Learn from attempts now also works for UFO.
- An early click the prediction can't line up (a portal or orb ahead, or no safe moment found) is held for the spot where a press worked on an earlier attempt, even through a gamemode portal, and fires when you reach it.

## v1.20.2
- Diagnostics: Stats > PREDICTION ACCURACY shows which way predictions are off per gamemode (sideways vs up/down), and so does each "prediction off" log line.

## v1.20.1
- Reverted v1.20.0: switching gamemode inside the prediction broke real portals. Portals work exactly like v1.19.2 again.

## v1.20.0
- Gamemode and size portals no longer stop the prediction: it switches through them, so clicks just before a portal (like an orb right after a UFO portal) can be buffered.

## v1.19.2
- UFO / swing: clicks the buffer can't fix go through as made again, instead of being held (the prediction was wrong in some UFO parts and held clicks never fired).

## v1.19.1
- UFO / swing: holding the button no longer counts as clicking orbs in the prediction (in the game, only a fresh click uses an orb there). This made held UFO clicks never fire.
- A held click that's checked every tick now fires just before doing nothing would kill you, instead of waiting.

## v1.19.0
- Fixed early clicks being called "too late": the quick search stepped over narrow safe moments far from the click. Now (cube, UFO, robot, ball, spider, swing) such a click is held and checked every tick for the whole window, and fires on the first safe tick.

## v1.18.8
- Fixed every orb click saying "too late, no delay helps" (a v1.18.6 change made the prediction ignore the orb you were clicking).

## v1.18.7
- "too late" and "no fix" log lines say what the predicted death hits and whether the prediction used an orb.

## v1.18.6
- Fixed: every click on an orb went raw. After the prediction used the orb, the same orb stopped it on the next tick. Used orbs are now ignored for the rest of the prediction.

## v1.18.5
- Clicks on an orb you're touching are predicted through the orb instead of going raw ("raw: pink orb (touching)").

## v1.18.4
- The Log now says when inputs pass through because another mod stops the buffer's physics hook (it used to stay silent).
- Stats > CLICKS shows how many inputs reached the buffer, so "nothing in the Log" can be told apart from "the buffer never sees my clicks".

## v1.18.3
- A deadly click that runs out of time is finished with quick checks instead of going through raw (what made the old Predictive Buffer keep working on Grief / Slaughterhouse). Stats counts these clicks.

## v1.18.2
- Heavy levels (Grief, Slaughterhouse): far fewer "raw: CPU budget" clicks. The first check of a click always gets enough time, the search gets its own budget, cube checks stop right after landing, and wave / ship searches do less extra work.
- Stats > PERFORMANCE shows the cost per predicted step and whether the level is light / medium / heavy.

## v1.18.1
- Fixed presses being swallowed after pausing while holding (the release in the pause menu never reached the mod).
- The mod never presses on its own to "fix" the button state any more.
- Safety nets: if the game's physics hook isn't running (another mod, a hitch), inputs go straight through; a held input is always released within 600 ms.
- CPU budget checked more often, so object-heavy levels (Grief, Slaughterhouse) can't overrun it by much.

## v1.18.0
- Less chop: held clicks are re-checked once per tick (not on every Click Between Frames sub-step) with a smaller time budget.
- Removed a wasted extra prediction per buffered click; the "how early was it" log check is much cheaper.
- Moving-hazard tracking only runs in mini cube.
- Stats > PERFORMANCE shows re-check cost, frame time and the mod's worst cost per frame.

## v1.17.1
- Wave "randomly goes up": the hidden prediction player now copies your ground / portal / landing / sliding state, so good releases aren't held back.

## v1.17.0
- Changes tab in the window, and this changelog on the mod's page.

## v1.16.2
- Wave wobble: spam taps are judged against your own hold / gap rhythm again, and a burst of spam keeps one fixed delay until you pause.

## v1.16.1
- "forget level" is always in the Stage tab (outside a level it tells you to open one).

## v1.16.0
- Wave spam: taps in one burst share the same delay instead of each getting a different one.

## v1.15.1
- A release made while a press is held fires exactly on time after it (keeps your hold length).

## v1.15.0
- Two engines: mini cube keeps the new one; every other gamemode is back to the v1.10.2 behaviour.
- Every mode: a held click fires on its planned tick at the latest.

## v1.14.3
- Wave / ship: clicks that run out of CPU time go through raw again; held clicks with no plan give up after 1 frame.

## v1.14.2
- Wave / ship judged against your own rhythm (fewer normal taps treated as deadly).

## v1.14.1
- Learn from attempts and near-miss waits: presses only. Queued releases held at most 2 frames.

## v1.14.0
- Modes tab: buffer on / off per gamemode, normal and mini separately.

## v1.13.2
- Held clicks fire exactly on their plan (was 2 ticks late). Moving-hazard speeds must be proven before they're used.
- New window look.

## v1.13.1
- Learned spots are saved per level between sessions. "forget level" button.

## v1.13.0
- Fixed held clicks that never fired ("held click hadn't fired yet"): they now fire on their plan.

## v1.12.3
- Window Key is Left Alt (new setting, old saved keys don't stick). Window turns back on once.

## v1.12.2
- Window Key read straight from the keyboard (works with Click Between Frames).

## v1.12.1
- Out of CPU on a deadly click: re-planned live instead of raw. Log says when / why each held click fired.
- Window works in menus too.

## v1.12.0
- Learn from attempts: remembers where inputs worked or killed you on a level.

## v1.11.0
- Near-miss check, moving-hazard prediction, Custom stage with Apply.

## v1.10.2
- Cube live check uses your real hold length. Log shows the gamemode and "too late" clicks.

## v1.10.0
- Wider windows for every stage. Live path preview with hitboxes.

## v1.9.0
- Extreme: live re-planning for clicks and releases. Deaths counted once. Left Shift window, half size.

## v1.8.0
- One Stage setting (Off, 1-4, Extreme). Every jump key works. Dense check right after a click.

## v1.7.0
- Draggable window (Log / Stats / Stage). Timing coach removed.
