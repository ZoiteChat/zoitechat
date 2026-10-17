// Capture only the tested process's window; a desktop-only image is not evidence.
import AppKit
import CoreGraphics
import Foundation

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}

guard CommandLine.arguments.count == 3,
      let pid = Int32(CommandLine.arguments[1]) else {
    fail("Usage: capture-window PID OUTPUT.png")
}
let output = URL(fileURLWithPath: CommandLine.arguments[2])
let deadline = Date().addingTimeInterval(30)
var chosen: [String: Any]?
while Date() < deadline {
    guard let app = NSRunningApplication(processIdentifier: pid), !app.isTerminated else {
        fail("ZoiteChat is not running")
    }
    app.activate(options: [])
    RunLoop.current.run(until: Date().addingTimeInterval(0.2))
    let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements],
                                            kCGNullWindowID) as? [[String: Any]] ?? []
    let owned = windows.filter { window in
        guard (window[kCGWindowOwnerPID as String] as? NSNumber)?.int32Value == pid,
              (window[kCGWindowLayer as String] as? NSNumber)?.intValue == 0,
              let bounds = window[kCGWindowBounds as String] as? [String: Any],
              let width = bounds["Width"] as? NSNumber,
              let height = bounds["Height"] as? NSNumber else { return false }
        return width.doubleValue >= 600 && height.doubleValue >= 300
    }
    chosen = owned.max { left, right in
        func area(_ window: [String: Any]) -> Double {
            let bounds = window[kCGWindowBounds as String] as? [String: NSNumber] ?? [:]
            return (bounds["Width"]?.doubleValue ?? 0) * (bounds["Height"]?.doubleValue ?? 0)
        }
        return area(left) < area(right)
    }
    if chosen != nil { break }
}
guard let window = chosen,
      let identifier = window[kCGWindowNumber as String] as? NSNumber else {
    fail("No visible ZoiteChat window found in this runner's desktop session")
}
let capture = Process()
capture.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
capture.arguments = ["-x", "-o", "-t", "png", "-l", identifier.stringValue, output.path]
do { try capture.run() } catch { fail("Cannot start screencapture: \(error)") }
capture.waitUntilExit()
guard capture.terminationStatus == 0,
      let png = try? Data(contentsOf: output),
      let bitmap = NSBitmapImageRep(data: png),
      bitmap.pixelsWide >= 600, bitmap.pixelsHigh >= 300 else {
    fail("Application window capture failed; check runner Screen Recording permissions")
}
var colors = Set<String>()
for y in stride(from: 0, to: bitmap.pixelsHigh, by: max(1, bitmap.pixelsHigh / 60)) {
    for x in stride(from: 0, to: bitmap.pixelsWide, by: max(1, bitmap.pixelsWide / 80)) {
        if let color = bitmap.colorAt(x: x, y: y)?.usingColorSpace(.deviceRGB) {
            colors.insert("\(Int(color.redComponent * 255)),\(Int(color.greenComponent * 255)),\(Int(color.blueComponent * 255))")
        }
    }
}
guard colors.count >= 8 else { fail("Window capture is blank or nearly uniform") }
let evidence: [String: Any] = ["pid": pid, "window_id": identifier.intValue,
    "width": bitmap.pixelsWide, "height": bitmap.pixelsHigh, "sampled_colors": colors.count]
do {
    let data = try JSONSerialization.data(withJSONObject: evidence, options: [.sortedKeys])
    print(String(decoding: data, as: UTF8.self))
} catch { fail("Cannot serialize screenshot evidence: \(error)") }
