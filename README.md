# DejaTop

> **Beta Release — v1.0.0-beta**

**DejaTop** is a zero-dependency C++17 CLI tool that automatically generates `.desktop` entries for Linux native apps, AppImages, GOG games, and Windows games run through Wine or Steam Proton (including GE-Proton).

No configuration files. No GUI. One command.

---

## Features

| Feature | Details |
|---|---|
| **Zero Dependencies** | Pure C++17, no GTK/Qt/ncurses/Python |
| **One-Shot Magic** | Instant desktop entries, zero prompts |
| **Smart Prefix Discovery** | Auto-detects existing prefixes from Steam, Heroic, Lutris |
| **Flatpak Steam Support** | Scans both native and Flatpak Steam userdata |
| **GE-Proton Priority** | Prefers GE-Proton → Experimental → Official Proton |
| **Smart Icon Discovery** | Recursively picks best `.ico`/`.png` icon, or extracts directly from `.exe` files if `icoutils` is installed |
| **Case-Insensitive Management** | `--delete`, `--replace-icon` work with partial names |
| **Tab Completion** | Bash completion script included for all commands, flags, and existing entries |

---

## Installation

```bash
make
make install
```

Installs to `~/.local/bin/dejatop` and man page to `~/.local/share/man/man1/`.

To uninstall:
```bash
make uninstall
```

---

## Usage

### One-Shot Creation

```bash
# Windows game — auto-discovers Proton + existing prefix
dejatop /mnt/Games/ACIII/ACIII.exe --name "Assassin's Creed III"

# Native / GOG / AppImage — no Proton logic, pure exec
dejatop ~/GOG-Games/Portal/start.sh --name "Portal"

# With all overrides
dejatop /path/to/game.exe --name "My Game" --icon /path/to/icon.png --runner proton --prefix ~/.local/share/Steam/steamapps/compatdata/12345
```

For `.exe` files, DejaTop will:
1. Scan Steam (native + Flatpak), Heroic, and Lutris for an existing prefix
2. If none found, prompt `[Y/n]` to create a shared DejaTop prefix
3. Pick the best Proton version (GE-Proton first)
4. Auto-select the best icon from the game directory
5. Generate the `.desktop` entry and Proton wrapper script

### Managing Entries

```bash
# Swap a wrong icon (case-insensitive)
dejatop --replace-icon "assassin" /path/to/icon.png

# Delete an entry
dejatop --delete "portal"

# List all managed entries
dejatop --list
```

### Documentation

```bash
man dejatop
```

---

## Proton Runner Selection

For `.exe` files, Proton is selected in this priority order:

1. **Latest GE-Proton** (e.g. `GE-Proton9-27`) from `~/.steam/root/compatibilitytools.d/`
2. **Proton - Experimental** or latest official Proton from Steam
3. **Error** — if nothing is found, DejaTop exits cleanly with a clear message

> `.sh` and `.AppImage` files are always treated as **native** — Proton/prefix logic is never triggered for them.

---

## Prefix Discovery Order (for `.exe` files)

1. Steam native — `~/.local/share/Steam/userdata/*/config/shortcuts.vdf`
2. Steam Flatpak — `~/.var/app/com.valvesoftware.Steam/data/Steam/userdata/`
3. Heroic — `~/.config/heroic/GamesConfig/*.json`
4. Lutris — `~/.config/lutris/games/*.yml`
5. **Prompt** — if nothing is found, asks `[Y/n]` to create a shared prefix

---

## Roadmap & Todo

- [x] **`.exe` icon extraction** — Icons baked inside `.exe` files can be extracted directly if `icoutils` (`wrestool` + `icotool`) is installed.
- [ ] **Steam Flatpak — Proton execution** — Flatpak Steam prefix *discovery* works. However, running a Proton binary that lives inside the Flatpak sandbox from outside it requires a `flatpak run` wrapper (not yet implemented). Native Steam Proton works fine.
- [ ] **GOG Galaxy prefix detection** — GOG game prefixes are not yet auto-discovered (GOG uses its own config location).
- [ ] **Standalone Wine prefix detection** — Wine prefixes created without Heroic or Lutris (e.g. raw `WINEPREFIX=...` usage) are not scanned.
- [ ] **`--delete` does not remove the wrapper script** — `--delete` removes the `.desktop` file but leaves the wrapper `.sh` in `~/.local/share/DejaTop/wrappers/`. Cleanup not yet implemented.
- [ ] **`--list` shows raw sanitized names** — `--list` strips the `dejatop_` prefix and `.desktop` suffix from the filename, but shows the sanitized name (e.g. `AssassinsCreedIII`) instead of the human-readable `Name=` field from inside the file.
- [ ] **Runner detection for unknown extensions** — Files without `.exe`, `.sh`, or `.AppImage` extensions (e.g. bare binaries with no extension) are always treated as native. There is no fallback prompt to let the user choose the runner.
- [ ] **`--runner wine` has no prefix management** — When `--runner wine` is passed manually, the `Exec=` is set to `wine "path/to/game.exe"` directly with no wrapper script and no prefix handling.
- [ ] **Heroic JSON parsing is naive** — The Heroic parser does a simple string search for `"winePrefix":` in the file. If Heroic changes its JSON schema or uses nested configs it may miss the prefix.
- [ ] **`sanitizeName` strips all special characters** — Game names with spaces, dots, or unicode (e.g. "DOOM (2016)") get heavily sanitized, making filenames like `DOOM2016` with no separator. A smarter slug function would preserve readability.
- [ ] **`--delete` and `--replace-icon` match on filename, not `Name=`** — Searching `"portal"` matches `dejatop_Portal.desktop` by filename. If the user names their entry something different from the filename (via `--name`), the lookup may fail or match the wrong entry.
- [ ] **Library Batch Mode** — Add a `--batch` command to scan entire game directories (e.g., `~/GOG Games/`) and generate entries for all installed games in a single pass.
- [ ] **Entry Management** — Add `--rename` to seamlessly change the name of existing entries, and `--update` to refresh the selected Proton version.
- [ ] **Dry-Run Previews** — Add `--dry-run` to safely preview the generated desktop entry and wrapper script without writing anything to disk.
- [ ] **Enhanced Launcher Discovery** — Improve detection logic for Heroic Games Launcher, Lutris, and standalone Wine prefixes.
- [ ] **Custom Environment Variables** — Support reading Steam's per-game launch options, and add a `--env` flag to pass custom variables (like `MANGOHUD=1`) to any runner.
- [ ] **Robustness & Validation** — Add strict verification using `desktop-file-validate` to ensure 100% standard compliance, and fix edge cases involving paths with special characters.

---

## License

[GNU General Public License v3.0](LICENSE)
