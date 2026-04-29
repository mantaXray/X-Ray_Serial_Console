---
name: qt-widgets-cpp
description: Use when modifying or debugging Qt Widgets C++ desktop applications, especially work involving UI layout, signals and slots, QObject lifetime, timers, worker threads, QSettings persistence, Qt Creator, CMake, translations, or UTF-8 text handling.
---

# Qt Widgets C++ Development

## Core Rule

Treat Qt UI behavior, object lifetime, persistent settings, project files, documentation, and verification as one change set. A Qt fix is not complete until the visible behavior, ownership model, build files, and relevant notes agree.

## Workflow

1. Read the repository guidance first: `AGENTS.md`, `README.md`, and any `docs/` design or pitfall notes if they exist.
2. Inspect the current Qt structure before editing:
   - `CMakeLists.txt` or `.pro`
   - `src/`, `include/`, `ui/`, `resources/`, `translations/`
   - existing signal/slot style and widget ownership patterns
3. For bugs, trace the real event path before changing code:
   - user action -> signal/slot -> worker/timer/state change -> UI refresh
   - background data -> queued signal -> main-thread update -> persistent state
4. Make the smallest root-cause fix. Do not redesign unrelated layouts, rename files, or move modules unless the task requires it.
5. Update context when behavior changes:
   - user-visible behavior: `README.md`
   - architecture, data flow, settings, or module boundaries: `docs/design.md`
   - repeatable Qt/CMake/platform traps: `docs/pitfalls.md`
   - non-obvious C++/Qt logic: concise source comments
6. Verify with the repository's own scripts when present, matching the requested scope. If the user asks for limited validation, do not run a full build; prefer syntax checks, CMake configure, focused static regression checks, and `git diff --check`.
7. If no limited validation is requested, use CMake directly:
   - `cmake -S . -B build`
   - `cmake --build build`

## Qt Checks

| Area | Check |
| --- | --- |
| Threading | Widgets must be created and updated on the GUI thread. Use queued signals for worker-to-UI updates. |
| QObject lifetime | Prefer parent ownership or explicit scoped ownership. Disconnect or stop background work before objects are hidden or destroyed. |
| Hidden windows | Closing a `QDockWidget` or non-deleting dialog may only hide it. Stop timers, polling, pending protocol receive sessions, and background sends on hide/close boundaries. |
| Timers | Define start, stop, restart, timeout, and teardown behavior. Avoid hidden timers mutating shared state. |
| Layouts | Use splitters, size policies, stretch factors, and table header modes deliberately. Default splitter sizes and table column totals must make important columns visible on first open; preserve user-resized headers when refreshing data. For tool windows with logs and tables, set the default splitter ratio from the user's first diagnostic need, not from equal halves. When compacting a form row, group each label with its editor and re-check parent stretch factors so empty width does not just move to the row end. |
| Menus | Reflect module boundaries in menu hierarchy. Put toolbox pages under their owning toolbox, but avoid a single-child submenu for an independent tool window. Do not mix view toggles and send actions into the same flat group. |
| Traffic logs | Treat timestamp, direction visibility, payload format, packet splitting, and pause state as separate concerns. Disabling timestamps must not hide TX/RX frames. A protocol tool's TX may be visible in the main traffic log, but it must not flush or reorder the main receive view's timeout-split RX buffer unless that send is intentionally a main receive boundary. |
| Protocol tools | Gate protocol-specific RX buffers by an active request/response session. A tool log should not become a global serial monitor unless that is explicitly designed. Keep main receive display settings and protocol-tool raw-log settings in separate controls and settings keys. |
| Filters | If a filter can hide all visible output while counters still advance, show an obvious filtered state and an empty-match hint. Persist filters deliberately and make clearing them discoverable. |
| Shared options | Do not wire similar-looking controls from different panels directly together unless they truly represent the same state. Prefer separate settings keys for independent views. |
| Units | Keep fixed units outside numeric inputs as adjacent labels. Use input suffixes only when they materially improve parsing or when the local UI already relies on that pattern. |
| Tables | For per-cell or per-row display options, define selection behavior, context-menu batch actions, multi-cell data spans, and tooltips/status feedback. Checkbox-only columns need explicit headers/tooltips, safe defaults, and a visible unchecked state; don't rely on a hidden horizontal scrollbar for discovery. |
| Compact inputs | Set combo boxes, spin boxes, and line edits to widths that fit real values plus a small margin. Common protocol fields such as function code, station id, address, quantity, and intervals should not keep large empty widths that steal space from logs or tables. Size spin boxes for the maximum value digits plus the platform stepper buttons, not just the visible default value. |
| Item Widgets | `QTableWidgetItem::ItemIsUserCheckable` is delegate-drawn and may not inherit `QCheckBox::indicator` styling. For important boolean controls, use real `QCheckBox` cell widgets or a tested delegate, then centralize state access through helper functions. |
| Persistence | Centralize `QSettings` path creation. Save/restore window geometry, splitter state, header state, user options, editor drafts, and presets consistently. Do not use a CMake build output directory as the only settings location; prefer a stable app config path for development runs while preserving explicit portable INI behavior. Version persisted header/splitter layouts when changing default geometry so stale defaults do not override new UI fixes. Keep settings-save functions free of display timing side effects; flush receive buffers only at explicit caller-owned boundaries. |
| CMake | Keep source, header, UI, resource, translation, and important documentation files listed so Qt Creator can show them. |
| Packaging | Use the matching Qt kit's deployment tool, such as `windeployqt` on Windows. Build installers from a deployed portable directory instead of copying only the executable. If a helper script is copied to the build directory, make it discover the source root from `CMakeCache.txt` or an explicit parameter. Keep BAT files as thin launchers over a PowerShell script. |
| Commit messages | When asked to generate a Git commit message, write bilingual Chinese/English plain text to `build/commit-message-<version>.txt` for TortoiseGit instead of relying on chat formatting. Ensure the application version has been bumped for the commit, then include summary, key changes, verification, and version status. |
| Encoding | Read/write source, translation, and Markdown files as UTF-8 when Chinese comments or UI text are present. |
| Translations | Keep UI text paths consistent when adding language switching or `.ts` files. |

