"""
dashboard_ui.py
===============
Instrument Cluster Logic and Gauge Data Formatter.
Calculates vehicle metrics such as dynamic range, instantaneous power (kW),
efficiency, and renderable gauge parameters.
"""

from typing import Dict, Any

class DashboardUI:
    @staticmethod
    def process_dashboard_state(telemetry: Dict[str, Any]) -> Dict[str, Any]:
        """Translates raw telemetry into styled dashboard presentation variables."""
        speed_rpm = telemetry.get("motor_speed_rpm", 0)
        # Gear ratio 7.5, wheel radius 0.3m
        speed_kmh = max(0.0, round((abs(speed_rpm) / 7.5) * (2 * 3.14159 * 0.3) * (60.0 / 1000.0), 1))

        pack_v = telemetry.get("pack_voltage_v", 100.0)
        pack_i = telemetry.get("pack_current_a", 0.0)
        power_kw = round((pack_v * pack_i) / 1000.0, 2)

        soc_pct = telemetry.get("soc_pct", 100.0)
        # Assuming 50 Ah nominal pack at ~100V = 5.0 kWh capacity
        nominal_kwh = 5.4
        remaining_kwh = (soc_pct / 100.0) * nominal_kwh
        # Typical EV consumption: 135 Wh/km
        estimated_range_km = int((remaining_kwh * 1000.0) / 135.0)

        # Drive Gear Selection
        vcu_state = telemetry.get("vcu_state", "STANDBY")
        if vcu_state == "READY_TO_DRIVE":
            gear = "D"
        elif vcu_state == "STANDBY":
            gear = "P"
        elif vcu_state == "FAULT":
            gear = "!"
        else:
            gear = "N"

        return {
            "speed_kmh": speed_kmh,
            "speed_rpm": speed_rpm,
            "power_kw": power_kw,
            "is_regenerating": (power_kw < -0.1),
            "soc_pct": round(soc_pct, 1),
            "estimated_range_km": estimated_range_km,
            "pack_voltage_v": round(pack_v, 1),
            "pack_current_a": round(pack_i, 1),
            "gear": gear,
            "drive_mode": telemetry.get("drive_mode", "NORMAL"),
            "vcu_state": vcu_state,
            "cooling_pump_pct": telemetry.get("cooling_pump_pwm_pct", 0),
            "radiator_fan_pct": telemetry.get("radiator_fan_pwm_pct", 0),
            "motor_temp_c": telemetry.get("motor_temp_c", 25),
            "inverter_temp_c": telemetry.get("inverter_temp_c", 25),
            "battery_temp_c": telemetry.get("temp_max_c", 25),
            "insulation_kohm": telemetry.get("insulation_resistance_kohm", 2500)
        }
