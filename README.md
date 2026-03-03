# 🪟 WindowsResize

> **Control your windows with elegance — no grabbing borders, no dragging the title bar.**

WindowsResize is a lightweight utility for **Windows**, written in **C++20**, that runs quietly in the background and turns any keyboard shortcut into a powerful window move and resize command — inspired by the classic utility from **Ubuntu/GNOME**.

---

## ✨ Why use it?

On Windows, moving or resizing a window requires precision: you need to click exactly on the title bar or the edges — especially the thin edges, which can be frustrating. WindowsResize solves this intuitively:

- 🔑 Press a shortcut
- 🖱️ Move the mouse wherever you want
- ✅ Press the shortcut again (or `Esc`) to confirm

That simple. Works with **any window** on the system.

---

## 🚀 Features

| Feature | Description |
|---|---|
| 🔄 **Move windows** | Moves the active window using the mouse, without clicking on the title bar |
| ↔️ **Resize windows** | Resizes the window from the edge closest to the cursor |
| 🖥️ **System tray** | Tray icon indicates the app is active |
| ⚡ **High frequency** | Updates at ~125Hz (every 8ms) for ultra-smooth movement |
| 🪶 **Lightweight and discreet** | Hidden window, no UI, zero performance impact |
| 🔁 **Auto-restore** | Minimized or maximized windows are restored before interaction |

---

## ⌨️ Keyboard Shortcuts

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#4f46e5',
    'primaryTextColor': '#ffffff',
    'primaryBorderColor': '#312e81',
    'lineColor': '#818cf8',
    'secondaryColor': '#1e1b4b',
    'tertiaryColor': '#eef2ff',
    'tertiaryTextColor': '#312e81',
    'clusterBkg': '#1e1b4b',
    'clusterBorder': '#4f46e5',
    'titleColor': '#c7d2fe',
    'fontFamily': 'monospace'
  }
}}%%
flowchart LR
    classDef hotkey  fill:#4f46e5,stroke:#312e81,color:#fff,font-weight:bold,rx:6
    classDef action  fill:#0f172a,stroke:#4f46e5,color:#c7d2fe,rx:6
    classDef cancel  fill:#7c3aed,stroke:#4c1d95,color:#fff,font-weight:bold,rx:6
    classDef quit    fill:#be123c,stroke:#881337,color:#fff,font-weight:bold,rx:6

    K1["⌨️  Alt + 1"]              -->|triggers| A1["🔄  Move active window"]
    K2["⌨️  Alt + 2"]              -->|triggers| A2["↔️  Resize active window"]
    K3["⌨️  Alt + 1 or Alt + 2  (press again)"]  -->|cancels| A3["⛔  Stop current action"]
    K4["⌨️  Esc"]                  -->|cancels| A3
    K5["⌨️  Alt + Shift + Q"]      -->|exits| A5["🚪  Close the application"]

    class K1,K2 hotkey
    class K3,K4 cancel
    class K5 quit
    class A1,A2,A3,A5 action
```

### Quick reference

| Shortcut | Action |
|---|---|
| `Alt + 1` | 🔄 Start/cancel **move** mode for the active window |
| `Alt + 2` | ↔️ Start/cancel **resize** mode for the active window |
| `Esc` | ⛔ Cancel any ongoing action |
| `Alt + Shift + Q` | 🚪 Exit the application |

---

## 🔍 How it works

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#4f46e5',
    'primaryTextColor': '#ffffff',
    'primaryBorderColor': '#312e81',
    'lineColor': '#818cf8',
    'secondaryColor': '#1e1b4b',
    'tertiaryColor': '#312e81',
    'tertiaryTextColor': '#c7d2fe',
    'clusterBkg': '#0f172a',
    'clusterBorder': '#4f46e5',
    'titleColor': '#c7d2fe'
  }
}}%%
flowchart TD
    classDef step   fill:#4f46e5,stroke:#312e81,color:#fff
    classDef cond   fill:#7c3aed,stroke:#4c1d95,color:#fff
    classDef done   fill:#059669,stroke:#064e3b,color:#fff
    classDef stop   fill:#be123c,stroke:#881337,color:#fff

    A([🖱️ User focuses a window]) --> B["⌨️ Presses Alt+1 or Alt+2"]
    B --> C{Valid window?}
    C -- No --> Z1([⚠️ Action ignored])
    C -- Yes --> D{Minimized or maximized?}
    D -- Yes --> E["🔁 Restore to normal mode"]
    D -- No --> F
    E --> F["📍 Capture cursor position\nand window dimensions"]
    F --> G{Selected mode}

    G -- Alt+1\nMove --> H["🔄 Apply cursor delta\nto window position"]
    G -- Alt+2\nResize --> I["🧭 Detect quadrant\n(cursor vs. window center)"]
    I --> J["↔️ Expand/shrink the\nclosest edge to cursor"]

    H --> K{Cancellation?}
    J --> K
    K -- Esc or\nsame shortcut --> L(["⛔ Interaction ended"])
    K -- Continue --> H

    class A,Z1,L done
    class B,E,F,H,I,J step
    class C,D,G,K cond
```

### Resize logic details

When pressing `Alt + 2`, the app detects **which quadrant of the window the cursor is in** at the moment of activation:

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#1e1b4b',
    'primaryTextColor': '#c7d2fe',
    'primaryBorderColor': '#4f46e5',
    'lineColor': '#6366f1',
    'clusterBkg': '#0f172a',
    'clusterBorder': '#4f46e5',
    'titleColor': '#c7d2fe'
  }
}}%%
quadrantChart
    title Quadrant detected when triggering Alt+2
    x-axis Left Edge --> Right Edge
    y-axis Bottom Edge --> Top Edge
    quadrant-1 "↗️ Top Right"
    quadrant-2 "↖️ Top Left"
    quadrant-3 "↙️ Bottom Left"
    quadrant-4 "↘️ Bottom Right"
    Cursor: [0.75, 0.75]
```

The resized edge is always the **one closest to the cursor** — making the behavior intuitive and non-destructive. The app also enforces minimum dimensions of **120 × 80 pixels**.

---

## 🖥️ System Tray

The system tray icon confirms the app is active and running.

| Action | Result |
|---|---|
| **Left-click** or **right-click** the icon | Opens the context menu with the **Exit** option |
| **Double-click** the icon | Plays a confirmation sound |

> 💡 If Windows restarts the taskbar (Explorer), the icon is **automatically restored**.

---

## 🏗️ Build

The project uses **CMake 3.21+** and **C++20**. Since it relies exclusively on the Win32 API, **the executable runs on Windows only**.

### 🪟 Windows — Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is generated at `build\Release\WindowsResize.exe`.

### 🪟 Windows — Ninja + MSVC or Clang

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

---

## 🧰 Requirements

| Requirement | Minimum version |
|---|---|
| Operating System | Windows 10 or later |
| Compiler | MSVC 2019+, Clang 12+, or MinGW-w64 |
| CMake | 3.21 or later |
| C++ Standard | C++20 |


---

<div align="center">

Made with 💜 in C++20 for Windows, powered by lots of ☕ 

</div>
