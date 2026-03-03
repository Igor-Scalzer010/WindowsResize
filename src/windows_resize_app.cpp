#include "windows_resize_app.hpp"
#include "resource.hpp"

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <utility>


// anonymous constexprs and helper functions 
namespace {
constexpr UINT_PTR kTickTimerId = 1;
constexpr UINT kTickIntervalMs = 8;
constexpr UINT kTrayCallbackMessage = WM_APP + 1;

constexpr int kHotkeyMove = 1;
constexpr int kHotkeyResize = 2;
constexpr int kHotkeyQuit = 3;

constexpr UINT_PTR kTrayIconId = 1;
constexpr UINT_PTR kTrayMenuExitId = 1001;
constexpr wchar_t kWindowClassName[] = L"WindowsResizeMessageWindow";

[[nodiscard]] bool IsPressed(const int virtual_key) {
    return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}
}  // final namespace

struct WindowsResizeApp::InteractionState {
    InteractionMode mode{};
    HWND target_window{};
    POINT cursor_start{};
    RECT window_start{};
    bool resize_from_left{};
    bool resize_from_top{};
};

class WindowsResizeApp::ScopedHotkey final {
public:
    ScopedHotkey(HWND window, int id, UINT modifiers, UINT virtual_key) : window_(window), id_(id) {
        registered_ = RegisterHotKey(window_, id_, modifiers, virtual_key) != 0;
    }

    ~ScopedHotkey() {
        if (registered_) {
            UnregisterHotKey(window_, id_);
        }
    }

    ScopedHotkey(const ScopedHotkey&) = delete;
    ScopedHotkey& operator=(const ScopedHotkey&) = delete;

    ScopedHotkey(ScopedHotkey&& other) noexcept
        : window_(std::exchange(other.window_, nullptr)),
          id_(std::exchange(other.id_, 0)),
          registered_(std::exchange(other.registered_, false)) {}

    ScopedHotkey& operator=(ScopedHotkey&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (registered_) {
            UnregisterHotKey(window_, id_);
        }

        window_ = std::exchange(other.window_, nullptr);
        id_ = std::exchange(other.id_, 0);
        registered_ = std::exchange(other.registered_, false);
        return *this;
    }

    [[nodiscard]] bool IsRegistered() const noexcept {
        return registered_;
    }

private:
    HWND window_{};
    int id_{};
    bool registered_{};
};

class WindowsResizeApp::TrayIcon final {
public:
    explicit TrayIcon(HWND owner_window) : owner_window_(owner_window) {}

    ~TrayIcon() {
        Remove();
    }

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    TrayIcon(TrayIcon&&) = delete;
    TrayIcon& operator=(TrayIcon&&) = delete;

    [[nodiscard]] bool Initialize() {
        data_ = {};
        data_.cbSize = sizeof(data_);
        data_.hWnd = owner_window_;
        data_.uID = kTrayIconId;
        data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data_.uCallbackMessage = kTrayCallbackMessage;
        data_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
        wcscpy_s(data_.szTip, L"WindowsResize em execucao");

        if (Shell_NotifyIconW(NIM_ADD, &data_) == 0) {
            return false;
        }

        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
        added_ = true;
        return true;
    }

    void RestoreAfterTaskbarRestart() {
        added_ = false;
        static_cast<void>(Initialize());
    }

    void Remove() noexcept {
        if (!added_) {
            return;
        }

        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }

    void ShowContextMenu() const {
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) {
            return;
        }

        AppendMenuW(menu, MF_STRING, kTrayMenuExitId, L"Exit");

        POINT cursor{};
        if (GetCursorPos(&cursor) != 0) {
            SetForegroundWindow(owner_window_);
            TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, owner_window_, nullptr);
            PostMessageW(owner_window_, WM_NULL, 0, 0);
        }

        DestroyMenu(menu);
    }

private:
    HWND owner_window_{};
    NOTIFYICONDATAW data_{};
    bool added_{};
};

WindowsResizeApp::WindowsResizeApp(HINSTANCE instance) : instance_(instance) {}

