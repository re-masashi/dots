import Quickshell.Io

JsonObject {
    property bool persistent: false
    property bool showOnHover: false
    property int dragThreshold: 20
    property Workspaces workspaces: Workspaces {}
    property Status status: Status {}
    property Sizes sizes: Sizes {}

    component Workspaces: JsonObject {
        property int shown: 5
        property bool rounded: true
        property bool activeIndicator: true
        property bool occupiedBg: false
        property bool showWindows: false
        property bool activeTrail: false
        property string label: "  "
        property string occupiedLabel: "󰮯 "
        property string activeLabel: "󰮯 "
    }

    component Status: JsonObject {
        property bool showAudio: false
        property bool showKbLayout: false
        property bool showNetwork: false
        property bool showBluetooth: false
        property bool showBattery: false
    }

    component Sizes: JsonObject {
        property int innerHeight: 0
        property int windowPreviewSize: 400
        property int trayMenuWidth: 300
        property int batteryWidth: 250
        property int networkWidth: 320
    }
}
