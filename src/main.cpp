#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace {
constexpr wchar_t kClassName[] = L"QDOLEDTextTunerWindow";
constexpr wchar_t kAppName[] = L"QD-OLED Text Tuner";
constexpr wchar_t kSettingsKey[] = L"Software\\QDOLEDTextTuner";
constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"QDOLEDTextTuner";
constexpr UINT kApplyGrayscale = 101;
constexpr UINT kApplyRgb = 102;
constexpr UINT kApplyBgr = 103;
constexpr UINT kRestore = 104;
constexpr UINT kStartup = 105;
constexpr UINT kDisplaySettings = 106;
constexpr UINT kClearType = 107;
constexpr UINT kPreview = 108;
constexpr UINT kStatus = 109;
constexpr UINT kBrowserHelp = 110;
constexpr UINT kContrastSlider = 111;
constexpr UINT kContrastLabel = 112;
constexpr UINT kLaunchEdgeGray = 113;
constexpr UINT kCreateAppShortcut = 114;
constexpr UINT kApplyAliased = 115;

struct SmoothingState {
  BOOL enabled = TRUE;
  UINT type = FE_FONTSMOOTHINGSTANDARD;
  UINT orientation = FE_FONTSMOOTHINGORIENTATIONRGB;
  UINT contrast = 1400;
};

HINSTANCE g_instance{};
HFONT g_uiFont{};
HFONT g_headingFont{};
HFONT g_previewGray{};
HFONT g_previewClearType{};
HWND g_status{};
HWND g_startup{};
HWND g_contrastSlider{};
HWND g_contrastLabel{};

std::wstring ErrorMessage(DWORD error) {
  wchar_t *text = nullptr;
  const DWORD size = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, 0, reinterpret_cast<wchar_t *>(&text), 0, nullptr);
  std::wstring result =
      size && text ? std::wstring(text, size) : L"Unknown Windows error";
  if (text)
    LocalFree(text);
  while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n'))
    result.pop_back();
  return result;
}

bool WriteDword(HKEY root, const wchar_t *key, const wchar_t *name,
                DWORD value) {
  HKEY handle{};
  if (RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle,
                      nullptr) != ERROR_SUCCESS)
    return false;
  const LONG result =
      RegSetValueExW(handle, name, 0, REG_DWORD,
                     reinterpret_cast<const BYTE *>(&value), sizeof(value));
  RegCloseKey(handle);
  return result == ERROR_SUCCESS;
}

std::optional<DWORD> ReadDword(HKEY root, const wchar_t *key,
                               const wchar_t *name) {
  DWORD value{}, size = sizeof(value), type{};
  if (RegGetValueW(root, key, name, RRF_RT_REG_DWORD, &type, &value, &size) !=
      ERROR_SUCCESS)
    return std::nullopt;
  return value;
}

SmoothingState ReadCurrentState() {
  SmoothingState state;
  SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &state.enabled, 0);
  SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &state.type, 0);
  SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &state.orientation,
                        0);
  SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &state.contrast, 0);
  return state;
}

void SaveInitialStateOnce() {
  if (ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"BackupCreated"))
    return;
  const auto state = ReadCurrentState();
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"Enabled",
             static_cast<DWORD>(state.enabled));
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"Type", state.type);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"Orientation",
             state.orientation);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"Contrast", state.contrast);
  WriteDword(HKEY_CURRENT_USER, kSettingsKey, L"BackupCreated", 1);
}

bool ApplyState(const SmoothingState &state) {
  constexpr UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
  BOOL enabled = state.enabled;
  bool ok = SystemParametersInfoW(SPI_SETFONTSMOOTHING, enabled, nullptr,
                                  flags) != FALSE;
  if (enabled) {
    ok &= SystemParametersInfoW(
              SPI_SETFONTSMOOTHINGTYPE, 0,
              reinterpret_cast<void *>(static_cast<UINT_PTR>(state.type)),
              flags) != FALSE;
    if (state.type == FE_FONTSMOOTHINGCLEARTYPE) {
      ok &= SystemParametersInfoW(SPI_SETFONTSMOOTHINGORIENTATION, 0,
                                  reinterpret_cast<void *>(
                                      static_cast<UINT_PTR>(state.orientation)),
                                  flags) != FALSE;
      ok &= SystemParametersInfoW(
                SPI_SETFONTSMOOTHINGCONTRAST, 0,
                reinterpret_cast<void *>(static_cast<UINT_PTR>(state.contrast)),
                flags) != FALSE;
    }
  }
  SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                      reinterpret_cast<LPARAM>(L"Control Panel\\Desktop"),
                      SMTO_ABORTIFHUNG, 1000, nullptr);
  return ok;
}

