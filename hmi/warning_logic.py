"""
warning_logic.py
================
Evaluates EV system health and determines which ISO-standard Telltale warning
icons and error banners to display on the HMI Instrument Cluster.
"""

from typing import Dict, Any, List

class WarningLogic:
    @staticmethod
    def evaluate_warnings(telemetry: Dict[str, Any]) -> List[Dict[str, str]]:
        warnings = []

        # 1. Critical IMD Isolation Fault (< 100 kOhm)
        r_kohm = telemetry.get("insulation_resistance_kohm", 2500)
        if r_kohm < 100 or telemetry.get("isolation_fault", False):
            warnings.append({
                "severity": "CRITICAL",
                "code": "ISO_FAULT",
                "icon": "⚡",
                "color": "#FF2A2A",
                "message": f"İzolasyon Arızası! ({r_kohm} kΩ < 100 kΩ) - HV Kapatıldı"
            })

        # 2. Battery Over-Temperature (> 55°C Warning, > 60°C Critical)
        bms_temp = telemetry.get("temp_max_c", 25)
        if bms_temp >= 60:
            warnings.append({
                "severity": "CRITICAL",
                "code": "BATT_HOT",
                "icon": "🔥",
                "color": "#FF2A2A",
                "message": f"Batarya Kritik Aşırı Sıcaklık! ({bms_temp}°C)"
            })
        elif bms_temp >= 50:
            warnings.append({
                "severity": "WARNING",
                "code": "BATT_WARM",
                "icon": "🌡️",
                "color": "#FFA500",
                "message": f"Batarya Sıcaklığı Yüksek ({bms_temp}°C) - Güç Kısıtlandı"
            })

        # 3. Cell Under-Voltage (< 2.8V warning, < 2.5V critical)
        min_cell_mv = telemetry.get("cell_min_mv", 3700)
        if min_cell_mv <= 2500:
            warnings.append({
                "severity": "CRITICAL",
                "code": "CELL_UV",
                "icon": "🪫",
                "color": "#FF2A2A",
                "message": f"Kritik Düşük Hücre Gerilimi! ({min_cell_mv} mV)"
            })
        elif min_cell_mv <= 3000:
            warnings.append({
                "severity": "WARNING",
                "code": "BATT_LOW",
                "icon": "⚠️",
                "color": "#FFA500",
                "message": "Batarya Şarjı Düşük! En Yakın Şarj İstasyonuna Gidin"
            })

        # 4. Inverter & Motor Thermal
        inv_temp = telemetry.get("inverter_temp_c", 25)
        mot_temp = telemetry.get("motor_temp_c", 25)
        if inv_temp >= 75 or mot_temp >= 90:
            warnings.append({
                "severity": "WARNING",
                "code": "POWERTRAIN_HOT",
                "icon": "⚙️",
                "color": "#FFA500",
                "message": f"Motor/İnvertör Isındı (Motor: {mot_temp}°C, İnv: {inv_temp}°C)"
            })

        # 5. Active Safety Systems (ABS / TCS)
        if telemetry.get("abs_active", False):
            warnings.append({
                "severity": "INFO",
                "code": "ABS_ACTIVE",
                "icon": "🛑",
                "color": "#00E5FF",
                "message": "ABS Devrede (Rejeneratif Frenleme Darbeleniyor)"
            })
        if telemetry.get("tcs_active", False):
            warnings.append({
                "severity": "INFO",
                "code": "TCS_ACTIVE",
                "icon": "🏎️",
                "color": "#00E5FF",
                "message": "TCS Devrede (Patinaj Önleme Tork Kısıyor)"
            })

        # 6. Precharge & System State Fault
        vcu_state = telemetry.get("vcu_state", "STANDBY")
        if vcu_state == "FAULT":
            warnings.append({
                "severity": "CRITICAL",
                "code": "VCU_FAULT",
                "icon": "🚨",
                "color": "#FF0000",
                "message": f"VCU Hata Modunda! (Hata Kodu: 0x{telemetry.get('vcu_fault_code', 0):04X})"
            })

        return warnings
