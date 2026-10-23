import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: rootCharging
    width: 1280
    height: 720
    color: "#080B11"

    property real chargePowerKw: 11.2
    property real chargeVoltageV: 112.4
    property real chargeCurrentA: 31.8
    property real socPct: 62.0
    property int timeToFullMinutes: 48
    property real battTempC: 31.0

    // Pulsing charging glow circle in center
    Rectangle {
        id: glowCircle
        anchors.centerIn: parent
        width: 380
        height: 380
        radius: 190
        color: "transparent"
        border.color: "#00E676"
        border.width: 12

        SequentialAnimation on scale {
            loops: Animation.Infinite
            PropertyAnimation { to: 1.05; duration: 1200; easing.type: Easing.InOutQuad }
            PropertyAnimation { to: 0.95; duration: 1200; easing.type: Easing.InOutQuad }
        }

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "⚡ ŞARJ OLUYOR"
                color: "#00E676"
                font.pixelSize: 22
                font.bold: true
                font.letterSpacing: 3
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Math.round(rootCharging.socPct) + "%"
                color: "#FFFFFF"
                font.pixelSize: 84
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: rootCharging.chargePowerKw.toFixed(1) + " kW"
                color: "#00E5FF"
                font.pixelSize: 32
                font.bold: true
            }
        }
    }

    // Charging Details Bottom Bar
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        width: 900
        height: 100
        radius: 20
        color: "#121824"

        Row {
            anchors.centerIn: parent
            spacing: 90

            Column {
                Text { text: "GERİLİM"; color: "#6C7A9C"; font.pixelSize: 14 }
                Text { text: rootCharging.chargeVoltageV + " V"; color: "#FFFFFF"; font.pixelSize: 24; font.bold: true }
            }
            Column {
                Text { text: "AKIM"; color: "#6C7A9C"; font.pixelSize: 14 }
                Text { text: rootCharging.chargeCurrentA + " A"; color: "#FFFFFF"; font.pixelSize: 24; font.bold: true }
            }
            Column {
                Text { text: "BATARYA SICAKLIĞI"; color: "#6C7A9C"; font.pixelSize: 14 }
                Text { text: rootCharging.battTempC + " °C"; color: "#FFFFFF"; font.pixelSize: 24; font.bold: true }
            }
            Column {
                Text { text: "DOLUMA KALAN SÜRE"; color: "#6C7A9C"; font.pixelSize: 14 }
                Text { text: rootCharging.timeToFullMinutes + " dk"; color: "#00E676"; font.pixelSize: 24; font.bold: true }
            }
        }
    }
}
