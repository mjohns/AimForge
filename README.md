## AimForge

AimForge is an open source aim trainer focused on simplicity, self improvement, and targeting weaknesses. Major goals of the project are:
* Cater to power users who tend to "main" aim trainers and spend most of their time doing benchmarks.
* Scenarios are expressable as a simple json file with knobs for the specific scenario tyep that are intuitive to tweak.
* Creating and editing scenarios should be simple enough that any user can confidently do it. Just press 'u' and you should intuitively be able to tweak the behavior of the current scenario.
* Enable easily playing scenarios at varying difficulties and sensitivities and while comparing scores and progress across those variations. (Automaticlly defined level veriations and fixed sensitivity suffixes).
* Provide a small focused app optimized for graphical / input performance. Graphical options only exist which provide visual clarity. No effects or gun models. No humanoid targets etc.

# Overview
Scenarios and playlists are distributed in "bundles" which are namespaced collections. So the AF (AimForge) bundle contains scenarios and playlists that all start with AF, like "AF Clicking", "AF Reflex Click", etc.
This allows sharing these bundles without naming collisions.

# Technical Overview
AimForge is a c++ app built on top of SDL3 (and the GPU api) and ImGui. Stats and settings are tracked using sqlite3 and scenarios and other config files are represented using json serialized protobufs. The app uses a simple custom "engine" that can efficiently render scenarios. It is currently fairly well optimized and can run at 5000 fps or capped at 1200 fps with 500k state updates per second (event polling, hit detection, etc). The code is also simple and focused enough that further optimizations should be straightforward to implement (compared to using something like Unreal). Effort was also put into making sure the worst frame is still good and can be easily viewed after a run in the perf tab. Typically the worst frame has a latency which projects 1300 fps.

# Building
The project is built with CMake. You can open the folder in Visual Studio (not code) on Windows.
On Unix like systems you can run:
```bash
cmake .
make
```