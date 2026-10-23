import QtQuick

Item {
    id: icon
    property string kind: "wifi"
    property color color: "white"
    property real stroke: 2.8
    property bool useC: false
    property bool darkAsset: false
    property real batteryLevel: 1
    property bool charging: false

    Image {
        anchors.fill: parent
        visible: icon.useC
        source: icon.useC ? "qrc:/qt/qml/MacGlassDock/assets/control-center/icons/" + icon.kind + "-c" + (icon.darkAsset ? "-dark" : "") + ".svg" : ""
        sourceSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
        smooth: true
    }

    Canvas {
        id: chargingBolt
        anchors.fill: parent
        visible: icon.useC && icon.kind === "battery" && icon.charging
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.fillStyle = icon.darkAsset ? "#172b42" : "#ffffff"
            c.beginPath()
            c.moveTo(width * .50, height * .36)
            c.lineTo(width * .375, height * .53)
            c.lineTo(width * .484, height * .53)
            c.lineTo(width * .453, height * .66)
            c.lineTo(width * .625, height * .455)
            c.lineTo(width * .516, height * .455)
            c.closePath()
            c.fill()
        }
        Connections {
            target: icon
            function onChargingChanged() { chargingBolt.requestPaint() }
            function onDarkAssetChanged() { chargingBolt.requestPaint() }
        }
    }

    Canvas {
        id: canvas
        visible: !icon.useC
        anchors.fill: parent
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.strokeStyle = icon.color
            c.fillStyle = icon.color
            // Variant C: heavier, compact glyphs that stay legible over wallpaper.
            c.lineWidth = icon.stroke * 1.35
            c.lineCap = "round"
            c.lineJoin = "round"
            const w = width, h = height, cx = w / 2, cy = h / 2
            if (icon.kind === "wifi") {
                c.lineWidth = Math.max(4, icon.stroke * 1.55)
                for (const r of [h*.36, h*.235]) { c.beginPath(); c.arc(cx, cy+h*.1, r, Math.PI*1.22, Math.PI*1.78); c.stroke() }
                c.beginPath(); c.arc(cx, cy+h*.1, h*.11, 0, Math.PI*2); c.fill()
            } else if (icon.kind === "bluetooth") {
                c.lineWidth = Math.max(4, icon.stroke * 1.45)
                c.beginPath(); c.moveTo(cx, h*.08); c.lineTo(cx, h*.92); c.lineTo(cx+w*.31, h*.64); c.lineTo(cx-w*.31, h*.36); c.lineTo(cx+w*.31, h*.08); c.lineTo(cx-w*.31, h*.36); c.lineTo(cx+w*.31, h*.64); c.lineTo(cx-w*.31, h*.92); c.stroke()
            } else if (icon.kind === "airdrop") {
                for (const r of [h*.36, h*.25, h*.14]) { c.beginPath(); c.arc(cx, cy+h*.08, r, Math.PI*1.15, Math.PI*1.85); c.stroke() }
                c.beginPath(); c.moveTo(cx, h*.57); c.lineTo(cx-w*.12, h*.75); c.lineTo(cx+w*.12, h*.75); c.closePath(); c.stroke()
            } else if (icon.kind === "focus") {
                c.beginPath(); c.arc(cx-w*.05, cy, h*.29, 0, Math.PI*2); c.fill()
                c.fillStyle = "transparent"; c.globalCompositeOperation = "destination-out"; c.beginPath(); c.arc(cx+w*.09, cy-h*.08, h*.29, 0, Math.PI*2); c.fill(); c.globalCompositeOperation = "source-over"
            } else if (icon.kind === "display") {
                c.beginPath(); c.arc(cx, cy, h*.22, 0, Math.PI*2); c.fill(); c.lineWidth = Math.max(4, icon.stroke * 1.35); for(let i=0;i<8;i++){const a=i*Math.PI/4;c.beginPath();c.moveTo(cx+Math.cos(a)*h*.31,cy+Math.sin(a)*h*.31);c.lineTo(cx+Math.cos(a)*h*.45,cy+Math.sin(a)*h*.45);c.stroke()}
            } else if (icon.kind === "sound") {
                c.beginPath(); c.moveTo(w*.18,h*.42); c.lineTo(w*.34,h*.42); c.lineTo(w*.52,h*.24); c.lineTo(w*.52,h*.76); c.lineTo(w*.34,h*.58); c.lineTo(w*.18,h*.58); c.closePath(); c.fill(); c.beginPath(); c.arc(w*.45,cy,h*.3,-.72,.72); c.stroke()
            } else if (icon.kind === "keyboard") {
                c.strokeRect(w*.1,h*.22,w*.8,h*.56); for(let row=0;row<2;row++)for(let col=0;col<5;col++){c.beginPath();c.arc(w*(.24+col*.13),h*(.38+row*.2),1.8,0,Math.PI*2);c.fill()} c.beginPath();c.moveTo(w*.28,h*.66);c.lineTo(w*.72,h*.66);c.stroke()
            } else if (icon.kind === "battery") {
                c.lineWidth = Math.max(1.5, icon.stroke)
                c.strokeRect(w*.05,h*.19,w*.78,h*.62)
                c.fillRect(w*.86,h*.38,w*.1,h*.24)
                const level = Math.max(0, Math.min(1, icon.batteryLevel))
                if (level > 0) c.fillRect(w*.12,h*.29,w*.64*level,h*.42)
                if (icon.charging) {
                    c.fillStyle = "#278ff0"
                    c.beginPath()
                    c.moveTo(w*.53,h*.27); c.lineTo(w*.37,h*.52); c.lineTo(w*.48,h*.52)
                    c.lineTo(w*.42,h*.73); c.lineTo(w*.64,h*.45); c.lineTo(w*.51,h*.45)
                    c.closePath(); c.fill()
                }
            } else if (icon.kind === "controls") {
                c.lineWidth = Math.max(1.7, icon.stroke)
                const pill = (x, y, width, height, knobX) => {
                    const radius = height/2
                    c.beginPath(); c.moveTo(x+radius,y); c.arcTo(x+width,y,x+width,y+height,radius)
                    c.arcTo(x+width,y+height,x,y+height,radius); c.arcTo(x,y+height,x,y,radius)
                    c.arcTo(x,y,x+width,y,radius); c.stroke()
                    c.beginPath(); c.arc(knobX,y+radius,height*.23,0,Math.PI*2); c.fill()
                }
                pill(w*.17,h*.14,w*.68,h*.29,w*.62)
                pill(w*.17,h*.57,w*.68,h*.29,w*.40)
            } else if (icon.kind === "mirroring") {
                c.strokeRect(w*.16,h*.18,w*.56,h*.48); c.strokeRect(w*.34,h*.34,w*.56,h*.48)
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: icon; function onColorChanged(){ canvas.requestPaint() } function onKindChanged(){ canvas.requestPaint() } function onBatteryLevelChanged(){ canvas.requestPaint() } function onChargingChanged(){ canvas.requestPaint() } }
    }
}
