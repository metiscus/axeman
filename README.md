# Axeman - Windows OOM Killer (Tray Application)

Axeman is a background utility that monitors system memory and prevents freeze-ups by terminating the largest memory consumer when usage hits a critical threshold.

## Features
*   **Silent Watchdog:** Runs in the system tray.
*   **Automatic OOM Prevention:** Monitors memory load and kills the largest process before the system becomes unresponsive.
*   **Configurable:** Adjust polling interval via `axeman.ini` and trigger threshold via UI or INI.
*   **Memory Threshold:** Fine-tune the trigger point between **90% and 99%** via the right-click menu.
*   **Launch at Startup:** Easy toggle to ensure Axeman is always protecting your system.
*   **Single Instance Protection:** Prevents multiple copies from running simultaneously.
*   **Safety First:** Hardcoded allowlist protects critical system processes (System, Explorer, etc.).
*   **Notifications:** Pops up a toast notification when a process is killed.
*   **Logging:** Records all termination events to `axeman.log` in the executable directory.

## Controls
*   **Right-click Tray Icon:**
    *   **Disable/Enable Axeman:** Pauses or resumes monitoring.
    *   **Memory Threshold:** Select a threshold from 90% to 99%.
    *   **Launch at Startup:** Toggle whether the app runs when Windows starts.
    *   **Exit:** Quits the application.
*   **Hover Tray Icon:** Shows current status (Watching/Disabled).

## Configuration (`axeman.ini`)
The application looks for `axeman.ini` in the same directory as the executable.

```ini
[Settings]
IntervalMS=500        ; Check every 500ms
ThresholdPercent=90   ; Trigger threshold (updated automatically by UI)
```

## Build Instructions
Requirements: Visual Studio and CMake.

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Usage
Run `axeman.exe` (Run as Administrator recommended for full termination powers).
The app will start minimized to the tray.

## Attribution
<a href="https://www.flaticon.com/free-icons/axe" title="axe icons">Axe icons created by ultimatearm - Flaticon</a>

## Privacy Policy
This program will not transfer any information to other networked systems.
