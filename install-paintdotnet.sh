#!/usr/bin/env bash
#
# install-paintdotnet.sh - Build patched Wine, install Paint.NET-on-Wine, and
# set up desktop integration plus a standalone uninstaller.
#
# Usage:
#   ./install-paintdotnet.sh [install] [options]
#   ./install-paintdotnet.sh uninstall [options]

set -euo pipefail

SCRIPT_PATH="$(readlink -f -- "${BASH_SOURCE[0]}")"
REPO_DIR="$(cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)"

XDG_DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
DEFAULT_INSTALL_DIR="$XDG_DATA/paintdotnet-wine"
DEFAULT_BIN_DIR="$HOME/.local/bin"

ACTION="install"
INSTALL_DIR="$DEFAULT_INSTALL_DIR"
BIN_DIR="$DEFAULT_BIN_DIR"
BUILD_DIR=""
CUSTOM_BUILD_DIR=0
THEME="dark"
PDN_ZIP=""
SKIP_BUILD=0
KEEP_BUILD=0
JOBS=""
ASSUME_YES=0
KEEP_PREFIX=0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

say() {
    printf '\n==> %s\n' "$*"
}

warn() {
    printf 'warning: %s\n' "$*" >&2
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

have() {
    command -v "$1" >/dev/null 2>&1
}

abs_path() {
    python3 -c 'import os, sys; print(os.path.abspath(os.path.expanduser(sys.argv[1])))' "$1"
}

usage() {
    cat <<EOF
Usage:
  $(basename "$0") [install] [options]
  $(basename "$0") uninstall [options]

Actions:
  install             Build patched Wine, install Paint.NET & desktop launcher (default)
  uninstall           Remove Paint.NET launcher, desktop entry, Wine build, and prefix

Options:
  --install-dir DIR   Installation directory (default: $DEFAULT_INSTALL_DIR)
  --bin-dir DIR       Directory for 'paintdotnet' launcher (default: $DEFAULT_BIN_DIR)
  --build-dir DIR     Out-of-tree Wine build directory (default: <install-dir>/build)
  --theme MODE        Windows UI theme: 'dark' or 'light' (default: dark)
  --pdn-zip PATH      Install Paint.NET from a local portable.x64.wine.zip archive
  --skip-build        Reuse existing <install-dir>/wine/bin/wine without rebuilding
  --keep-build        Keep temporary Wine build directory after installation
  -j, --jobs N        Parallel make jobs (default: nproc)
  -y, --yes           Non-interactive mode; skip confirmation prompts
  --keep-prefix       With 'uninstall': keep <install-dir>/wineprefix user data
  --uninstall         Alias for the 'uninstall' action
  -h, --help          Show this help message
EOF
}

parse_args() {
    if [ $# -gt 0 ]; then
        case "$1" in
            install)
                ACTION="install"
                shift
                ;;
            uninstall|--uninstall)
                ACTION="uninstall"
                shift
                ;;
        esac
    fi

    while [ $# -gt 0 ]; do
        case "$1" in
            --install-dir)
                [ $# -ge 2 ] || die "--install-dir requires an argument"
                INSTALL_DIR="$2"
                shift 2
                ;;
            --bin-dir)
                [ $# -ge 2 ] || die "--bin-dir requires an argument"
                BIN_DIR="$2"
                shift 2
                ;;
            --build-dir)
                [ $# -ge 2 ] || die "--build-dir requires an argument"
                BUILD_DIR="$2"
                CUSTOM_BUILD_DIR=1
                shift 2
                ;;
            --theme)
                [ $# -ge 2 ] || die "--theme requires 'dark' or 'light'"
                THEME="$2"
                shift 2
                ;;
            --pdn-zip)
                [ $# -ge 2 ] || die "--pdn-zip requires a file path"
                PDN_ZIP="$2"
                shift 2
                ;;
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            --keep-build)
                KEEP_BUILD=1
                shift
                ;;
            -j|--jobs)
                [ $# -ge 2 ] || die "$1 requires a number"
                JOBS="$2"
                shift 2
                ;;
            -y|--yes)
                ASSUME_YES=1
                shift
                ;;
            --keep-prefix)
                KEEP_PREFIX=1
                shift
                ;;
            --uninstall)
                ACTION="uninstall"
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "Unknown option: $1 (try --help)"
                ;;
        esac
    done

    case "$THEME" in
        dark|light) ;;
        *) die "Invalid --theme '$THEME'; expected 'dark' or 'light'" ;;
    esac

    if [ -z "$JOBS" ]; then
        if have nproc; then
            JOBS="$(nproc)"
        else
            JOBS=4
        fi
    fi

    if [ -n "$PDN_ZIP" ]; then
        PDN_ZIP="$(abs_path "$PDN_ZIP")"
        [ -f "$PDN_ZIP" ] || die "Paint.NET archive not found: $PDN_ZIP"
    fi

    INSTALL_DIR="$(abs_path "$INSTALL_DIR")"
    BIN_DIR="$(abs_path "$BIN_DIR")"
    if [ -z "$BUILD_DIR" ]; then
        BUILD_DIR="$INSTALL_DIR/build"
    else
        BUILD_DIR="$(abs_path "$BUILD_DIR")"
    fi
}

