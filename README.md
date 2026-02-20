## FspAimForge

FpsAimForge is an open source aim trainer focused on simplicity, focus, and self improvement.

* [All releases](https://github.com/mjohns/FpsAimForge/releases)
* [Download latest](https://github.com/mjohns/FpsAimForge/releases/download/v0.3.5/FpsAimForge-v0.3.5.zip)

Major goals of the project are:
* Creating and editing scenarios is actually simple.
* Easy to play scenarios at varying difficulty levels and senistivities while tracking/comparing progress. (progressive overload)
* Amazing performance due to custom "engine" and local first (offline) design.
* Small app download while still including scenarios. You can download the full exe and a large selection of scenarios in just a 5mb download. No additional runtime downloads required.

See [videos](https://www.youtube.com/@FpsAimForge) of FpsAimForge in action!

Prebuilt binaries can be found under the [releases](https://github.com/mjohns/FpsAimForge/releases) tab.

# Features
* Settings like sensitivity, theme, and crosshair are saved per scenario (optional)
* Quickly change settings and sensitivity. Hold "s" to bring up the quick settings screen and release "s" to save. Use the scroll wheel while holding "s" to change senstivity. [video](https://www.youtube.com/watch?v=PHVEZ-ijGzM)
* Adjust crosshair size by holding "c" and using the scroll wheel.
* View replays of your last run from the stats screen.
* Simplified and consistent scoring. Tracking scenarios are score directly on hit percent which is calculated at the microsecond level and with granularity based on update/second.

# Interesting scenario types
* Proximity tracking. Tracking variation where score is based on how close to the center you are.
* Option to remove closest target on miss.
* Switching scenarios where the target radius shrinks as health is taken away.
* Switching scenarios not as focused on health timing allowing you to respond to the kill sound to move to the next target. (Sound played when x% health left, target removed once you move off after sound, You get score based on percent of health taken from target).

# Overview
Scenarios and playlists are distributed in "bundles" which are namespaced collections. So the AF bundle contains scenarios and playlists that all start with AF, like "AF Clicking", "AF Reflex Click", etc.
This allows sharing these bundles without naming collisions. The files are placed within the bundles folder in the user folder which can be opened from the settings page.

# Technical Overview
FpsAimForge is a c++ app built on top of SDL3 (and the GPU api) and ImGui. Stats and settings are tracked using sqlite3 and scenarios and other config files are represented using json serialized protobufs. The app uses a simple custom "engine" that can efficiently render scenarios. It is currently fairly well optimized and can run at 5000 fps or capped at 1200 fps with 500k state updates per second (event polling, hit detection, etc). The code is also simple and focused enough that further optimizations should be straightforward to implement (compared to using something like Unreal). Effort was also put into making sure the worst frame is still good and can be easily viewed after a run in the perf tab. Typically the worst frame has a latency which projects 1300 fps.

# Building
The project is built with CMake. You can open the folder in Visual Studio (not code) on Windows and build from there.
On Unix like systems you can run:
```bash
cmake -S . -B build && cmake --build build
cd build/bin
./FpsAimForge
```
