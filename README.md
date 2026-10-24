# Nova V2.3 (Input Buffer)

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
| Stage 3 | 3 frames early (2 for ship) |
| Stage 4 | 5 frames early (4 for wave, 2 for ship) |
| Extreme | 20 frames early (12 for wave, 2 for ship) |
| Custom | your own frames (0.5 to 30), set in the window |

Extreme and Custom also re-plan held clicks live, every tick, so long holds stay safe. **Custom Min / Max** (Stage tab, Custom only): the smallest shift a click gets and the latest it can ever go in (240 ticks a second, 4 = 1 frame). **F6** cycles through the stages (you can change the key in the mod settings).

## The window

**Left Alt** shows / hides it (change it in the settings: *Window Key*). You can also use the **Nova button** in the pause menu (green circle with the target icon), which is the way on phones; the window is bigger there too. The tabs are in the sidebar on the left.

- **Log**: what the buffer did with each click: buffered, fired, raw, deaths. On Windows, **console** opens it in its own command-prompt window instead.
- **Stats** (hidden: click the version badge 5 times): prediction accuracy, your rhythm, windows, performance, and your system. The same 5 clicks show the **Dev** tab (live details to send when something's off).
- **Stage**: pick the stage, set Custom frames, and the options (Learn from attempts, Log: more detail, Custom Min / Max).
- **Modes**: switch the buffer on or off per gamemode.
- **Tools**: Orb helper (on by default; any orb), Level scan (finds the inputs for the whole level while paused, never stops before the end), Show scan (watch it work), Show rays (your hitbox, the press / release paths, a live pathfinder and what the buffer thought, as lines in the level), Export scan (the scan as a text file: where to press, what it couldn't solve), Forget scan (delete this level's scan), Snap to scan (your clicks use the scan, off by default), your saved stats, and Export report (one file with everything, to send when something's off).

## Versions

✅ = works on that platform, ❌ = not available there

| Version | Platform | Works |
|---------|----------|:-----:|
| **v2.3.0** | Windows | ✅ |
|  | macOS | ✅ |
|  | iOS | ✅ |
|  | Android | ✅ |
| **v2.2.0** | Windows | ✅ |
|  | macOS | ✅ |
|  | iOS | ✅ |
|  | Android | ✅ |
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

## What's new in V2.3

- **Nova:** the mod is called Nova now (id `alquerez.nova`); your Input Buffer data comes over on the first start.
- **GD techniques in the scan:** straight fly (ship taps), buffer clicks (ball / spider holds into landings), dual parts, mini wave, slopes and speed portals.
- **Nova look:** a neon NOVA sign with a galaxy O, a space background, glowing section outlines, a rebuilt Modes tab with your icons, a settings cog by the online count, and **Nova Run** - your icons through GD obstacles in all 8 gamemodes, with GD's real portals and trails. (And sometimes your robot jumps across the window.)
- **New scan method (sprint):** much faster, and a scan always finishes now.
- **Snap To Scan** finds the scanned line, in wave too.

## What's new in V2.2

- **New window:** tabs in a sidebar, dark glass with a purple hue, switches instead of checkboxes.
- **Animations:** the window grows in when it opens, shrinks away when it closes, and pages slide in.
- **Console log (Windows):** the Log in its own command-prompt window (Log tab -> console).
- **Ship, light touch:** ship clicks are held at most 2 frames and only for a crash right ahead, so a straight fly isn't pulled down.
- **Wave rays:** wave inputs are judged by casting your two straight lines (press / release) against the level, like a wave bot's view - far less work per click, so spam keeps up.
- **Spam fixed:** wave / ship spam no longer drifts you up.
- **Orb Helper** on by default, now for dash / spider / teleport / custom orbs too.
- **Show Rays:** press / release paths, a live pathfinder and what the buffer is thinking, drawn as lines.
- **Online users** at the top of the window (needs the server in `server/`).
- **Custom Min / Max:** the smallest and largest shift on the Custom stage; Custom saves the moment you press +/-.
- **Scan Level** no longer crashes big levels, and **Forget Scan** deletes a level's scan.

## What's new in V2.1

- **"V2.1"** badge next to the title, and "by AlQuerez23" / "Discord: alquerez23".
- The bottom of the window says how to toggle it ("Left Alt to toggle", or your own key) and what you're running on (Windows 10 / 11, macOS, iOS, Android).
- New **System** section in Stats: OS version, device, Geometry Dash, Geode and mod versions.
- Phones: a GD-style green pause-menu button, and a 1.5x bigger window.
- Fixed the crash on iOS / Apple Silicon Mac when touching an orb.
- Wave / ship: your second input is checked (on time vs. keeping spacing), holds are capped by your own rhythm, ship releases on Extreme capped at 5 frames.
- New Tools tab: Orb helper, Level scan + Show scan + Real game + Snap to scan, saved stats, Export report.
- New purple logo.
- Wave overhaul: much lighter predictions (shorter lookahead, held releases follow their plan), Extreme releases held at most 5 frames (was 12), and releases near orbs / portals no longer come out late.

Every version's full changes are in [changelog.md](changelog.md).
