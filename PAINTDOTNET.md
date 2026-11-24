# Running Paint.NET on Wine (`wine-pdn-fix`)

This repository is a patched **Wine 11.22** tree containing compatibility and modern UI patches for **Paint.NET 5.2+** (`paintdotnet/Paint.NET-on-Wine`).

## Included Patches

### Graphics, Imaging & Core Compatibility
- **`dxgi`**: Added `DXGIDeclareAdapterRemovalSupport()` stub and `IDXGIOutput6::CheckHardwareCompositionSupport()` `S_OK` stub.
- **`d3d11`**: Implemented `ID3D11Device::GetCreationFlags()` and `ID3D10Device1::GetCreationFlags()` so Paint.NET's D3D11 device pool properly reuses `D3D11_CREATE_DEVICE_BGRA_SUPPORT` devices.
- **`d2d1` & IDLs**: Preserved Wine 11.19–11.22 upstream Direct2D fixes and ported WIC bitmap-lock bitmap creation (`IID_IWICBitmapLock`), `DXGI_FORMAT_A8_UNORM` WIC render target support, `CLSID_D2D1Histogram`, `ID2D1EffectContext1`/`2`, `ID2D1ColorContext1`, `ID2D1GradientStopCollection1`, command-list draw commands, analytical geometry bounds, and factory locking.
- **`windowscodecs`**: Implemented `PixelFormatInfo_GetColorContext` (embedded Compact ICC sRGB/scRGB profiles), `WICSerializeMetadataContent`, and `GUID_WICPixelFormat64bppPRGBAHalf` registration.
- **`vkd3d`**: Updated `d3d12_reflection_GetMinFeatureLevel` (used by `d3dcompiler_43`/`47` `D3DReflect`) to return `D3D_FEATURE_LEVEL_10_0`.
- **`dwrite`**: Added overflow-safe `OffsetRectSafe` clamping in `glyphrunanalysis_get_texturebounds` and `glyphrunanalysis_render`.
- **`user32`**: Added `SetWindowFeedbackSetting` stub.

### WinRT Composition & Animation
- **`uianimation`**: Implemented `IUIAnimationManager`, `IUIAnimationStoryboard`, `IUIAnimationTransition`, `IUIAnimationTransitionLibrary`, `IUIAnimationVariable`, and `IUIAnimationTimer`.
- **`coremessaging`**: Implemented `Windows.System.DispatcherQueue` runtimeclass (`IDispatcherQueue` / `IDispatcherQueue2` / `IDispatcherQueueStatics`) alongside upstream `DispatcherQueueController`.
- **`dcomp` & `windows.ui.composition.core.idl`**: Implemented `DllGetActivationFactory` for `Windows.UI.Composition.Core.CompositorController`, plus `DCompositionBoostCompositorClock` and `CreatePresentationFactory` stubs.

### Modern Windows 11 Light/Dark UI
- **`dwmapi`**: Implemented `DWMWA_USE_IMMERSIVE_DARK_MODE` (attributes `19` and `20`) in `DwmSetWindowAttribute` / `DwmGetWindowAttribute`, persisting per-window state and falling back to `AppsUseLightTheme`.
- **`win32u`**: Modernized `WS_EX_TOOLWINDOW` non-client rendering (`Tools`, `History`, `Layers`, `Colors`) with flat 1px borders, Light/Dark mode caption bars, non-bold captions, and flat close glyphs instead of classic Windows 95 3D bevels.
- **`aero.msstyles`**: Replaced bright blue XP/classic caption defaults and bold Tahoma metrics with clean modern Windows 11 light colors and `Segoe UI` metrics.

---

## Quick Install & Uninstall (Automated)

To build Wine from this repository, download Paint.NET-on-Wine, configure a DXVK + Windows 11 prefix, install a `paintdotnet` launcher + desktop menu entry, and generate an uninstaller in one step:

```bash
./install-paintdotnet.sh
```

Common options:
- `./install-paintdotnet.sh --theme light` — configure Windows 11 Light Mode instead of the default Dark Mode.
- `./install-paintdotnet.sh --install-dir ~/opt/paintdotnet-wine --bin-dir ~/.local/bin` — customize install locations.
- `./install-paintdotnet.sh --pdn-zip /path/to/portable.x64.wine.zip` — install from a local archive instead of fetching from GitHub Releases.
- `./install-paintdotnet.sh --help` — list all options (`--skip-build`, `--keep-build`, `-j N`, `-y`).

Once installed, run Paint.NET from the application menu or terminal:

```bash
paintdotnet [image-or-pdn-file]
```

To uninstall everything (launcher, `.desktop` entry, icon, built Wine, Paint.NET app, and Wine prefix):

```bash
paintdotnet-uninstall
# or from this repository:
./install-paintdotnet.sh uninstall
```

Pass `--keep-prefix` to preserve your Wine prefix settings when uninstalling.

---

## 1. Prerequisites

Install Wine's 64-bit build dependencies plus `clang`, `lld`, `winetricks`, `cabextract`, `curl`, and Vulkan drivers (including Mesa `lavapipe` / `vulkan-swrast` if you want a software GPU fallback):

