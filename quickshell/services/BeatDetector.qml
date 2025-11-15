pragma Singleton

import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property real bpm: 1

    Process {
        id: beatDetectorProc
        running: false  // Only run when explicitly needed
        command: [Quickshell.env("CAELESTIA_BD_PATH") || "/home/nafi/.config/quickshell/caelestia/assets/beat_detector", "190", "--no-log", "--no-stats", "--no-visual"]
        stdout: SplitParser {
            onRead: data => {
                const match = data.match(/\s*BPM: ([0-9]+\.[0-9]+)/);
                if (match)
                    root.bpm = parseFloat(match[1]);
            }
        }
    }
}
