pragma Singleton

import qs.config
import Quickshell
import Quickshell.Io
import QtQuick

Singleton {
    id: root

    property list<int> values: Array(Config.dashboard.visualiserBars)
    property int refCount

    Connections {
        target: Config.dashboard

        function onVisualiserBarsChanged() {
            root.values = Array(Config.dashboard.visualiserBars);
            cavaProc.running = false;
            cavaProc.running = true;
        }
    }

    Process {
        id: cavaProc

        running: root.refCount > 0  // Only run when actually needed
        command: ["sh", "-c", `printf '[general]\nframerate=30\nbars=${Config.dashboard.visualiserBars}\nsleep_timer=3\n[output]\nchannels=mono\nmethod=raw\nraw_target=/dev/stdout\ndata_format=ascii\nascii_max_range=100' | cava -p /dev/stdin`]  // Reduced framerate from 60 to 30
        stdout: SplitParser {
            onRead: data => {
                if (root.refCount)
                    root.values = data.slice(0, -1).split(";").map(v => parseInt(v, 10));
            }
        }
    }
}
