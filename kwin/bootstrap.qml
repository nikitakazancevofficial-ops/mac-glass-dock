import QtQml
import "native" as Native

QtObject {
    property Native.Bootstrap bridge: Native.Bootstrap {}
    Component.onCompleted: {
        const ok = bridge.load(Qt.resolvedUrl("plugins"))
        console.info("MacGlass bootstrap loaded:", ok)
    }
}
