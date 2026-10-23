"""
charging_screen.py
==================
Dedicated charging mode screen presentation logic.
Triggers automatically when the EV is plugged into AC mains / Level 2 EVSE.
"""

from typing import Dict, Any

class ChargingScreenUI:
    @staticmethod
    def process_charging_state(telemetry: Dict[str, Any]) -> Dict[str, Any]:
        """Calculates charging animation, rate in kW, and time to full."""
        charging_state = telemetry.get("charging_state", "IDLE")
        ac_plug_present = telemetry.get("ac_plug_present", False)
        
        is_charging = ac_plug_present or (charging_state in ("CC", "CV", "PRECHARGE"))

        v_out = telemetry.get("obc_output_v", telemetry.get("pack_voltage_v", 100.0))
        i_out = telemetry.get("obc_output_a", abs(telemetry.get("pack_current_a", 0.0)))
        charge_power_kw = round((v_out * i_out) / 1000.0, 2)

        soc = telemetry.get("soc_pct", 50.0)
        remaining_pct = max(0.0, 100.0 - soc)
        
        # 5.4 kWh nominal pack. If charging at charge_power_kw:
        time_to_full_min = 0
        if charge_power_kw > 0.2:
            energy_needed_kwh = (remaining_pct / 100.0) * 5.4
            hours_needed = energy_needed_kwh / charge_power_kw
            time_to_full_min = int(hours_needed * 60)

        return {
            "is_charging_active": is_charging,
            "charging_state": charging_state,
            "charge_power_kw": charge_power_kw,
            "charge_voltage_v": round(v_out, 1),
            "charge_current_a": round(i_out, 1),
            "pilot_current_limit_a": telemetry.get("pilot_current_limit_a", 16),
            "soc_pct": round(soc, 1),
            "time_to_full_minutes": time_to_full_min,
            "battery_temp_c": telemetry.get("temp_max_c", 25)
        }
