// AppDelegate.swift – GameSave Installer app entry and dialogs

import AppKit
import Foundation

final class AppDelegate: NSObject, NSApplicationDelegate {
    /// Keep a visible window so the app has a front presence; dialogs show above it.
    private var mainWindow: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Required: be a normal app (Dock icon, can show windows). Default can be .accessory when built without a storyboard.
        NSApp.setActivationPolicy(.regular)
        showMainWindow()
        NSApp.activate(ignoringOtherApps: true)
        mainWindow?.makeKeyAndOrderFront(nil)
        // Let the window appear and paint before showing any modal
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.4) { [weak self] in
            self?.runInstallFlow()
        }
    }

    private func showMainWindow() {
        let contentRect = NSRect(x: 0, y: 0, width: 400, height: 140)
        let window = NSWindow(
            contentRect: contentRect,
            styleMask: [.titled, .closable],
            backing: .buffered,
            defer: false
        )
        window.title = "GameSave Installer"
        window.center()
        window.isReleasedWhenClosed = false
        window.collectionBehavior = [.canJoinAllSpaces, .fullScreenNone]

        let contentView = NSView(frame: NSRect(origin: .zero, size: contentRect.size))
        contentView.wantsLayer = true
        window.contentView = contentView

        let label = NSTextField(labelWithString: "Preparing to install…")
        label.font = NSFont.systemFont(ofSize: NSFont.systemFontSize)
        label.frame = NSRect(x: 24, y: 58, width: 352, height: 22)
        label.autoresizingMask = [.minXMargin, .maxXMargin]
        contentView.addSubview(label)

        mainWindow = window
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        true
    }

    private func runInstallFlow() {
        NSApp.activate(ignoringOtherApps: true)
        // 1. Get plugin source: embedded or user-chosen
        let pluginSource: URL?
        if let embedded = InstallerLogic.embeddedPluginURL() {
            pluginSource = embedded
        } else {
            pluginSource = choosePluginFile()
            if pluginSource == nil {
                NSApp.terminate(nil)
                return
            }
        }
        guard let source = pluginSource else {
            NSApp.terminate(nil)
            return
        }

        // 2. If OBS is running, prompt to quit
        if InstallerLogic.isOBSRunning {
            let alert = NSAlert()
            alert.messageText = "OBS Studio is running"
            alert.informativeText = "Please quit OBS to install the GameSave plugin."
            alert.alertStyle = .warning
            alert.addButton(withTitle: "Quit OBS")
            alert.addButton(withTitle: "Cancel")
            let response = alert.runModal()
            if response == .alertSecondButtonReturn {
                NSApp.terminate(nil)
                return
            }
            _ = InstallerLogic.quitOBS()
        }

        // 3. Optional: warn if OBS plugin folder doesn't exist
        if !InstallerLogic.pluginsDirectoryExists {
            let alert = NSAlert()
            alert.messageText = "OBS plugin folder not found"
            alert.informativeText = "The OBS Studio plugin folder doesn't exist yet. Install OBS Studio first, or we can create the folder and install the plugin anyway."
            alert.alertStyle = .informational
            alert.addButton(withTitle: "Continue")
            alert.addButton(withTitle: "Cancel")
            if alert.runModal() == .alertSecondButtonReturn {
                NSApp.terminate(nil)
                return
            }
        }

        // 4. Install plugin
        do {
            try InstallerLogic.installPlugin(from: source)
        } catch InstallerError.pluginNotFoundInBundle {
            showError("Plugin not found in installer.", informative: "Please re-download the installer.")
            NSApp.terminate(nil)
            return
        } catch InstallerError.cannotCreatePluginsDirectory(let err) {
            showError("Could not create plugin folder.", informative: err.localizedDescription)
            NSApp.terminate(nil)
            return
        } catch InstallerError.copyFailed(let err) {
            showError("Installation failed.", informative: err.localizedDescription)
            NSApp.terminate(nil)
            return
        } catch {
            showError("Installation failed.", informative: error.localizedDescription)
            NSApp.terminate(nil)
            return
        }

        // 5. Success – offer to open OBS
        let alert = NSAlert()
        alert.messageText = "GameSave is installed"
        alert.informativeText = "Restart OBS (or open OBS) and go to Docks → GameSave."
        alert.alertStyle = .informational
        alert.addButton(withTitle: "Open OBS")
        alert.addButton(withTitle: "OK")
        if alert.runModal() == .alertFirstButtonReturn {
            _ = InstallerLogic.openOBSIfInstalled()
        }
        NSApp.terminate(nil)
    }

    /// Let user pick a game-save.plugin from disk (when not bundled)
    private func choosePluginFile() -> URL? {
        let alert = NSAlert()
        alert.messageText = "No plugin bundled"
        alert.informativeText = "Choose a game-save.plugin file to install (e.g. from a build or download)."
        alert.alertStyle = .informational
        alert.addButton(withTitle: "Choose Plugin…")
        alert.addButton(withTitle: "Cancel")
        guard alert.runModal() == .alertFirstButtonReturn else { return nil }

        let panel = NSOpenPanel()
        panel.title = "Select GameSave Plugin"
        panel.canChooseDirectories = true
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.directoryURL = FileManager.default.homeDirectoryForCurrentUser
        if panel.runModal() == .OK, let url = panel.url {
            let name = url.lastPathComponent
            if name == "\(InstallerLogic.pluginName).\(InstallerLogic.pluginExtension)" {
                return url
            }
            if name.hasSuffix(".\(InstallerLogic.pluginExtension)") {
                return url
            }
            let inner = url.appendingPathComponent("\(InstallerLogic.pluginName).\(InstallerLogic.pluginExtension)")
            if FileManager.default.fileExists(atPath: inner.path) {
                return inner
            }
            return url
        }
        return nil
    }

    private func showError(_ message: String, informative: String) {
        let alert = NSAlert()
        alert.messageText = message
        alert.informativeText = informative
        alert.alertStyle = .critical
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }
}
