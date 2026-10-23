// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
import QtQuick
import org.kde.taskmanager as TaskManager
import org.kde.pipewire as PipeWire

Item {
    id: root
    property string uuid: ""
    readonly property bool ready: video.ready
    readonly property bool failed: video.state === PipeWire.PipeWireSourceItem.Error
    readonly property int nodeId: request.nodeId
    TaskManager.ScreencastingRequest {id: request; uuid: root.uuid}
    PipeWire.PipeWireSourceItem {
        id: video
        anchors.fill: parent
        nodeId: request.nodeId
        objectSerial: request.objectSerial
        // Hiding until ready stops PipeWire itself: an invisible item pauses
        // its stream, so the first frame can never make ready become true.
        visible: true
    }
}
