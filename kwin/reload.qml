import QtQuick
import MacGlassBootstrap 1.0 as Native
// Reuse the protected, already registered module; only the effect path changes.
Native.Bootstrap {
    Component.onCompleted: console.info("MacGlass session effect loaded:", load(Qt.resolvedUrl("plugins")))
}
