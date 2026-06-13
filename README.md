# QD-OLED Text Tuner for Windows 11

A small, dependency-free Win32 utility for reducing colored text fringes on ASUS ROG Swift **PG32UCDM** and **PG32UCDM3** QD-OLED monitors.

## Why this monitor can show color fringing

The PG32UCDM3 is a 31.5-inch 3840×2160 240 Hz QD-OLED display. ASUS identifies it as a fourth-generation Tandem QD-OLED model, while close-up panel testing shows that its red, green, and blue subpixels remain arranged in a **triangle**, not a conventional vertical RGB stripe.

That distinction matters because ClearType gains apparent horizontal resolution by addressing red, green, and blue components independently. Its RGB/BGR choices describe a one-dimensional stripe order; neither choice models a triangle whose green component is vertically offset. At 140 PPI the artifact is much smaller than on earlier 1440p QD-OLEDs, but a sensitive viewer can still see magenta/green edges.

Sources:

- [Official ASUS PG32UCDM3 product page](https://rog.asus.com/us/monitors/27-to-31-5-inches/rog-swift-oled-pg32ucdm-gen3-pg32ucdm3/)
- [TFTCentral PG32UCDM3 review and microscope-based subpixel discussion](https://tftcentral.co.uk/reviews/asus-rog-swift-pg32ucdm3)
- [Microsoft: `SystemParametersInfo` font-smoothing settings](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow)
- [Microsoft: DirectWrite rendering modes](https://learn.microsoft.com/en-us/windows/win32/directwrite/dwrite-rendering-mode-enumerations)
- [Microsoft: grayscale text uses flat pixel geometry](https://learn.microsoft.com/en-us/windows/win32/directwrite/introducing-directwrite)
- [Microsoft: ClearType contrast is adjustable from 1000 to 2200](https://learn.microsoft.com/en-us/windows/win32/gdi/cleartype-antialiasing)
- [Chromium source: `--disable-lcd-text` disables LCD text](https://chromium.googlesource.com/chromium/src/+/master/content/public/common/content_switches.cc)
- [Chromium: command-line switches may change or be removed](https://www.chromium.org/developers/how-tos/run-chromium-with-flags/)

## What the tool does

The application now provides two separate layers rather than only toggling grayscale:

- **Windows grayscale profile:** avoids RGB subpixel assumptions and therefore colored ClearType fringes in applications that honor the Windows preference.
- **ClearType RGB and BGR profiles:** lets you compare both stripe assumptions instead of forcing one answer.
- **No-antialiasing profile:** disables Windows font smoothing for maximally hard, color-free pixel edges. It can look sharp at the cost of visibly jagged curves and is included as an experimental comparison rather than the default recommendation.
- **Adjustable ClearType contrast:** exposes Windows' supported 1000–2200 contrast range. Lower values can reduce the intensity of colored edges; higher values can make stems look darker and sharper. This setting applies only to ClearType.
- **One-click Edge grayscale launch:** starts Edge with Chromium's `--disable-lcd-text` switch, which requests that Chromium disable LCD/subpixel text.
- **Per-application grayscale shortcuts:** select the `.exe` for Chrome, Brave, VS Code, Discord, Slack, or another Chromium/Electron application and create a Desktop shortcut containing `--disable-lcd-text`.
- **Reversible changes:** saves the original Windows font-smoothing state the first time a profile is applied and provides a Restore button.
- **Sign-in reapply and diagnostics:** can reapply Windows grayscale at sign-in, shows GDI comparison samples, and opens display scaling or the built-in ClearType tuner.

The app changes only the current user's Windows font-smoothing preferences. It requires no administrator privileges.

## Does it work in Edge and every other app?

**No—not in every app.** The tool changes the current Windows user's font-smoothing preference; it does not inject code into applications or replace their text renderer.

| Application type | Expected result |
| --- | --- |
| Traditional GDI desktop applications | Usually honor the selected profile after reopening |
| Windows shell and older Win32 UI | Often honor it, but sign-out or restart may be required |
| Edge, Chrome, and Chromium/Electron apps | Use the tool's `--disable-lcd-text` launcher/shortcut; support remains application/version-dependent |
| Firefox, Office, IDEs, and modern DirectWrite apps | Application-dependent; many choose their own rendering parameters |
| Games and GPU-rendered interfaces | Usually unaffected |

For **Microsoft Edge**, completely exit Edge first, then use **Launch Edge with grayscale text**. Edge's Startup boost and background-extension settings can leave browser processes running; an already-running browser will usually reuse the existing process and ignore new command-line switches. The generic shortcut creator provides the same option for other Chromium/Electron applications.

Chromium's source currently defines `--disable-lcd-text` as disabling LCD text, but Chromium explicitly warns that command-line switches can change or disappear without notice. Firefox does not use Chromium switches. Browser chrome and web-page content may also use different rendering paths.

Microsoft's DirectWrite API allows applications to select application-specific gamma, enhanced contrast, ClearType level, pixel geometry, rendering mode, and antialiasing mode. Consequently, there is no safe universal registry value that converts every application to triangular-QD-OLED-aware rendering. The tuner combines all practical controls Windows exposes, but it cannot mathematically remap arbitrary third-party rendering to the panel's triangular geometry.

## Recommended setup order

1. Connect over DisplayPort or HDMI and use the panel's native **3840×2160** resolution.
2. Confirm Windows is using RGB output rather than chroma-subsampled YCbCr. In NVIDIA Control Panel, for example, use RGB and Full dynamic range. A blurry whole desktop can indicate 4:2:2 or 4:2:0 output rather than a font issue.
3. Start with **125%, 150%, or 175%** Windows scaling. Higher scaling makes glyph features span more physical pixels; choose the lowest value that does not expose distracting fringes at your normal distance.
4. Set the monitor's **VividPixel to 0** initially. Added edge sharpening can create halos and does not correct subpixel geometry.
5. Start with **grayscale**. If it looks too soft, compare low-contrast ClearType (try 1000–1200) and **No antialiasing**. ClearType may be sharper but can reintroduce color; no antialiasing is color-free but jagged.
6. For Edge or another Chromium/Electron app, fully exit it and use the dedicated launcher or generated grayscale shortcut.
7. Compare in both light and dark themes. QD-OLED fringing can be more obvious on particular foreground/background combinations.

If the entire desktop—not only text—looks soft, check native resolution, GPU scaling, chroma format, and monitor sharpening before tuning fonts.

## Download the executable — no CMake required

Normal users do **not** need CMake, Visual Studio, or a compiler. GitHub builds the Windows executable automatically.

### Download a build from GitHub Actions

1. Open this repository's **Actions** tab.
2. Open the newest successful **Build Windows executable** run.
3. In the run's **Artifacts** section, download `QDOLEDTextTuner-win-x64`.
4. Unzip the artifact download and open its `QDOLEDTextTuner-win-x64` folder. The inner portable ZIP is also included if you want to keep or share it.
5. Run `QDOLEDTextTuner.exe`. Windows SmartScreen may show an unrecognized-app warning because the executable is not code-signed. Review the source and workflow before choosing **More info → Run anyway**.

Artifacts are retained for 30 days. A maintainer can also select **Run workflow** on the Actions page to make a fresh build without changing any code.

### Download a release

When a maintainer pushes a version tag such as `v1.0.0`, the same workflow creates a GitHub Release and attaches the portable ZIP plus its SHA-256 checksum. Releases do not expire like Actions artifacts.

### Build it yourself (developer option)

CMake is only needed if you want to compile the source yourself. From a Visual Studio 2022 Developer Command Prompt:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is written to `build/Release/QDOLEDTextTuner.exe`.

## Command line

```text
QDOLEDTextTuner.exe --apply-grayscale
```

This applies the recommended profile without opening a window. The optional sign-in entry uses this mode.

## Privacy and safety

The application is offline, has no telemetry, does not install a service or driver, and writes only to the current user's font-smoothing and Startup preferences. The original smoothing state is stored under `HKCU\Software\QDOLEDTextTuner`.
