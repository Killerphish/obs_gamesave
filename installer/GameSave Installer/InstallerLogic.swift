// InstallerLogic.swift – OBS detection, plugin copy, paths

import AppKit
import Foundation

enum InstallerError: Error {
    case pluginNotFoundInBundle
    case cannotCreatePluginsDirectory(Error)
    case copyFailed(Error)
}

struct InstallerLogic {
    static let obsBundleId = "com.obsproject.obs-studio"
    static let pluginName = "game-save"
    static let pluginExtension = "plugin"

    /// Plugins directory: ~/Library/Application Support/obs-studio/plugins
    static var pluginsDirectory: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/obs-studio/plugins", isDirectory: true)
    }

    /// OBS Application Support parent (to detect if OBS has been run)
    static var obsApplicationSupport: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/obs-studio", isDirectory: true)
    }

    /// Whether OBS Studio is currently running
    static var isOBSRunning: Bool {
        !NSRunningApplication.runningApplications(withBundleIdentifier: obsBundleId).isEmpty
    }

    /// Quit OBS Studio. Returns false if user cancelled or quit failed.
    static func quitOBS() -> Bool {
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier: obsBundleId)
        for app in apps {
            app.terminate()
            // Allow a short time to quit
            let deadline = Date().addingTimeInterval(3)
            while Date() < deadline, !app.isTerminated {
                RunLoop.current.run(until: Date().addingTimeInterval(0.1))
            }
            if !app.isTerminated {
                app.forceTerminate()
            }
        }
        return true
    }

    /// URL of the embedded plugin in the app bundle, or nil if not found
    static func embeddedPluginURL() -> URL? {
        Bundle.main.url(forResource: pluginName, withExtension: pluginExtension, subdirectory: nil)
    }

    /// Whether the OBS plugins directory (or parent) exists
    static var pluginsDirectoryExists: Bool {
        let fm = FileManager.default
        return fm.fileExists(atPath: pluginsDirectory.path) || fm.fileExists(atPath: obsApplicationSupport.path)
    }

    /// Ensure plugins directory exists; create if needed
    static func ensurePluginsDirectory() throws {
        let fm = FileManager.default
        do {
            try fm.createDirectory(at: pluginsDirectory, withIntermediateDirectories: true, attributes: nil)
        } catch {
            throw InstallerError.cannotCreatePluginsDirectory(error)
        }
    }

    /// Copy plugin from source URL to OBS plugins directory (replaces existing)
    static func installPlugin(from source: URL) throws {
        let dest = pluginsDirectory.appendingPathComponent("\(pluginName).\(pluginExtension)", isDirectory: false)
        let fm = FileManager.default
        try ensurePluginsDirectory()
        if fm.fileExists(atPath: dest.path) {
            try fm.removeItem(at: dest)
        }
        do {
            try fm.copyItem(at: source, to: dest)
        } catch {
            throw InstallerError.copyFailed(error)
        }
    }

    /// Copy embedded plugin to OBS plugins directory (replaces existing)
    static func installPlugin() throws {
        guard let source = embeddedPluginURL() else {
            throw InstallerError.pluginNotFoundInBundle
        }
        try installPlugin(from: source)
    }

    /// Open OBS if installed in /Applications
    static func openOBSIfInstalled() -> Bool {
        let obsURL = URL(fileURLWithPath: "/Applications/OBS.app")
        guard FileManager.default.fileExists(atPath: obsURL.path) else { return false }
        return NSWorkspace.shared.open(obsURL)
    }
}