- **Compilers & build tools**: `gcc`, `clang`, `lld`, `make`, `flex`, `bison`, `pkg-config`
- **Runtime / prefix tools**: `winetricks`, `cabextract`, `curl`, `python3`
- **Libraries**: `x11`, `wayland-client`, `freetype2`, `fontconfig`, `vulkan`, `gnutls`, etc. (see Wine's `configure` output if any development header is missing)

---

## 2. Build Wine Out-of-Tree

Keep build artifacts outside the repository:

```bash
mkdir -p ../wine-build
cd ../wine-build
/path/to/wine-pdn-fix/configure --enable-win64
make -j"$(nproc)"
```

*(Optionally pass `--prefix="$PWD/install"` to `configure` and run `make install -j"$(nproc)"` if you want an installed tree.)*

---

## 3. Download Paint.NET-on-Wine

Download the latest experimental x64 portable package from [`paintdotnet/Paint.NET-on-Wine`](https://github.com/paintdotnet/Paint.NET-on-Wine/releases):

```bash
mkdir -p ~/paintdotnet
cd ~/paintdotnet
curl -L -o pdn.zip \
  "$(curl -s https://api.github.com/repos/paintdotnet/Paint.NET-on-Wine/releases/latest \
    | python3 -c 'import sys,json; print(next(a["browser_download_url"] for a in json.load(sys.stdin)["assets"] if "portable.x64.wine" in a["name"]))')"
python3 -c "import zipfile; zipfile.ZipFile('pdn.zip').extractall('app')"
```

---

## 4. Create and Configure the Wine Prefix

Initialize a fresh `win11` 64-bit Wine prefix with ClearType enabled:

```bash
export WINE_BUILD=/path/to/wine-build
export WINE="$WINE_BUILD/wine"
export WINESERVER="$WINE_BUILD/server/wineserver"
export WINEPREFIX="$HOME/.local/share/paintdotnet/wineprefix"

WINEDLLOVERRIDES="mshtml=" "$WINE" wineboot --init
"$WINESERVER" -w

"$WINE" winecfg -v win11
"$WINESERVER" -w

# Enable ClearType font smoothing
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v FontSmoothingType /t REG_DWORD /d 2 /f
```

### Install DXVK (Workaround for Pure 64-bit Prefixes)

Because this build uses `--enable-win64` without 32-bit `syswow64` binaries, `winetricks -q dxvk` copies the 64-bit DXVK DLLs into `system32/` and then fails when invoking `C:\windows\syswow64\regedit.exe`. Apply the generated override registry file directly with 64-bit `regedit.exe`:

```bash
WINE="$WINE" WINESERVER="$WINESERVER" WINEPREFIX="$WINEPREFIX" winetricks -q dxvk || true
"$WINE" regedit.exe /S 'C:\windows\Temp\_dxvk\override-dll.reg'
"$WINESERVER" -w
```

### Choose Modern Dark Mode or Light Mode

**Dark Mode (Windows 11 style):**

```bash
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 0 /f
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v SystemUsesLightTheme /t REG_DWORD /d 0 /f
"$WINE" wineboot -u && "$WINESERVER" -w
```

**Light Mode:**

```bash
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 1 /f
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v SystemUsesLightTheme /t REG_DWORD /d 1 /f
"$WINE" wineboot -u && "$WINESERVER" -w
```

---

## 5. Launch Paint.NET

```bash
export WINEPREFIX="$HOME/.local/share/paintdotnet/wineprefix"
export DXVK_LOG_LEVEL=error
export DXVK_CONFIG_FILE="$("$WINE" winepath -w ~/paintdotnet/app/dxvk.conf)"
export WINEDLLOVERRIDES="mshtml=;d3dcompiler_47=n"

"$WINE" ~/paintdotnet/app/paintdotnet.exe
```

### Optional Native Wine DLL Flags

Paint.NET's `/wine` mode uses managed fallbacks by default, which can be selectively switched back to Wine's patched native DLLs:

- **`/useNativeWAM`** and **`/enableComposition`**: Verified working with Wine's patched `uianimation.dll`, `coremessaging.dll`, and `dcomp.dll`.
- **Wine `d3dcompiler_47`**: Omitting `d3dcompiler_47=n` exercises Wine's patched `GetMinFeatureLevel` in `vkd3d`, though `d3dcompiler_47=n` is recommended if shader caching compatibility with Microsoft's compiler is desired.

### Known Limitations
- **DXVK required**: Running without DXVK (`WineD3D`) currently fails in `PaintDotNet.Controls.ColorRectangleControl` (`Unexpected null reference 'this.hwndDevice3D'`).
- **`/useNativeDWrite`**: Passing `/useNativeDWrite` (instead of the bundled `DWriteCore.dll`) currently fails in `DWriteFactory7.CreateGlyphRunAnalysis` (`E_INVALIDARG` for `TextRenderingMode = NaturalSymmetricDownsampled`). Leave `DWriteCore` enabled (default).
- **Fedora 44+**: If you see `Cannot get symbol u_charsToUChars from libicuuc`, export `DOTNET_SYSTEM_GLOBALIZATION_USENLS=1` before launching.
