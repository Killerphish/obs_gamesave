# GameSave – OBS Livestream Recording Organizer

GameSave is an OBS Studio plugin that organizes your livestream recordings into a consistent folder structure: **Tournament Name / Day of Week / filename**. It works with OBS’s YouTube “Manage Broadcast” flow so you can name recordings from your broadcast title and have files land in the right place automatically.

## Features

- **YouTube broadcast selector** – A dropdown lists your pre-scheduled YouTube broadcasts (from the YouTube Data API). The first option is **“Schedule broadcast”** (default when none exist). Next to it: **“Load”** when a broadcast is selected, **“New”** when “Schedule broadcast” is selected. **Refresh** reloads the list; **Sign in with YouTube** starts OAuth so the plugin can list and manage broadcasts.
- **Load** – With a broadcast selected, **Load** opens a dialog: **Load** (apply that broadcast’s stream key and title to OBS and set Tournament Name), **Load and Start Stream** (same then start streaming), or **Cancel**.
- **Schedule broadcast (New)** – With “Schedule broadcast” selected, **New** opens a dialog similar to OBS YouTube stream setup: broadcast title, description, scheduled start time, plus GameSave **Tournament Name**. Submitting creates the broadcast via the YouTube API and refreshes the dropdown.
- **Tournament name from YouTube** – The **YT** button still reads the current YouTube broadcast title from OBS config (when you use Manage Broadcast with “Remember Settings”) and uses it as the tournament/folder name.
- **Set recording path (Approach A)** – “Apply” sets OBS’s recording path to `BasePath/TournamentName/%A/`, so new recordings go directly into the right weekday folder.
- **Move on stop (Approach B)** – With “Auto-organize on stop” enabled, when a recording ends the plugin moves the file into `BasePath/TournamentName/<Weekday>/<filename>`.

## Requirements

- OBS Studio 30+ (tested with 31.x)
- macOS 12.0+ (Universal: arm64 and x86_64)
- CMake 3.28+
- Xcode (for macOS build)
- Build uses OBS and dependency sources as defined in `buildspec.json` (downloaded and built by the preset)

## Build and install (one command)

From the project root:

```bash
./scripts/build-and-install.sh
```

This configures, builds, and installs the plugin into OBS’s plugin folder. **Restart OBS**, then open **Docks → GameSave**. The first run can take 15–30 minutes while it downloads and builds OBS and dependencies.

**Or step by step:**

```bash
./scripts/build.sh    # build only
./scripts/install.sh  # install into OBS (after building)
```

Install copies to `~/Library/Application Support/obs-studio/plugins/`. Override with `OBS_PLUGINS=/path ./scripts/install.sh` if needed.

## Install with GUI

You can build a **GameSave Installer** app that installs the plugin via a simple GUI. The installer will prompt you to close OBS if it is running, then copy the plugin into OBS’s plugin folder.

From the project root:

```bash
./scripts/build-installer.sh
```

This builds the plugin (if needed), builds the installer app, and embeds the plugin inside it. The result is `installer/build/Release/GameSave Installer.app`. Double‑click the app to run it: follow the prompts (quit OBS if asked), then restart OBS and open **Docks → GameSave**.

The installer app is built with Swift; if `xcodebuild` is unavailable (e.g. Xcode plugin issues), the script falls back to `swiftc` so no full Xcode GUI is required. If the plugin has not been built yet (e.g. `cmake` not in PATH), the app still builds and runs but will show “Plugin not found” until you run `./scripts/build.sh` and then `./scripts/build-installer.sh` again.

## Usage

1. **Open the GameSave dock** – Docks → GameSave (or enable it from the menu).
2. **YouTube broadcasts (optional)** – Click **Sign in with YouTube** to authorize the plugin (OAuth in your browser). The **Broadcast** dropdown then lists your upcoming scheduled broadcasts; **“Schedule broadcast”** is the default. Select a broadcast and click **Load** to apply its stream key and title to OBS (dialog: Load, Load and Start Stream, or Cancel). Or select “Schedule broadcast” and click **New** to create a new broadcast with title, description, scheduled time, and Tournament Name.
3. **Set Tournament Name** – Use the broadcast selector and Load/New as above, or click **YT** to copy the current YouTube title from OBS config, or type the tournament name (e.g. “Spring Tournament 2026”) in the Tournament Name field.
4. **Set Base Recording Directory** – Enter or browse to the root folder where you want recordings (e.g. `/Users/you/Recordings`).
5. **Apply** – Click **Apply** so OBS’s recording path becomes `BasePath/TournamentName/%A/`. New recordings will go into subfolders by weekday (e.g. `Spring Tournament 2026/Saturday/`).
6. **Optional: Auto-organize on stop** – If you leave recordings in a single folder and prefer to move them after each stream, leave “Auto-organize on stop” checked. When you stop recording, the plugin will move the last recording into `BasePath/TournamentName/<Weekday>/<filename>` after a short delay.

**YouTube API and OAuth:** Listing and creating broadcasts uses the YouTube Data API v3. The plugin uses Google OAuth 2.0 for desktop (browser sign-in, redirect to localhost). To enable it, you may need to create a Google Cloud project, enable the YouTube Data API v3, and create an OAuth 2.0 Client ID (Desktop app). Configure the plugin with that client ID if the build does not include one (see plugin or build documentation).

Resulting layout example:

```
/Users/you/Recordings/
  Spring Tournament 2026/
    Saturday/
      2026-03-03 14-30-00.mkv
    Sunday/
      2026-03-04 09-00-00.mkv
```

## License

GPL-2.0-or-later (same as OBS Studio).