std::wstring StateDescription(const SmoothingState &state) {
  if (!state.enabled)
    return L"Current Windows setting: font smoothing off";
  if (state.type == FE_FONTSMOOTHINGSTANDARD)
    return L"Current Windows setting: grayscale / standard smoothing";
  const std::wstring orientation =
      state.orientation == FE_FONTSMOOTHINGORIENTATIONBGR ? L"ClearType BGR"
                                                          : L"ClearType RGB";
  return L"Current Windows setting: " + orientation + L", contrast " +
         std::to_wstring(state.contrast);
}

void SetStatus(std::wstring_view prefix) {
  std::wstring text(prefix);
  if (!text.empty())
    text += L"  •  ";
  text += StateDescription(ReadCurrentState());
  SetWindowTextW(g_status, text.c_str());
}

bool StartupEnabled() {
  wchar_t value[MAX_PATH * 2]{};
  DWORD size = sizeof(value);
  return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ,
                      nullptr, value, &size) == ERROR_SUCCESS;
}

bool SetStartup(bool enabled) {
  HKEY key{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
                      nullptr, &key, nullptr) != ERROR_SUCCESS)
    return false;
  LONG result{};
  if (enabled) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(path) + L"\" --apply-grayscale";
    result = RegSetValueExW(
        key, kRunValue, 0, REG_SZ,
        reinterpret_cast<const BYTE *>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    result = RegDeleteValueW(key, kRunValue);
    if (result == ERROR_FILE_NOT_FOUND)
      result = ERROR_SUCCESS;
  }
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

std::optional<std::wstring> ExpandExistingPath(const wchar_t *value) {
  wchar_t expanded[MAX_PATH]{};
  if (!ExpandEnvironmentStringsW(value, expanded, MAX_PATH))
    return std::nullopt;
  if (GetFileAttributesW(expanded) == INVALID_FILE_ATTRIBUTES)
    return std::nullopt;
  return std::wstring(expanded);
}

std::optional<std::wstring> FindEdge() {
  constexpr std::array<const wchar_t *, 3> candidates{
      L"%ProgramFiles(x86)%\\Microsoft\\Edge\\Application\\msedge.exe",
      L"%ProgramFiles%\\Microsoft\\Edge\\Application\\msedge.exe",
      L"%LOCALAPPDATA%\\Microsoft\\Edge\\Application\\msedge.exe",
  };
  for (const auto *candidate : candidates) {
    if (auto path = ExpandExistingPath(candidate))
      return path;
  }
  return std::nullopt;
}

bool CreateGrayscaleShortcut(const std::wstring &executable,
                             std::wstring &shortcutPath) {
  wchar_t desktop[MAX_PATH]{};
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                              SHGFP_TYPE_CURRENT, desktop)))
    return false;

  const std::filesystem::path exePath(executable);
  shortcutPath = (std::filesystem::path(desktop) /
                  (exePath.stem().wstring() + L" (grayscale).lnk"))
                     .wstring();

  IShellLinkW *link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IShellLinkW,
                              reinterpret_cast<void **>(&link))))
    return false;
  link->SetPath(executable.c_str());
  link->SetArguments(L"--disable-lcd-text");
  link->SetWorkingDirectory(exePath.parent_path().c_str());
  link->SetDescription(L"Launch with Chromium LCD/subpixel text disabled");

  IPersistFile *persist = nullptr;
  HRESULT result = link->QueryInterface(IID_IPersistFile,
                                        reinterpret_cast<void **>(&persist));
  if (SUCCEEDED(result)) {
    result = persist->Save(shortcutPath.c_str(), TRUE);
    persist->Release();
  }
  link->Release();
  return SUCCEEDED(result);
}

void ChooseAndCreateShortcut(HWND owner) {
  wchar_t executable[MAX_PATH]{};
  OPENFILENAMEW dialog{sizeof(dialog)};
  dialog.hwndOwner = owner;
  dialog.lpstrFilter = L"Applications (*.exe)\0*.exe\0All files (*.*)\0*.*\0";
  dialog.lpstrFile = executable;
  dialog.nMaxFile = MAX_PATH;
  dialog.lpstrTitle = L"Choose a Chromium or Electron application";
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&dialog))
    return;

  std::wstring shortcut;
  if (CreateGrayscaleShortcut(executable, shortcut)) {
    const std::wstring message =
        L"Created:\n" + shortcut +
        L"\n\nFully exit the application before using this shortcut. The "
        L"switch is Chromium-specific and may be changed or ignored by an "
        L"application.";
    MessageBoxW(owner, message.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
  } else {
    MessageBoxW(owner, L"Windows could not create the desktop shortcut.",
                kAppName, MB_OK | MB_ICONERROR);
  }
}

