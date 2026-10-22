# Input Buffer V2.1

**By AlQuerez23.** Discord: **alquerez23**

Clicked a hair too early? Instead of dying, the click waits for the first moment it's safe and goes through then.

## How it works

- Every click is checked first: the mod plays it forward on a hidden copy of your player (jumps, orbs, pads, portals, moving objects).
- **Safe click:** goes through right away, untouched.
- **Deadly click:** held, then fired on the earliest tick where it survives, as long as that's inside your stage's window.
- **Can't be saved:** goes through as you made it.
- Clicks only move later, never earlier, and nothing is pressed for you. Double taps, releases and hold lengths are kept.
- Works with or without Click Between Frames, at any TPS. Platformer is left alone.
- Mini cube learns from your attempts on each level.

## Stages

How early a click can be and still get saved (60fps frames, 1 frame = 16.7 ms):

- **Off:** nothing (raw input)
- **Stage 1:** 1 frame early
- **Stage 2:** 2 frames early
- **Stage 3:** 4 frames early (3 for ship / wave)
- **Stage 4:** 8 frames early (5 for ship / wave)
- **Extreme:** 20 frames early (12 for ship / wave), with live re-planning
- **Custom:** your own frames (0.5 to 30), set in the window

**F6** cycles the stages.

## The window

**Left Alt** shows / hides it (or the key you set as Window Key). Or the **Input Buffer button** in the pause menu (green circle with the target icon), the way to open it on phones.

- **Log:** what happened to each click
- **Stats:** accuracy, your rhythm, windows, performance, your system
- **Stage:** stage, Custom frames, options
- **Modes:** buffer on / off per gamemode
- **Tools:** Orb helper, Level scan, Show scan, Real game (practice), Click guide, Snap to scan, your saved stats, Export report

## Platforms

**V2.1 works on:** Windows, macOS (Apple Silicon and Intel), iOS and Android (32 and 64-bit).

The full version list is in the Changelog tab.