check_prereqs() {
    say "Checking prerequisites"
    local missing=()
    local cmd

    if [ "$SKIP_BUILD" -eq 0 ]; then
        if ! have gcc && ! have clang; then
            missing+=("gcc or clang")
        fi
        for cmd in make flex bison; do
            have "$cmd" || missing+=("$cmd")
        done
    fi

    for cmd in python3 winetricks cabextract; do
        have "$cmd" || missing+=("$cmd")
    done

    if [ -z "$PDN_ZIP" ]; then
        have curl || missing+=("curl")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        die "Missing required tools: ${missing[*]}. Install them via your package manager and retry."
    fi
}

build_and_install_wine() {
    local wine_dir="$INSTALL_DIR/wine"

    if [ "$SKIP_BUILD" -eq 1 ]; then
        [ -x "$wine_dir/bin/wine" ] || die "--skip-build specified, but $wine_dir/bin/wine was not found"
        [ -x "$wine_dir/bin/wineserver" ] || die "--skip-build specified, but $wine_dir/bin/wineserver was not found"
        say "Skipping Wine build; reusing $wine_dir"
        return 0
    fi

    [ -x "$REPO_DIR/configure" ] || die "Could not find executable $REPO_DIR/configure"

    say "Configuring patched Wine (--enable-win64) in $BUILD_DIR"
    mkdir -p "$BUILD_DIR" "$wine_dir"

    (
        cd "$BUILD_DIR"
        "$REPO_DIR/configure" --enable-win64 --prefix="$wine_dir"
        say "Building Wine with $JOBS jobs (this takes a while)"
        make -j"$JOBS"
        say "Installing Wine into $wine_dir"
        make install -j"$JOBS"
    )

    if [ "$KEEP_BUILD" -eq 0 ] && [ "$CUSTOM_BUILD_DIR" -eq 0 ]; then
        say "Removing temporary build directory $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
}

