# Input Buffer V2.1

**An input buffer for Geometry Dash** (Geode mod) by **AlQuerez23**. Discord: **alquerez23**

Clicked a hair too early? Instead of dying, the click waits for the first moment it's safe and goes through then.

## How it works

1. **Every click is checked before the game uses it.** The mod runs your click on a hidden copy of your player and plays the next moment forward: your jump, gravity, orbs, pads, portals, moving objects.
2. **Safe click?** It goes through right away, untouched.
3. **Deadly click?** It's held, and the mod finds the **earliest tick where the same click survives**. It fires there, as long as that's inside your stage's window.
4. **Can't be saved?** It goes through as you made it.

A few rules it always keeps:

- Clicks only ever move **later**, never earlier. Nothing is ever pressed for you.
- Every input is kept: double taps, releases and hold lengths survive.
- Works with or without **Click Between Frames**, at any TPS (60, 240, 480+).
- Held clicks follow their plan: the path is predicted once when you click, then only checked each tick, so it stays light on CPU and keeps input lag low.
- Mini cube **learns from your attempts**: spots where a click killed you or worked are remembered per level.
- Platformer levels are left alone.

## Stages

One setting, **Stage**, picks how early a click can be and still get saved. Frames are 60fps frames (1 frame = 16.7 ms), the same at any TPS.

| Stage | Saves clicks up to |
|-------|--------------------|
| Off | nothing (raw input) |
| Stage 1 | 1 frame early |
| Stage 2 | 2 frames early |
| Stage 3 | 4 frames early (3 for ship / wave) |
| Stage 4 | 8 frames early (5 for ship / wave) |
| Extreme | 20 frames early (12 for ship / wave) |
| Custom | your own frames (0.5 to 30), set in the window |

Extreme and Custom also re-plan held clicks live, every tick, so long holds stay safe. **F6** cycles through the stages (you can change the key in the mod settings).

## The window

**Left Alt** shows / hides it (change it in the settings: *Window Key*). You can also use the **Input Buffer button** in the pause menu (green circle with the target icon), which is the way on phones; the window is bigger there too. While a level runs it shrinks to a small pill ("Small while playing").

- **Log**: what the buffer did with each click: buffered, fired, raw, deaths.
- **Stats**: prediction accuracy, your rhythm, windows, performance, and your system.
- **Stage**: pick the stage, set Custom frames, and the options (Show paths, Learn from attempts, Log: more detail, Small while playing).
- **Modes**: switch the buffer on or off per gamemode.
- **Tools**: Orb helper, Level scan (finds the inputs for the whole level while paused, never stops before the end), Show scan (watch it work), Real game (practice, experimental: the scan uses the real level where its own prediction can't get through), Click guide (dots for those inputs), Snap to scan (your clicks use the scan, off by default), your saved stats, and Export report (one file with everything, to send when something's off).

## Versions

✅ = works on that platform, ❌ = not available there

| Version | Platform | Works |
|---------|----------|:-----:|
| **v2.1.0** | Windows | ✅ |
|  | macOS | ✅ |
|  | iOS | ✅ |
|  | Android | ✅ |
| **v2.0.0** | Windows | ✅ |
|  | macOS | ✅ |
|  | iOS | ✅ |
|  | Android | ✅ |
| **v1.26.2** | Windows | ✅ |
|  | macOS | ❌ |
|  | iOS | ❌ |
|  | Android | ✅ |
| **v1.7.0 - v1.26.1** | Windows | ✅ |
|  | macOS | ❌ |
|  | iOS | ❌ |
|  | Android | ❌ |

## What's new in V2.1

- **"V2.1"** badge next to the title, and "by AlQuerez23" / "Discord: alquerez23".
- The bottom of the window says how to toggle it ("Left Alt to toggle", or your own key) and what you're running on (Windows 10 / 11, macOS, iOS, Android).
- New **System** section in Stats: OS version, device, Geometry Dash, Geode and mod versions.
- Phones: a GD-style green pause-menu button, and a 1.5x bigger window.
- Fixed the crash on iOS / Apple Silicon Mac when touching an orb.
- Wave / ship: your second input is checked (on time vs. keeping spacing), holds are capped by your own rhythm, ship releases on Extreme capped at 5 frames.
- New Tools tab: Orb helper, Level scan + Show scan + Real game + Click guide + Snap to scan, saved stats, Export report.
- New purple logo.
- Wave overhaul: much lighter predictions (shorter lookahead, held releases follow their plan), Extreme releases held at most 5 frames (was 12), and releases near orbs / portals no longer come out late.

Every version's full changes are in [changelog.md](changelog.md).
