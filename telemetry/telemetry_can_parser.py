"""
telemetry_can_parser.py
======================
Decodes raw CAN frames based on the vehicle CAN matrix (DBC) into structured,
human-readable engineering telemetry dictionaries. Supports both python-can
interfaces (SocketCAN, PCAN, MCP2515) and Virtual in-memory queues for simulation.
"""

import struct
from typing import Dict, Any, Optional

class CANParser:
    """Parses binary 8-byte CAN payloads according to ev_network.dbc definitions."""

    @staticmethod
    def parse_frame(can_id: int, data: bytes) -> Optional[Dict[str, Any]]:
        """Decodes an incoming CAN frame into telemetry fields."""
        if not data or len(data) < 8:
            # Pad to 8 bytes if needed
            data = data.ljust(8, b'\x00')

        try:
            if can_id == 0x100:  # VCU_Heartbeat_State
                state, fault_code, drive_mode, counter, flags = struct.unpack("<BHBBB2x", data)
                drive_modes = {0: "ECO", 1: "NORMAL", 2: "SPORT"}
                states = {0: "INIT", 1: "STANDBY", 2: "PRECHARGE", 3: "READY_TO_DRIVE", 4: "FAULT"}
                return {
                    "msg_type": "VCU_Heartbeat_State",
                    "can_id": hex(can_id),
                    "vcu_state": states.get(state, f"UNKNOWN({state})"),
                    "vcu_fault_code": fault_code,
                    "drive_mode": drive_modes.get(drive_mode, "NORMAL"),
                    "rolling_counter": counter,
                    "brake_pressed": bool(flags & 0x01),
                    "ready_to_drive": bool(flags & 0x02)
                }

            elif can_id == 0x101:  # VCU_Torque_Command
                target_torque_raw, flags, max_speed = struct.unpack("<hBBH2x", data)
                return {
                    "msg_type": "VCU_Torque_Command",
                    "can_id": hex(can_id),
                    "target_torque_nm": round(target_torque_raw * 0.1, 1),
                    "inverter_enable": bool(flags & 0x01),
                    "direction_forward": bool(flags & 0x02),
                    "regen_allowed": bool(flags & 0x04),
                    "max_speed_rpm": max_speed
                }

            elif can_id == 0x102:  # VCU_Aux_Status
                pump_pwm, fan_pwm, aux_flags = struct.unpack("<BBB5x", data)
                return {
                    "msg_type": "VCU_Aux_Status",
                    "can_id": hex(can_id),
                    "cooling_pump_pwm_pct": pump_pwm,
                    "radiator_fan_pwm_pct": fan_pwm,
                    "headlights": bool(aux_flags & (1 << 0)),
                    "brake_lights": bool(aux_flags & (1 << 1)),
                    "precharge_relay": bool(aux_flags & (1 << 2)),
                    "main_pos_contactor": bool(aux_flags & (1 << 3)),
                    "main_neg_contactor": bool(aux_flags & (1 << 4))
                }

            elif can_id == 0x200:  # BMS_Pack_Status
                pack_v_raw, pack_i_raw, soc_raw, soh_raw, flags = struct.unpack("<HhBBBx", data)
                return {
                    "msg_type": "BMS_Pack_Status",
                    "can_id": hex(can_id),
                    "pack_voltage_v": round(pack_v_raw * 0.1, 1),
                    "pack_current_a": round(pack_i_raw * 0.1, 1),
                    "soc_pct": round(soc_raw * 0.5, 1),
                    "soh_pct": round(soh_raw * 0.5, 1),
                    "bms_contactor_allow": bool(flags & 0x01),
                    "fault_level": (flags >> 1) & 0x07
                }

            elif can_id == 0x201:  # BMS_Limits_Alarms
                max_disch_raw, max_chg_raw, alarms = struct.unpack("<HHB3x", data)
                return {
                    "msg_type": "BMS_Limits_Alarms",
                    "can_id": hex(can_id),
                    "max_discharge_current_a": round(max_disch_raw * 0.1, 1),
                    "max_charge_current_a": round(max_chg_raw * 0.1, 1),
                    "alarm_over_voltage": bool(alarms & 0x01),
                    "alarm_under_voltage": bool(alarms & 0x02),
                    "alarm_over_temp": bool(alarms & 0x04),
                    "alarm_under_temp": bool(alarms & 0x08),
                    "balancing_active": bool(alarms & 0x10)
                }

            elif can_id == 0x202:  # BMS_Cell_Extremes
                min_mv, max_mv, min_t, max_t, min_id, max_id = struct.unpack("<HHbbBB", data)
                return {
                    "msg_type": "BMS_Cell_Extremes",
                    "can_id": hex(can_id),
                    "cell_min_mv": min_mv,
                    "cell_max_mv": max_mv,
                    "cell_delta_mv": max_mv - min_mv,
                    "temp_min_c": min_t,
                    "temp_max_c": max_t,
                    "cell_min_id": min_id,
                    "cell_max_id": max_id
                }

            elif can_id == 0x300:  # INV_Motor_Feedback
                rpm, torque_raw, dc_v_raw, dc_i_raw = struct.unpack("<hhhh", data)
                return {
                    "msg_type": "INV_Motor_Feedback",
                    "can_id": hex(can_id),
                    "motor_speed_rpm": rpm,
                    "actual_torque_nm": round(torque_raw * 0.1, 1),
                    "dc_bus_voltage_v": round(dc_v_raw * 0.1, 1),
                    "dc_bus_current_a": round(dc_i_raw * 0.1, 1)
                }

            elif can_id == 0x301:  # INV_Thermal_Status
                inv_temp, mot_temp, faults, derating = struct.unpack("<bbHB3x", data)
                return {
                    "msg_type": "INV_Thermal_Status",
                    "can_id": hex(can_id),
                    "inverter_temp_c": inv_temp,
                    "motor_temp_c": mot_temp,
                    "inverter_fault_flags": faults,
                    "derating_active": bool(derating)
                }

            elif can_id == 0x302:  # ABS_TCS_Status
                slip_raw, flags, t_cut = struct.unpack("<BBB5x", data)
                return {
                    "msg_type": "ABS_TCS_Status",
                    "can_id": hex(can_id),
                    "slip_ratio_pct": round(slip_raw * 0.5, 1),
                    "abs_active": bool(flags & 0x01),
                    "tcs_active": bool(flags & 0x02),
                    "torque_reduction_pct": t_cut
                }

            elif can_id == 0x400:  # OBC_Status_Control
                ac_present, pilot_limit, v_out_raw, i_out_raw, state = struct.unpack("<BBHHBx", data)
                obc_states = {0: "IDLE", 1: "WAIT_PILOT", 2: "PRECHARGE", 3: "CC", 4: "CV", 5: "COMPLETE", 6: "FAULT"}
                return {
                    "msg_type": "OBC_Status_Control",
                    "can_id": hex(can_id),
                    "ac_plug_present": bool(ac_present),
                    "pilot_current_limit_a": pilot_limit,
                    "obc_output_v": round(v_out_raw * 0.1, 1),
                    "obc_output_a": round(i_out_raw * 0.1, 1),
                    "charging_state": obc_states.get(state, "UNKNOWN")
                }

            elif can_id == 0x450:  # IMD_Status
                r_kohm, status_flag, fault = struct.unpack("<HBB4x", data)
                return {
                    "msg_type": "IMD_Status",
                    "can_id": hex(can_id),
                    "insulation_resistance_kohm": r_kohm,
                    "imd_status_flag": status_flag,
                    "isolation_fault": bool(fault)
                }

        except Exception as e:
            return {"error": str(e), "can_id": hex(can_id), "raw": data.hex()}

        return None
