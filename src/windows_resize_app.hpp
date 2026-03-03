#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstdint>
#include <memory>
#include <vector>

class WindowsResizeApp final {
public:
    enum class InteractionMode : std::uint8_t {
        Move,
        Resize
    };

    explicit WindowsResizeApp(HINSTANCE instance);
    ~WindowsResizeApp();

    WindowsResizeApp(const WindowsResizeApp&) = delete;
    WindowsResizeApp& operator=(const WindowsResizeApp&) = delete;

    WindowsResizeApp(WindowsResizeApp&&) = delete;
    WindowsResizeApp& operator=(WindowsResizeApp&&) = delete;

    int Run();

private:
    struct InteractionState;
    class ScopedHotkey;
    class TrayIcon;

    static LRESULT CALLBACK WindowProcRouter(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    bool RegisterWindowClass() const;
    bool CreateHiddenWindow();
    bool InitializeHotkeys();
    bool InitializeTrayIcon();

    LRESULT OnHotkey(int hotkey_id);
    LRESULT OnTrayEvent(LPARAM lparam);

    void HandleInteractionHotkey(InteractionMode mode);
    void StartInteraction(InteractionMode mode);
    void StopInteraction() noexcept;
    void OnTick();
    bool IsCancelRequested() const;

    bool IsValidTargetWindow(HWND hwnd) const;
    void ApplyMove(const InteractionState& state, const POINT& cursor) const;
    void ApplyResize(const InteractionState& state, const POINT& cursor) const;

    HINSTANCE instance_{};
    HWND message_window_{};
    UINT taskbar_created_message_{};
    std::unique_ptr<TrayIcon> tray_icon_{};
    std::vector<std::unique_ptr<ScopedHotkey>> hotkeys_{};
    std::unique_ptr<InteractionState> interaction_state_{};
};