WindowsResizeApp::~WindowsResizeApp() {
    if (message_window_ != nullptr) {
        DestroyWindow(message_window_);
    }
}

int WindowsResizeApp::Run() {
    if (!RegisterWindowClass()) {
        return 1;
    }

    if (!CreateHiddenWindow()) {
        return 1;
    }

    if (!InitializeHotkeys()) {
        return 1;
    }

    if (!InitializeTrayIcon()) {
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

bool WindowsResizeApp::RegisterWindowClass() const {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WindowsResizeApp::WindowProcRouter;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;

    return RegisterClassExW(&wc) != 0;
}

bool WindowsResizeApp::CreateHiddenWindow() {
    message_window_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"WindowsResize",
        0,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);

    taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
    return message_window_ != nullptr;
}

bool WindowsResizeApp::InitializeHotkeys() {
    struct HotkeyConfig {
        int id;
        UINT modifiers;
        UINT virtual_key;
    };

    constexpr std::array<HotkeyConfig, 3> kHotkeys{{
        {kHotkeyMove, MOD_ALT | MOD_NOREPEAT, '1'},
        {kHotkeyResize, MOD_ALT | MOD_NOREPEAT, '2'},
        {kHotkeyQuit, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'Q'},
    }};

    std::vector<std::unique_ptr<ScopedHotkey>> initialized_hotkeys;
    initialized_hotkeys.reserve(kHotkeys.size());
    for (const auto& hotkey : kHotkeys) {
        auto registered_hotkey =
            std::make_unique<ScopedHotkey>(message_window_, hotkey.id, hotkey.modifiers, hotkey.virtual_key);
        if (!registered_hotkey->IsRegistered()) {
            return false;
        }
        initialized_hotkeys.push_back(std::move(registered_hotkey));
    }

    hotkeys_ = std::move(initialized_hotkeys);
    return true;
}

bool WindowsResizeApp::InitializeTrayIcon() {
    tray_icon_ = std::make_unique<TrayIcon>(message_window_);
    return tray_icon_->Initialize();
}

LRESULT CALLBACK WindowsResizeApp::WindowProcRouter(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<WindowsResizeApp*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<WindowsResizeApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->WindowProc(hwnd, msg, wparam, lparam);
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT WindowsResizeApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == taskbar_created_message_ && tray_icon_) {
        tray_icon_->RestoreAfterTaskbarRestart();
        return 0;
    }

    switch (msg) {
        case WM_HOTKEY:
            return OnHotkey(static_cast<int>(wparam));
        case WM_COMMAND:
            if (LOWORD(wparam) == kTrayMenuExitId) {
                DestroyWindow(message_window_);
                return 0;
            }
            return 0;
        case WM_TIMER:
            if (wparam == kTickTimerId) {
                OnTick();
            }
            return 0;
        case kTrayCallbackMessage:
            return OnTrayEvent(lparam);
        case WM_DESTROY:
            StopInteraction();
            hotkeys_.clear();
            tray_icon_.reset();
            message_window_ = nullptr;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

LRESULT WindowsResizeApp::OnHotkey(const int hotkey_id) {
    switch (hotkey_id) {
        case kHotkeyMove:
            HandleInteractionHotkey(InteractionMode::Move);
            return 0;
        case kHotkeyResize:
            HandleInteractionHotkey(InteractionMode::Resize);
            return 0;
        case kHotkeyQuit:
            DestroyWindow(message_window_);
            return 0;
        default:
            return 0;
    }
}

LRESULT WindowsResizeApp::OnTrayEvent(const LPARAM lparam) {
    const UINT tray_event = LOWORD(static_cast<DWORD>(lparam));
    switch (tray_event) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            if (tray_icon_) {
                tray_icon_->ShowContextMenu();
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            MessageBeep(MB_OK);
            return 0;
        default:
            return 0;
    }
}

void WindowsResizeApp::HandleInteractionHotkey(InteractionMode mode) {
    if (interaction_state_ && interaction_state_->mode == mode) {
        StopInteraction();
        return;
    }

    StartInteraction(mode);
}

bool WindowsResizeApp::IsValidTargetWindow(HWND hwnd) const {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return false;
    }

    if (hwnd == message_window_) {
        return false;
    }

    return IsWindowVisible(hwnd) != 0;
}

void WindowsResizeApp::StartInteraction(InteractionMode mode) {
    HWND target = GetForegroundWindow();
    if (!IsValidTargetWindow(target)) {
        return;
    }

    if (IsIconic(target) || IsZoomed(target)) {
        ShowWindow(target, SW_RESTORE);
    }

    RECT rect{};
    POINT cursor{};
    if (GetWindowRect(target, &rect) == 0 || GetCursorPos(&cursor) == 0) {
        return;
    }

    auto state = std::make_unique<InteractionState>();
    state->mode = mode;
    state->target_window = target;
    state->cursor_start = cursor;
    state->window_start = rect;

    if (mode == InteractionMode::Resize) {
        const LONG center_x = rect.left + (rect.right - rect.left) / 2;
        const LONG center_y = rect.top + (rect.bottom - rect.top) / 2;
        state->resize_from_left = cursor.x < center_x;
        state->resize_from_top = cursor.y < center_y;
    }

    interaction_state_ = std::move(state);
    SetTimer(message_window_, kTickTimerId, kTickIntervalMs, nullptr);
}

void WindowsResizeApp::StopInteraction() noexcept {
    if (interaction_state_) {
        KillTimer(message_window_, kTickTimerId);
        interaction_state_.reset();
    }
}

void WindowsResizeApp::OnTick() {
    if (!interaction_state_) {
        return;
    }

    auto& state = *interaction_state_;
    if (!IsValidTargetWindow(state.target_window)) {
        StopInteraction();
        return;
    }

    if (IsCancelRequested()) {
        StopInteraction();
        return;
    }

    POINT cursor{};
    if (GetCursorPos(&cursor) == 0) {
        StopInteraction();
        return;
    }

    if (state.mode == InteractionMode::Move) {
        ApplyMove(state, cursor);
        return;
    }

    ApplyResize(state, cursor);
}

bool WindowsResizeApp::IsCancelRequested() const {
    return IsPressed(VK_ESCAPE);
}

void WindowsResizeApp::ApplyMove(const InteractionState& state, const POINT& cursor) const {
    const LONG dx = cursor.x - state.cursor_start.x;
    const LONG dy = cursor.y - state.cursor_start.y;

    const LONG new_left = state.window_start.left + dx;
    const LONG new_top = state.window_start.top + dy;

    SetWindowPos(state.target_window, nullptr, new_left, new_top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowsResizeApp::ApplyResize(const InteractionState& state, const POINT& cursor) const {
    LONG left = state.window_start.left;
    LONG top = state.window_start.top;
    LONG right = state.window_start.right;
    LONG bottom = state.window_start.bottom;

    const LONG dx = cursor.x - state.cursor_start.x;
    const LONG dy = cursor.y - state.cursor_start.y;

    if (state.resize_from_left) {
        left += dx;
    } else {
        right += dx;
    }

    if (state.resize_from_top) {
        top += dy;
    } else {
        bottom += dy;
    }

    const LONG min_width = (std::max)(120L, static_cast<LONG>(GetSystemMetrics(SM_CXMINTRACK)));
    const LONG min_height = (std::max)(80L, static_cast<LONG>(GetSystemMetrics(SM_CYMINTRACK)));

    if ((right - left) < min_width) {
        if (state.resize_from_left) {
            left = right - min_width;
        } else {
            right = left + min_width;
        }
    }

    if ((bottom - top) < min_height) {
        if (state.resize_from_top) {
            top = bottom - min_height;
        } else {
            bottom = top + min_height;
        }
    }

    SetWindowPos(state.target_window, nullptr, left, top, right - left, bottom - top, SWP_NOZORDER | SWP_NOACTIVATE);
}
