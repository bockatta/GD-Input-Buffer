# In-game checklist (v1.9.0)

Open the window (Left Shift). After each test, check the Log and Stats tabs.
Do each test with the old build and the new one on the same level for a fair comparison.

| # | Test | What to look for |
|---|------|------------------|
| 1 | Cube: tap 1–3 frames early into a spike gap | `PRESS buffered +Nt`, survives; N small |
| 2 | Cube: double tap fast over two spikes | Both jumps happen, never only one |
| 3 | Ship: normal flying through a corridor | Few `buffered` lines; no `ok (savable...)` deaths |
| 4 | Ship/robot: hold for a set time after an early press | Height/jump same as your hold length |
| 5 | Wave: press 1 tick early into a tight corner | Gets buffered (not passed raw) |
| 6 | Wave: fast spam section (normal mode) | `raw: spam guard`, no lag |
| 7 | UFO/swing: rhythm taps | No `buffered` on taps that were fine before |
| 8 | Yellow/pink orb clicked slightly early | `buffered ... via orb`, orb used |
| 9 | Dash / teleport / spider orb | Still `raw: dash orb ...` etc. (not predicted) |
| 10 | Mini portal, then click | No `orb/portal` message from the portal behind you |
| 11 | Dual (1 player) | Both icons get every input, no desync |
| 12 | Extreme: click well early | `buffered` lines; saves near the safe edge, not mid-window |
| 13 | 60 / 144 / 170 / 240 fps, CBF on and off | Same results; `cpu` budget rarely hit |
| 14 | Orb visuals during play | No stray orb circles/effects from predictions |
| 15 | Die, respawn, practice checkpoints | Nothing fires after respawn |
| 16 | Play a minute normally, watch `pred %` | High (90%+); if low, note the mode |
| 17 | Hit a yellow pad once, then click early before the next one | Second time: no `raw: yellow pad`, prediction continues |
| 18 | Speed portal / gravity portal ahead of a click | No cut-off message; `pred %` stays high after them |
| 19 | Blue / green orb clicked slightly early | `buffered ... via orb`, gravity flips correctly |
| 20 | Click far too early | Death line says how many ticks too early |
| 25 | Orb/pad visuals and pad/orb activation for you | Predictions never use up an orb or pad for the real player |
| 26 | Pause, drag the window by its title, change tabs, scroll | Moves, stays on screen, remembers position next level |
| 35 | Die in practice mode, respawn | One DEATH line, buffer keeps working |
| 36 | Extreme: click well early, watch the Log | `fired +Nt live (planned +Mt)` lines; saves land |
| 37 | Extreme ship/wave: hold, release early into a wall | Release held a few ticks, then fires; survives |
| 27 | Play with the window over where you click | Every click still jumps (window ignores clicks while playing) |
| 28 | F6 repeatedly | 1 > 2 > 3 > 4 > Extreme > Off > 1, every time; Stage tab matches |
| 29 | All settings button, close it, press Left Shift | Window comes back |
| 30 | Cube: tap normally for a minute | Stats: mini/cube accuracy high, few or no `buffered` on clean jumps |
| 31 | Ship / wave corridors, normal play | Few `buffered` lines; Stats shows your hold/gap rhythm |
| 32 | Play with keyboard (space, up, W) | Same `buffered` behavior as mouse |
| 33 | Hold space (key repeat) | No extra `buffered` / `queued` lines while held |
| 34 | Final Destination on Extreme | Fewer `raw: CPU budget` and `no fix` deaths than before |

Report any line where the window says one thing and the game did another.
