// Explicit entry point so the app runs correctly when built with swiftc (no storyboard).
// Without this, @main may not connect the delegate or start the run loop properly.
import AppKit

let delegate = AppDelegate()
NSApplication.shared.delegate = delegate
_ = NSApplicationMain(CommandLine.argc, CommandLine.unsafeArgv)