UINT SelectedContrast() {
  return g_contrastSlider ? static_cast<UINT>(SendMessageW(g_contrastSlider,
                                                           TBM_GETPOS, 0, 0))
                          : 1400;
}

void UpdateContrastLabel() {
  if (!g_contrastLabel)
    return;
  const std::wstring text =
      L"ClearType contrast: " + std::to_wstring(SelectedContrast()) +
      L"  (lower can reduce colored edge intensity; affects ClearType only)";
  SetWindowTextW(g_contrastLabel, text.c_str());
}

std::wstring DetectDisplay() {
  DISPLAY_DEVICEW adapter{sizeof(adapter)};
  for (DWORD adapterIndex = 0;
       EnumDisplayDevicesW(nullptr, adapterIndex, &adapter, 0);
       ++adapterIndex) {
    if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;
    DISPLAY_DEVICEW monitor{sizeof(monitor)};
    for (DWORD monitorIndex = 0;
         EnumDisplayDevicesW(adapter.DeviceName, monitorIndex, &monitor, 0);
         ++monitorIndex) {
      std::wstring name = monitor.DeviceString;
      std::wstring upper = name;
      CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));
      if (upper.find(L"PG32UCDM") != std::wstring::npos)
        return L"Detected: " + name;
    }
  }
  return L"Display auto-detection did not find “PG32UCDM”; profiles can still "
         L"be used.";
}

HWND AddControl(HWND parent, const wchar_t *klass, const wchar_t *text,
                DWORD style, int x, int y, int width, int height, UINT id = 0) {
  HWND control = CreateWindowExW(
      0, klass, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height,
      parent, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), g_instance,
      nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
  return control;
}

void DrawPreview(HDC dc, RECT area, bool grayscale) {
  const COLORREF background = RGB(247, 248, 250);
  HBRUSH brush = CreateSolidBrush(background);
  FillRect(dc, &area, brush);
  DeleteObject(brush);
  FrameRect(dc, &area, reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH)));
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(24, 27, 31));
  SelectObject(dc, grayscale ? g_previewGray : g_previewClearType);
  RECT title = area;
  title.left += 14;
  title.top += 10;
  DrawTextW(dc, grayscale ? L"Grayscale preview" : L"ClearType preview", -1,
            &title, DT_TOP | DT_SINGLELINE);
  SelectObject(dc, grayscale ? g_previewGray : g_previewClearType);
  RECT sample = area;
  sample.left += 14;
  sample.right -= 10;
  sample.top += 42;
  DrawTextW(
      dc,
      L"The quick brown fox jumps over 0123456789\nIl1  O0  rn/m  RGB edges",
      -1, &sample, DT_TOP | DT_LEFT | DT_WORDBREAK);
}