install_paintdotnet_app() {
    local app_dir="$INSTALL_DIR/app"
    local staging_dir="$INSTALL_DIR/.app-staging.$$"
    local tmp_zip=""
    local zip_path="$PDN_ZIP"

    rm -rf "$staging_dir"
    mkdir -p "$staging_dir"

    if [ -z "$zip_path" ]; then
        say "Fetching latest Paint.NET-on-Wine release metadata"
        local download_url
        download_url="$(
            curl -fsSL "https://api.github.com/repos/paintdotnet/Paint.NET-on-Wine/releases/latest" \
                | python3 -c '
import json, sys
data = json.load(sys.stdin)
for asset in data.get("assets", []):
    if "portable.x64.wine" in asset.get("name", ""):
        print(asset["browser_download_url"])
        sys.exit(0)
sys.exit(1)
'
        )" || die "Could not locate portable.x64.wine asset in paintdotnet/Paint.NET-on-Wine releases"

        tmp_zip="$(mktemp "${TMPDIR:-/tmp}/paintdotnet-wine.XXXXXX.zip")"
        say "Downloading $download_url"
        curl -fL --progress-bar -o "$tmp_zip" "$download_url"
        zip_path="$tmp_zip"
    else
        say "Extracting local Paint.NET archive: $zip_path"
    fi

    python3 -m zipfile -e "$zip_path" "$staging_dir"

    if [ -n "$tmp_zip" ]; then
        rm -f "$tmp_zip"
    fi

    # Handle archives that wrap contents inside a single top-level directory
    if [ ! -f "$staging_dir/paintdotnet.exe" ]; then
        local nested_exe
        nested_exe="$(find "$staging_dir" -maxdepth 2 -name paintdotnet.exe -print -quit || true)"
        if [ -n "$nested_exe" ]; then
            local nested_root
            nested_root="$(dirname "$nested_exe")"
            mv "$nested_root"/* "$staging_dir"/
        fi
    fi

    [ -f "$staging_dir/paintdotnet.exe" ] || die "paintdotnet.exe not found inside archive"

    rm -rf "$app_dir"
    mv "$staging_dir" "$app_dir"
}

configure_prefix() {
    local wine_dir="$INSTALL_DIR/wine"
    local wine_bin="$wine_dir/bin/wine"
    local wineserver_bin="$wine_dir/bin/wineserver"
    local prefix_dir="$INSTALL_DIR/wineprefix"

    say "Configuring Wine prefix at $prefix_dir"
    mkdir -p "$prefix_dir"

    export PATH="$wine_dir/bin:$PATH"
    export WINE="$wine_bin"
    export WINESERVER="$wineserver_bin"
    export WINEPREFIX="$prefix_dir"
    export WINEDLLOVERRIDES="mshtml="

    if [ ! -d "$prefix_dir/drive_c" ]; then
        "$wine_bin" wineboot --init
        "$wineserver_bin" -w
    fi

    "$wine_bin" winecfg -v win11
    "$wineserver_bin" -w

    # Enable ClearType font smoothing
    "$wine_bin" reg add 'HKCU\Control Panel\Desktop' /v FontSmoothingType /t REG_DWORD /d 2 /f >/dev/null
    "$wineserver_bin" -w

    # Install DXVK (winetricks exits non-zero when invoking missing 32-bit regedit
    # in a pure 64-bit prefix; apply the generated .reg with 64-bit regedit.exe).
    say "Installing DXVK into $prefix_dir"
    WINE="$wine_bin" WINESERVER="$wineserver_bin" WINEPREFIX="$prefix_dir" winetricks -q dxvk || true
    if [ -f "$prefix_dir/drive_c/windows/Temp/_dxvk/override-dll.reg" ]; then
        "$wine_bin" regedit.exe /S 'C:\windows\Temp\_dxvk\override-dll.reg'
        "$wineserver_bin" -w
    elif [ -f "$prefix_dir/drive_c/windows/temp/_dxvk/override-dll.reg" ]; then
        "$wine_bin" regedit.exe /S 'C:\windows\temp\_dxvk\override-dll.reg'
        "$wineserver_bin" -w
    fi
    "$wineserver_bin" -w
    python3 - "$prefix_dir/drive_c/windows/system32/d3d11.dll" <<'PY' || die "DXVK installation failed: system32/d3d11.dll is not DXVK"
import sys
with open(sys.argv[1], "rb") as f:
    data = f.read()
if b"dxvk" not in data.lower():
    sys.exit(1)
PY
    grep -Eq '"\*?d3d11"="native"' "$prefix_dir/user.reg" || die "DXVK installation failed: native d3d11 override missing in user.reg"

    local light_val=0
    if [ "$THEME" = "light" ]; then
        light_val=1
    fi
    say "Applying Windows 11 '$THEME' theme (AppsUseLightTheme=$light_val)"
    "$wine_bin" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' \
        /v AppsUseLightTheme /t REG_DWORD /d "$light_val" /f >/dev/null
    "$wine_bin" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' \
        /v SystemUsesLightTheme /t REG_DWORD /d "$light_val" /f >/dev/null
    "$wine_bin" wineboot -u
    "$wineserver_bin" -w
}

install_launcher() {
    local launcher_path="$BIN_DIR/paintdotnet"
    local wine_bin_dir="$INSTALL_DIR/wine/bin"
    local app_dir="$INSTALL_DIR/app"
    local prefix_dir="$INSTALL_DIR/wineprefix"

    say "Installing launcher at $launcher_path"
    mkdir -p "$BIN_DIR"

    python3 - "$launcher_path" "$wine_bin_dir" "$app_dir" "$prefix_dir" <<'PY'
import sys

launcher_path, wine_bin_dir, app_dir, prefix_dir = sys.argv[1:5]

def sh_quote(s: str) -> str:
    return "'" + s.replace("'", "'\"'\"'") + "'"

content = f"""#!/usr/bin/env bash
set -euo pipefail

WINE_BIN_DIR={sh_quote(wine_bin_dir)}
WINE="$WINE_BIN_DIR/wine"
WINESERVER="$WINE_BIN_DIR/wineserver"
APP_DIR={sh_quote(app_dir)}
PREFIX_DIR={sh_quote(prefix_dir)}

export PATH="$WINE_BIN_DIR:$PATH"
export WINEPREFIX="$PREFIX_DIR"
export WINESERVER
export WINEDEBUG="${{WINEDEBUG:--all}}"
export DXVK_LOG_LEVEL="${{DXVK_LOG_LEVEL:-error}}"
export WINEDLLOVERRIDES="${{WINEDLLOVERRIDES:-mshtml=;d3dcompiler_47=n}}"

to_winpath() {{
    local out
    if [ -x "$WINE_BIN_DIR/winepath" ]; then
        out="$("$WINE_BIN_DIR/winepath" -w "$1" 2>/dev/null || true)"
    else
        out="$("$WINE" winepath -w "$1" 2>/dev/null || true)"
    fi
    printf '%s' "${{out%$'\\r'}}"
}}

if [ -z "${{DXVK_CONFIG_FILE:-}}" ] && [ -f "$APP_DIR/dxvk.conf" ]; then
    dxvk_win="$(to_winpath "$APP_DIR/dxvk.conf")"
    if [ -n "$dxvk_win" ]; then
        export DXVK_CONFIG_FILE="$dxvk_win"
    fi
fi

ARGS=()
for arg in "$@"; do
    if [[ "$arg" != /* ]] && [ -e "$arg" ]; then
        win_arg="$(to_winpath "$arg")"
        if [ -n "$win_arg" ]; then
            ARGS+=("$win_arg")
            continue
        fi
    fi
    ARGS+=("$arg")
done

cd "$APP_DIR"
exec "$WINE" "$APP_DIR/paintdotnet.exe" "${{ARGS[@]}}"
"""

with open(launcher_path, "w", encoding="utf-8") as f:
    f.write(content)
PY

    chmod +x "$launcher_path"
}

install_desktop_integration() {
    local app_dir="$INSTALL_DIR/app"
    local launcher_path="$BIN_DIR/paintdotnet"
    local xdg_data="${XDG_DATA_HOME:-$HOME/.local/share}"
    local icons_dir="$xdg_data/icons/hicolor/256x256/apps"
    local apps_dir="$xdg_data/applications"
    local icon_png="$icons_dir/paintdotnet.png"
    local icon_ico="$icons_dir/paintdotnet.ico"
    local desktop_file="$apps_dir/paintdotnet.desktop"
    local chosen_icon=""

    say "Installing desktop entry and icon"
    mkdir -p "$icons_dir" "$apps_dir"

    if [ -f "$app_dir/paintdotnet.ico" ]; then
        chosen_icon="$(python3 - "$app_dir/paintdotnet.ico" "$icon_png" "$icon_ico" <<'PY'
import shutil, struct, sys

ico_path, out_png, out_ico = sys.argv[1:4]
with open(ico_path, "rb") as f:
    data = f.read()

best = None  # (width, size, chunk)
if len(data) >= 6:
    _reserved, _type, count = struct.unpack("<HHH", data[:6])
    for i in range(count):
        base = 6 + i * 16
        if base + 16 > len(data):
            break
        w, h, _cc, _res, _pl, _bpp, size, offset = struct.unpack("<BBBBHHII", data[base:base+16])
        real_w = 256 if w == 0 else w
        chunk = data[offset:offset+size]
        if chunk.startswith(b"\x89PNG\r\n\x1a\n"):
            cand = (real_w, len(chunk), chunk)
            if best is None or cand[:2] > best[:2]:
                best = cand

if best is not None:
    with open(out_png, "wb") as f:
        f.write(best[2])
    print(out_png)
else:
    shutil.copy2(ico_path, out_ico)
    print(out_ico)
PY
)"
    fi

    python3 - "$desktop_file" "$launcher_path" "$chosen_icon" <<'PY'
import sys

desktop_path, launcher_path, icon_path = sys.argv[1:4]
icon_line = f"Icon={icon_path}\n" if icon_path else ""
exec_escaped = launcher_path.replace("\\", "\\\\").replace('"', '\\"').replace(" ", "\\ ")

content = (
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Paint.NET\n"
    "Comment=Image and photo editing software (via patched Wine)\n"
    f"Exec={exec_escaped} %F\n"
    f"{icon_line}"
    "Terminal=false\n"
    "Categories=Graphics;2DGraphics;RasterGraphics;\n"
    "MimeType=image/png;image/jpeg;image/bmp;image/gif;image/webp;image/tiff;image/x-tga;\n"
    "StartupWMClass=paintdotnet.exe\n"
)

with open(desktop_path, "w", encoding="utf-8") as f:
    f.write(content)
PY

    chmod 644 "$desktop_file"

    if have update-desktop-database; then
        update-desktop-database "$apps_dir" >/dev/null 2>&1 || true
    fi
    if have gtk-update-icon-cache; then
        gtk-update-icon-cache -f -t "$xdg_data/icons/hicolor" >/dev/null 2>&1 || true
    fi

    INSTALLED_DESKTOP_FILE="$desktop_file"
    INSTALLED_ICON_FILE="$chosen_icon"
}

install_uninstaller() {
    local uninstall_script="$INSTALL_DIR/uninstall.sh"
    local uninstall_link="$BIN_DIR/paintdotnet-uninstall"

    say "Creating standalone uninstaller at $uninstall_script"

    python3 - "$uninstall_script" "$INSTALL_DIR" "$BIN_DIR" "$INSTALLED_DESKTOP_FILE" "$INSTALLED_ICON_FILE" <<'PY'
import sys

out_path, install_dir, bin_dir, desktop_file, icon_file = sys.argv[1:6]

def sh_quote(s: str) -> str:
    return "'" + s.replace("'", "'\"'\"'") + "'"

content = f"""#!/usr/bin/env bash
set -euo pipefail

INSTALL_DIR={sh_quote(install_dir)}
BIN_DIR={sh_quote(bin_dir)}
DESKTOP_FILE={sh_quote(desktop_file)}
ICON_FILE={sh_quote(icon_file)}

ASSUME_YES=0
KEEP_PREFIX=0

while [ $# -gt 0 ]; do
    case "$1" in
        -y|--yes)
            ASSUME_YES=1
            shift
            ;;
        --keep-prefix)
            KEEP_PREFIX=1
            shift
            ;;
        -h|--help)
            cat <<EOF
Usage: $(basename "$0") [-y|--yes] [--keep-prefix]

Removes the Paint.NET Wine installation at:
  Install directory : $INSTALL_DIR
  Launcher scripts  : $BIN_DIR/paintdotnet, $BIN_DIR/paintdotnet-uninstall
  Desktop entry     : $DESKTOP_FILE
EOF
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

if [ "$ASSUME_YES" -eq 0 ]; then
    echo "This will remove Paint.NET Wine from:"
    echo "  $INSTALL_DIR"
    echo "  $BIN_DIR/paintdotnet"
    echo "  $DESKTOP_FILE"
    if [ "$KEEP_PREFIX" -eq 1 ]; then
        echo "  (Keeping $INSTALL_DIR/wineprefix)"
    fi
    printf 'Continue? [y/N] '
    read -r reply
    case "$reply" in
        y|Y|yes|YES) ;;
        *)
            echo "Cancelled."
            exit 0
            ;;
    esac
fi

if [ -x "$INSTALL_DIR/wine/bin/wineserver" ] && [ -d "$INSTALL_DIR/wineprefix" ]; then
    WINEPREFIX="$INSTALL_DIR/wineprefix" "$INSTALL_DIR/wine/bin/wineserver" -k >/dev/null 2>&1 || true
fi

rm -f "$BIN_DIR/paintdotnet" "$BIN_DIR/paintdotnet-uninstall"
[ -n "$DESKTOP_FILE" ] && rm -f "$DESKTOP_FILE"
[ -n "$ICON_FILE" ] && rm -f "$ICON_FILE"

if [ "$KEEP_PREFIX" -eq 1 ]; then
    rm -rf "$INSTALL_DIR/wine" "$INSTALL_DIR/app" "$INSTALL_DIR/build" "$INSTALL_DIR/uninstall.sh"
else
    rm -rf "$INSTALL_DIR"
fi

if command -v update-desktop-database >/dev/null 2>&1 && [ -n "$DESKTOP_FILE" ]; then
    update-desktop-database "$(dirname "$DESKTOP_FILE")" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    xdg_data="${{XDG_DATA_HOME:-$HOME/.local/share}}"
    [ -d "$xdg_data/icons/hicolor" ] && gtk-update-icon-cache -f -t "$xdg_data/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "Paint.NET Wine uninstalled."
"""

with open(out_path, "w", encoding="utf-8") as f:
    f.write(content)
PY

    chmod +x "$uninstall_script"
    ln -sf "$uninstall_script" "$uninstall_link"
}

run_uninstall() {
    local uninstall_script="$INSTALL_DIR/uninstall.sh"
    local args=()
    [ "$ASSUME_YES" -eq 1 ] && args+=("--yes")
    [ "$KEEP_PREFIX" -eq 1 ] && args+=("--keep-prefix")

    if [ -x "$uninstall_script" ]; then
        exec "$uninstall_script" "${args[@]}"
    fi

    # Fallback when uninstall.sh is absent
    local xdg_data="${XDG_DATA_HOME:-$HOME/.local/share}"
    local desktop_file="$xdg_data/applications/paintdotnet.desktop"
    local icon_png="$xdg_data/icons/hicolor/256x256/apps/paintdotnet.png"
    local icon_ico="$xdg_data/icons/hicolor/256x256/apps/paintdotnet.ico"

    if [ "$ASSUME_YES" -eq 0 ]; then
        echo "No standalone uninstall.sh found in $INSTALL_DIR."
        echo "Will clean:"
        echo "  $INSTALL_DIR"
        echo "  $BIN_DIR/paintdotnet"
        echo "  $BIN_DIR/paintdotnet-uninstall"
        echo "  $desktop_file"
        printf 'Continue? [y/N] '
        read -r reply
        case "$reply" in
            y|Y|yes|YES) ;;
            *) echo "Cancelled."; exit 0 ;;
        esac
    fi

    if [ -x "$INSTALL_DIR/wine/bin/wineserver" ] && [ -d "$INSTALL_DIR/wineprefix" ]; then
        WINEPREFIX="$INSTALL_DIR/wineprefix" "$INSTALL_DIR/wine/bin/wineserver" -k >/dev/null 2>&1 || true
    fi

    rm -f "$BIN_DIR/paintdotnet" "$BIN_DIR/paintdotnet-uninstall" "$desktop_file" "$icon_png" "$icon_ico"

    if [ "$KEEP_PREFIX" -eq 1 ]; then
        rm -rf "$INSTALL_DIR/wine" "$INSTALL_DIR/app" "$INSTALL_DIR/build" "$INSTALL_DIR/uninstall.sh"
    else
        rm -rf "$INSTALL_DIR"
    fi

    if have update-desktop-database; then
        update-desktop-database "$xdg_data/applications" >/dev/null 2>&1 || true
    fi
    say "Uninstall complete"
}

main() {
    parse_args "$@"

    if [ "$ACTION" = "uninstall" ]; then
        run_uninstall
        exit 0
    fi

    check_prereqs
    build_and_install_wine
    install_paintdotnet_app
    configure_prefix
    install_launcher
    install_desktop_integration
    install_uninstaller

    say "Installation complete!"
    printf '  Launcher   : %s/paintdotnet\n' "$BIN_DIR"
    printf '  Uninstaller: %s/paintdotnet-uninstall (or %s/uninstall.sh)\n' "$BIN_DIR" "$INSTALL_DIR"
    printf '  Desktop    : %s\n' "$INSTALLED_DESKTOP_FILE"
    printf '  Wine       : %s/wine\n' "$INSTALL_DIR"
    printf '  Prefix     : %s/wineprefix\n' "$INSTALL_DIR"
    printf '  App        : %s/app\n' "$INSTALL_DIR"

    case ":$PATH:" in
        *":$BIN_DIR:"*) ;;
        *)
            printf '\nNote: %s is not currently in your PATH. Add it (e.g. export PATH="%s:$PATH")\n' "$BIN_DIR" "$BIN_DIR"
            printf '      or run "%s/paintdotnet" directly / launch Paint.NET from your application menu.\n' "$BIN_DIR"
            ;;
    esac
}

main "$@"