## Common Mistakes

- Updating UI behavior but forgetting the matching CMake or Qt Creator-visible project files.
- Rebuilding a table or model in a way that loses user-adjusted column widths.
- Setting table columns wider than the default panel/splitter allocation, so first-open UI hides important columns behind a horizontal scrollbar.
- Using terse checkbox headers such as `HEX` when the action is really a per-row mode switch; name it by behavior and set a conservative default.
- Assuming item-view checkboxes look like styled page checkboxes. Verify both checked and unchecked states on the actual table background.
- Treating multi-register or multi-cell values as if every table cell can be parsed independently.
- Assuming a hidden dock/dialog has stopped its timer or worker task.
- Letting a hidden or idle protocol tool keep buffering bytes from the main serial receive path.
- Hiding all receive text through a saved filter without showing that filtering is active.
- Reusing main receive display checkboxes for a protocol tool's independent raw log.
- Putting fixed units like `ms` inside a spin box, shrinking the editable value and implying the unit can be changed.
- Treating a timestamp checkbox as the switch that decides whether outgoing serial frames are displayed.
- Letting a protocol tool's TX log reuse the main send-log path with default flush behavior when the main receive area has its own timeout packet splitting.
- Calling receive-buffer flushes from generic settings persistence, causing dock visibility, splitter movement, or table header saves to change live receive timing.
- Giving diagnostic logs and register tables equal splitter space by default when one of them is clearly more important for first-open debugging.
- Leaving short protocol inputs and function-code combo boxes at generic wide form-control widths.
- Fixing over-wide inputs with fixed size policies while leaving a trailing stretch or stale saved geometry that preserves the same right-side blank area.
- Updating widgets directly from a worker thread.
- Adding a second `QSettings` location instead of using the application's shared settings factory.
- Writing settings only beside the executable while debugging from a CMake build tree, then losing them when the build directory is cleaned or switched.
- Saving send options but forgetting the editor draft or other user-entered text that users expect to restore.
- Shipping a Qt Windows application by copying only the `.exe` and forgetting plugins or compiler runtime DLLs.
- Fixing a Qt timing bug without documenting the lifecycle boundary that caused it.
