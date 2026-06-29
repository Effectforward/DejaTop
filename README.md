# DejaTop

> **v1.0.0**

Zero-dependency C++17 CLI tool that generates `.desktop` entries for Linux
native apps, AppImages, GOG games, and Windows games run through Wine or
Steam Proton (including GE-Proton). No configuration files. No GUI.

---

## Features

| Feature | Details |
|---|---|
| **Zero Dependencies** | Pure C++17 — no GTK, Qt, ncurses, or Python |
| **Smart Icon Discovery** | Scored scan with PNG/ICO binary header parsing; picks game-specific icons over shared engine defaults |
| **Prefix Auto-Discovery** | Scans Steam (native + Flatpak), Heroic, and Lutris for existing Proton prefixes |
| **GE-Proton Priority** | Prefers GE-Proton → Experimental → official; numeric version ordering |
| **Steam Launch Options** | Reads per-game options from `shortcuts.vdf` for non-Steam shortcuts |
| **Environment Variables** | `--env KEY=VALUE`, repeatable, all runners, values with spaces handled correctly |
| **Wrapper Logging** | Proton launcher output logged to `~/.local/share/DejaTop/logs/<name>.log` |
| **Entry Management** | `--show`, `--rename`, `--update`, `--list`, `--delete`, `--replace-icon` |
| **Collision Guard** | `--force` required to overwrite an existing entry |
| **Validation** | Runs `desktop-file-validate` post-creation if available |
| **Tab Completion** | Bash completion for all commands, flags, and existing entry names |

---

## Installation

```bash
make
make install   # installs to ~/.local/bin/ and ~/.local/share/man/man1/
make uninstall
```

---

## Usage

### Creating entries

```bash
# Native Linux game (GOG, AppImage, bare script)
dejatop ~/GOG-Games/Portal/start.sh --name "Portal"

# Windows game — auto-discovers Proton and prefix
dejatop /mnt/Games/MyGame/game.exe --name "My Game"

# With MangoHud overlay and async shaders
dejatop /path/game.exe --name "My Game" --env MANGOHUD=1 --env DXVK_ASYNC=1

# Override category when auto-detection is wrong
dejatop ~/apps/mpv.AppImage --name "mpv" --category "AudioVideo;Video;"

# All overrides
dejatop /path/game.exe \
  --name "My Game"          \
  --icon /path/to/icon.png  \
  --runner proton           \
  --prefix ~/.local/share/Steam/steamapps/compatdata/12345 \
  --category "Game;"        \
  --force
```

**Category defaults:**
- `Game;` for Wine/Proton runners
- `Game;` for native apps installed under paths matching common game
  directories (`GOG`, `games`, `Heroic`, `Lutris`, `Bottles`)
- `Utility;Application;` for everything else

Use `--category` to override.

**`--env` behaviour:**
- `native` / `wine`: prepends `env KEY=VALUE` to the `Exec=` field
- `proton`: writes `export KEY="VALUE"` lines into the wrapper script

### Managing entries

```bash
# Inspect — icon status, Proton version, prefix path, log size
dejatop --show portal

# Rename an entry (renames desktop file, wrapper, and log automatically)
dejatop --rename "Portal" "Portal (GOG)"

# Refresh the Proton version in an existing wrapper
dejatop --update portal

# Swap a wrong icon
dejatop --replace-icon portal /path/to/better-icon.png

# Delete an entry (removes desktop file and wrapper script)
dejatop --delete portal

# List all managed entries with display name and safe id
dejatop --list
```

### `--show` output

Native entry:
```
DejaTop Entry: Portal
----------------------------------------
  Desktop:    ~/.local/share/applications/dejatop_Portal.desktop
  Name:       Portal
  Category:   Game;
  Exec:       "/home/user/GOG-Games/Portal/start.sh"
  Path:       /home/user/GOG-Games/Portal
  Icon:       .../game/portal/resource/game.ico  ✔
  Wrapper:    (none — native runner)
  Log:        (none)
```

Proton entry:
```
DejaTop Entry: Cyberpunk 2077
----------------------------------------
  Desktop:    ~/.local/share/applications/dejatop_Cyberpunk2077.desktop
  Name:       Cyberpunk 2077
  Category:   Game;
  Exec:       ~/.local/share/DejaTop/wrappers/Cyberpunk2077_wrapper.sh
  Path:       /mnt/Games/Cyberpunk2077
  Icon:       /mnt/Games/Cyberpunk2077/icon.png  ✔
  Wrapper:    ~/.local/share/DejaTop/wrappers/Cyberpunk2077_wrapper.sh  ✔
    Proton:   ~/.local/share/Steam/steamapps/common/GE-Proton10-34
    Prefix:   ~/.local/share/Steam/steamapps/compatdata/1091500
  Log:        ~/.local/share/DejaTop/logs/Cyberpunk2077.log  (2.4 KB)
```

---

## Proton selection

1. Latest GE-Proton (numeric comparison — `GE-Proton10-34` beats `GE-Proton9-27`)
2. Proton - Experimental or latest official Proton
3. Clear error if nothing found

`.sh` and `.AppImage` files are always native — Proton logic never triggers.

---

## Prefix discovery (`.exe` only)

1. Steam native → `~/.local/share/Steam/userdata/*/config/shortcuts.vdf`
2. Steam Flatpak → `~/.var/app/com.valvesoftware.Steam/data/Steam/userdata/`
3. Heroic → `~/.config/heroic/GamesConfig/*.json`
4. Lutris → `~/.config/lutris/games/*.yml`
5. Interactive prompt to create a shared prefix if none found

---

## Planned

- `--batch <dir>` — generate entries for a whole library in one pass
- `--dry-run` — preview without writing
- Combined management commands in a single invocation
- Steam Flatpak GE-Proton execution wrapper

---

## License

[GNU General Public License v3.0](LICENSE)
