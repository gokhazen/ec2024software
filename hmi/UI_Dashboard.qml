import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.15

Rectangle {
    id: rootDashboard
    width: 1280
    height: 720
    color: "#0A0D14"

    // Property bindings from C++ / Python context
    property real vehicleSpeedKmh: 68.0
    property real powerKw: 24.5
    property real socPct: 84.0
    property int remainingRangeKm: 310
    property string gear: "D"
    property string driveMode: "SPORT"
    property bool isRegen: false
    property real motorTempC: 42.0
    property real battTempC: 28.0

    // Top Status Header
    Rectangle {
        id: topBar
        anchors.top: parent.top
        width: parent.width
        height: 60
        color: "#121722"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.verticalCenter: parent.verticalCenter
            text: "EV COCKPIT VCU 2.0"
            color: "#6C7A9C"
            font.pixelSize: 18
            font.bold: true
        }

        // Drive Mode Indicator
        Rectangle {
            anchors.centerIn: parent
            width: 140
            height: 36
            radius: 18
            color: rootDashboard.driveMode === "SPORT" ? "#FF4B2B" : (rootDashboard.driveMode === "ECO" ? "#00E676" : "#2979FF")

            Text {
                anchors.centerIn: parent
                text: rootDashboard.driveMode
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 16
            }
        }

        // Clock
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 40
            anchors.verticalCenter: parent.verticalCenter
            text: "14:25"
            color: "#E2E8F0"
            font.pixelSize: 20
        }
    }

    // Main Digital Speedometer (Center)
    Item {
        id: speedArea
        anchors.centerIn: parent
        width: 400
        height: 400

        Text {
            id: speedValue
            anchors.centerIn: parent
            text: Math.round(rootDashboard.vehicleSpeedKmh)
            color: "#FFFFFF"
            font.pixelSize: 130
            font.bold: true
        }

        Text {
            anchors.top: speedValue.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            text: "KM / H"
            color: "#6C7A9C"
            font.pixelSize: 22
            font.letterSpacing: 4
        }

        // Active Gear (P, R, N, D)
        Text {
            anchors.bottom: speedValue.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 10
            text: rootDashboard.gear
            color: "#00E5FF"
            font.pixelSize: 42
            font.bold: true
        }
    }

    // Dynamic Power / Regen Bar (Bottom Center)
    Rectangle {
        id: powerBarContainer
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        width: 600
        height: 24
        radius: 12
        color: "#182030"

        Rectangle {
            id: powerFill
            height: parent.height
            radius: 12
            width: Math.min(300, (Math.abs(rootDashboard.powerKw) / 100.0) * 300)
            anchors.verticalCenter: parent.verticalCenter
            x: rootDashboard.powerKw >= 0 ? 300 : (300 - width)
            color: rootDashboard.powerKw >= 0 ? "#00E676" : "#00B0FF"
        }

        Text {
            anchors.top: parent.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            text: (rootDashboard.powerKw >= 0 ? "+" : "") + rootDashboard.powerKw.toFixed(1) + " kW"
            color: rootDashboard.powerKw >= 0 ? "#00E676" : "#00B0FF"
            font.pixelSize: 18
            font.bold: true
        }
    }

    // Left Gauge: Battery SOC & Range
    Item {
        anchors.left: parent.left
        anchors.leftMargin: 80
        anchors.verticalCenter: parent.verticalCenter
        width: 250
        height: 250

        Text {
            text: Math.round(rootDashboard.socPct) + "%"
            color: rootDashboard.socPct < 20 ? "#FF5252" : "#00E676"
            font.pixelSize: 52
            font.bold: true
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 65
            text: "BATARYA DOLULUĞU"
            color: "#6C7A9C"
            font.pixelSize: 14
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 110
            text: rootDashboard.remainingRangeKm + " km"
            color: "#FFFFFF"
            font.pixelSize: 38
            font.bold: true
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 155
            text: "KALAN TAHMİNİ MENZİL"
            color: "#6C7A9C"
            font.pixelSize: 14
        }
    }

    // Right Gauge: Thermal & Powertrain Status
    Item {
        anchors.right: parent.right
        anchors.rightMargin: 80
        anchors.verticalCenter: parent.verticalCenter
        width: 250
        height: 250

        Text {
            text: rootDashboard.motorTempC + "°C"
            color: rootDashboard.motorTempC > 65 ? "#FF5252" : "#E2E8F0"
            font.pixelSize: 42
            font.bold: true
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 50
            text: "MOTOR SICAKLIĞI"
            color: "#6C7A9C"
            font.pixelSize: 14
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 100
            text: rootDashboard.battTempC + "°C"
            color: rootDashboard.battTempC > 50 ? "#FF5252" : "#E2E8F0"
            font.pixelSize: 42
            font.bold: true
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 150
            text: "BATARYA PAKETİ"
            color: "#6C7A9C"
            font.pixelSize: 14
        }
    }
}
