from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text(encoding="utf-8")
readme = (root / "README.md").read_text(encoding="utf-8") if (root / "README.md").exists() else ""

cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
resource = (root / "src/QDOLEDTextTuner.rc").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build.yml").read_text(encoding="utf-8")

required_source = {
    "grayscale system profile": "FE_FONTSMOOTHINGSTANDARD",
    "ClearType profile": "FE_FONTSMOOTHINGCLEARTYPE",
    "RGB orientation": "FE_FONTSMOOTHINGORIENTATIONRGB",
    "BGR orientation": "FE_FONTSMOOTHINGORIENTATIONBGR",
    "settings broadcast": "WM_SETTINGCHANGE",
    "original settings backup": "SaveInitialStateOnce",
    "restore path": "Original setting restored",
    "startup reapply": "--apply-grayscale",
    "display settings shortcut": "ms-settings:display",
    "browser compatibility explanation": "Rendering controls",
    "ClearType contrast slider": "TBM_SETRANGE",
    "Chromium grayscale switch": "--disable-lcd-text",
    "Edge discovery": "FindEdge",
    "per-app shortcut creator": "CreateGrayscaleShortcut",
    "aliased profile": "kApplyAliased",
}
for label, token in required_source.items():
    assert token in source, f"missing {label}: {token}"

required_docs = [
    "triangular",
    "grayscale",
    "DirectWrite",
    "PG32UCDM3",
    "no CMake required",
    "QDOLEDTextTuner-win-x64",
    "Startup boost",
    "1000–2200",
    "disable-lcd-text",
    "per-application grayscale shortcuts",
    "No-antialiasing profile",
]
for token in required_docs:
    assert token.lower() in readme.lower(), f"README must document {token}"

assert "enable_language(RC)" in cmake, "CMake must enable the Windows resource compiler"
assert "src/QDOLEDTextTuner.rc" in cmake, "CMake must compile the Windows resource file"
assert 'RT_MANIFEST "QDOLEDTextTuner.manifest"' in resource, "resource must embed the app manifest"
assert "runs-on: windows-2022" in workflow, "workflow must use the stable Visual Studio 2022 runner"
assert "Verify Windows executable" in workflow, "workflow must validate its PE output"
assert "Upload Windows build diagnostics" in workflow, "workflow must retain diagnostics on failure"

print("source contract checks passed")