void CreateFonts(UINT dpi) {
  const int uiHeight = -MulDiv(10, dpi, 72);
  const int headingHeight = -MulDiv(16, dpi, 72);
  const int previewHeight = -MulDiv(12, dpi, 72);
  g_uiFont =
      CreateFontW(uiHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g_headingFont = CreateFontW(headingHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g_previewGray = CreateFontW(previewHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  g_previewClearType = CreateFontW(
      previewHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  switch (message) {
  case WM_CREATE: {
    const UINT dpi = GetDpiForWindow(window);
    CreateFonts(dpi);
    HWND heading =
        AddControl(window, L"STATIC", kAppName, SS_LEFT, 24, 18, 650, 34);
    SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(g_headingFont),
                 TRUE);
    AddControl(window, L"STATIC",
               L"PG32UCDM / PG32UCDM3 panels use triangular RGB QD-OLED "
               L"subpixels. Windows ClearType expects a stripe, so grayscale "
               L"is the recommended no-color-fringe profile.",
               SS_LEFT, 24, 58, 730, 44);
    AddControl(window, L"STATIC", DetectDisplay().c_str(), SS_LEFT, 24, 106,
               730, 22);

    AddControl(window, L"BUTTON", L"Grayscale", BS_PUSHBUTTON, 24, 142, 140, 36,
               kApplyGrayscale);
    AddControl(window, L"BUTTON", L"ClearType RGB", BS_PUSHBUTTON, 172, 142,
               140, 36, kApplyRgb);
    AddControl(window, L"BUTTON", L"ClearType BGR", BS_PUSHBUTTON, 320, 142,
               140, 36, kApplyBgr);
    AddControl(window, L"BUTTON", L"No antialiasing", BS_PUSHBUTTON, 468, 142,
               140, 36, kApplyAliased);
    AddControl(window, L"BUTTON", L"Restore original", BS_PUSHBUTTON, 616, 142,
               138, 36, kRestore);

    g_contrastLabel = AddControl(window, L"STATIC", L"", SS_LEFT, 24, 194, 650,
                                 22, kContrastLabel);
    g_contrastSlider =
        AddControl(window, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_HORZ, 24,
                   216, 730, 34, kContrastSlider);
    SendMessageW(g_contrastSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1000, 2200));
    SendMessageW(g_contrastSlider, TBM_SETTICFREQ, 200, 0);
    SendMessageW(g_contrastSlider, TBM_SETPOS, TRUE,
                 ReadCurrentState().contrast);
    UpdateContrastLabel();

    AddControl(window, L"STATIC",
               L"GDI preview (browser and DirectWrite rendering can differ)",
               SS_LEFT, 24, 260, 500, 22);
    AddControl(window, L"STATIC", L"", SS_OWNERDRAW, 24, 286, 350, 116,
               kPreview);
    AddControl(window, L"STATIC", L"", SS_OWNERDRAW, 404, 286, 350, 116,
               kPreview + 1);

    AddControl(window, L"STATIC", L"Chromium / Electron per-app tools", SS_LEFT,
               24, 418, 300, 22);
    AddControl(window, L"BUTTON", L"Launch Edge with grayscale text",
               BS_PUSHBUTTON, 24, 444, 238, 36, kLaunchEdgeGray);
    AddControl(window, L"BUTTON", L"Create grayscale app shortcut…",
               BS_PUSHBUTTON, 274, 444, 238, 36, kCreateAppShortcut);
    AddControl(window, L"BUTTON", L"How does this work?", BS_PUSHBUTTON, 524,
               444, 230, 36, kBrowserHelp);

    g_startup = AddControl(window, L"BUTTON",
                           L"Reapply Windows grayscale when I sign in",
                           BS_AUTOCHECKBOX, 24, 500, 340, 28, kStartup);
    Button_SetCheck(g_startup, StartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
    AddControl(window, L"BUTTON", L"Display scaling", BS_PUSHBUTTON, 404, 496,
               168, 34, kDisplaySettings);
    AddControl(window, L"BUTTON", L"ClearType tuner", BS_PUSHBUTTON, 584, 496,
               170, 34, kClearType);

    AddControl(window, L"STATIC",
               L"The system profile covers compatible Windows/GDI apps. The "
               L"Chromium shortcut adds --disable-lcd-text for Edge, Chrome, "
               L"Brave, VS Code, and similar apps; fully close them first.",
               SS_LEFT, 24, 548, 730, 42);
    g_status =
        AddControl(window, L"STATIC", L"", SS_LEFT, 24, 606, 730, 26, kStatus);
    SetStatus(L"Ready");
    return 0;
  }
  case WM_DRAWITEM: {
    const auto *item = reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
    if (item->CtlID == kPreview || item->CtlID == kPreview + 1) {
      DrawPreview(item->hDC, item->rcItem, item->CtlID == kPreview);
      return TRUE;
    }
    break;
  }
  case WM_HSCROLL:
    if (reinterpret_cast<HWND>(lparam) == g_contrastSlider)
      UpdateContrastLabel();
    return 0;
  case WM_COMMAND: {
    const UINT id = LOWORD(wparam);
    if (id == kApplyGrayscale || id == kApplyRgb || id == kApplyBgr ||
        id == kApplyAliased) {
      SaveInitialStateOnce();
      SmoothingState state;
      state.enabled = id != kApplyAliased;
      state.type = id == kApplyGrayscale ? FE_FONTSMOOTHINGSTANDARD
                                         : FE_FONTSMOOTHINGCLEARTYPE;
      state.orientation = id == kApplyBgr ? FE_FONTSMOOTHINGORIENTATIONBGR
                                          : FE_FONTSMOOTHINGORIENTATIONRGB;
      state.contrast = SelectedContrast();
      SetStatus(ApplyState(state) ? L"Applied. Restart open apps if needed."
                                  : L"Could not apply every setting.");
      InvalidateRect(window, nullptr, TRUE);
    } else if (id == kRestore) {
      SmoothingState state;
      const auto enabled =
          ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"Enabled");
      const auto type = ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"Type");
      const auto orientation =
          ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"Orientation");
      const auto contrast =
          ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"Contrast");
      if (!enabled || !type || !orientation || !contrast) {
        MessageBoxW(window, L"No original setting has been saved yet.",
                    kAppName, MB_OK | MB_ICONINFORMATION);
      } else {
        state.enabled = static_cast<BOOL>(*enabled);
        state.type = *type;
        state.orientation = *orientation;
        state.contrast = *contrast;
        SendMessageW(g_contrastSlider, TBM_SETPOS, TRUE, state.contrast);
        UpdateContrastLabel();
        SetStatus(ApplyState(state) ? L"Original setting restored."
                                    : L"Restore failed.");
      }
    } else if (id == kStartup) {
      const bool enable = Button_GetCheck(g_startup) == BST_CHECKED;
      if (!SetStartup(enable)) {
        Button_SetCheck(g_startup, enable ? BST_UNCHECKED : BST_CHECKED);
        MessageBoxW(window, ErrorMessage(GetLastError()).c_str(), kAppName,
                    MB_OK | MB_ICONERROR);
      }
    } else if (id == kDisplaySettings) {
      ShellExecuteW(window, L"open", L"ms-settings:display", nullptr, nullptr,
                    SW_SHOWNORMAL);
    } else if (id == kClearType) {
      ShellExecuteW(window, L"open", L"cttune.exe", nullptr, nullptr,
                    SW_SHOWNORMAL);
    } else if (id == kLaunchEdgeGray) {
      if (const auto edge = FindEdge()) {
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(window, L"open", edge->c_str(), L"--disable-lcd-text",
                          nullptr, SW_SHOWNORMAL));
        if (result <= 32)
          MessageBoxW(window, L"Edge could not be launched.", kAppName,
                      MB_OK | MB_ICONERROR);
        else
          SetStatus(L"Launched Edge with --disable-lcd-text. Existing Edge "
                    L"processes must be closed first.");
      } else {
        MessageBoxW(
            window,
            L"Microsoft Edge was not found in its standard install locations.",
            kAppName, MB_OK | MB_ICONWARNING);
      }
    } else if (id == kCreateAppShortcut) {
      ChooseAndCreateShortcut(window);
    } else if (id == kBrowserHelp) {
      MessageBoxW(
          window,
          L"This tool now has two independent layers:\n\n"
          L"1. Windows profiles: grayscale, RGB/BGR ClearType, no "
          L"antialiasing, and adjustable ClearType contrast (1000–2200). These "
          L"affect apps that honor the user setting.\n\n"
          L"2. Chromium/Electron launch override: --disable-lcd-text asks "
          L"Chromium to turn off LCD/subpixel text. It can help Edge, Chrome, "
          L"Brave, VS Code, and similar apps. Fully exit every background "
          L"process first.\n\n"
          L"The Chromium switch is not a Windows guarantee: an app can ignore "
          L"or remove it, and Firefox/non-Chromium apps do not use it.",
          L"Rendering controls", MB_OK | MB_ICONINFORMATION);
    }
    return 0;
  }
  case WM_DESTROY:
    DeleteObject(g_uiFont);
    DeleteObject(g_headingFont);
    DeleteObject(g_previewGray);
    DeleteObject(g_previewClearType);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t *commandLine,
                      int showCommand) {
  g_instance = instance;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&controls);
  const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  if (commandLine &&
      std::wstring_view(commandLine).find(L"--apply-grayscale") !=
          std::wstring_view::npos) {
    SmoothingState state;
    state.type = FE_FONTSMOOTHINGSTANDARD;
    const int result = ApplyState(state) ? 0 : 1;
    if (SUCCEEDED(comResult))
      CoUninitialize();
    return result;
  }

  WNDCLASSEXW windowClass{sizeof(windowClass)};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  windowClass.lpszClassName = kClassName;
  if (!RegisterClassExW(&windowClass))
    return 1;

  const HWND window = CreateWindowExW(
      0, kClassName, kAppName,
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, 800, 690, nullptr, nullptr, instance, nullptr);
  if (!window)
    return 1;
  BOOL darkCaption = TRUE;
  DwmSetWindowAttribute(window, 20, &darkCaption, sizeof(darkCaption));
  ShowWindow(window, showCommand);
  UpdateWindow(window);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  if (SUCCEEDED(comResult))
    CoUninitialize();
  return static_cast<int>(message.wParam);
}
