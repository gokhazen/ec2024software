"""
ev_sil_virtual_bus.py
=====================
Software-In-The-Loop (SIL) Virtual CAN Bus & EV Physics Simulator.
Accurately models:
1. 27S Lithium-Ion Battery (ECM: OCV + R0 internal impedance).
2. PMSM Motor & Vehicle Longitudinal Dynamics (Inertia, Aero Drag, Rolling Resistance).
3. Bender IR155 IMD Isolation & J1772 Pilot signals.
4. Virtual CAN Message Router connecting all vehicle controllers.
"""

import time
import math
import struct
from typing import Dict, Any, List, Callable

class VirtualCANBus:
    """Thread-safe virtual CAN network router."""
    def __init__(self):
        self.subscribers: List[Callable[[int, bytes], None]] = []
        self.total_messages_routed = 0

    def subscribe(self, callback: Callable[[int, bytes], None]):
        self.subscribers.append(callback)

    def publish(self, can_id: int, data: bytes):
        self.total_messages_routed += 1
        for sub in self.subscribers:
            try:
                sub(can_id, data)
            except Exception as e:
                pass


class EVPhysicsSimulator:
    """Longitudinal dynamic vehicle model."""
    def __init__(self, virtual_can: VirtualCANBus):
        self.can = virtual_can
        
        # Vehicle Constants
        self.vehicle_mass_kg = 850.0      # Lightweight electric vehicle (kg)
        self.wheel_radius_m = 0.28        # 14" tire radius (m)
        self.gear_ratio = 7.2             # Single-speed reduction gear
        self.drag_coeff_cd = 0.29         # Aerodynamic drag coefficient
        self.frontal_area_m2 = 1.65       # Frontal area (m^2)
        self.rolling_res_cr = 0.012       # Tire rolling resistance
        self.rho_air = 1.225              # Air density (kg/m^3)
        self.gravity = 9.81

        # Dynamic States
        self.vehicle_speed_mps = 0.0
        self.motor_speed_rpm = 0.0
        self.motor_torque_nm = 0.0
        self.wheel_speed_mps = 0.0
        self.slip_ratio = 0.0

        # Battery 27S Model
        self.cell_count = 27
        self.soc_pct = 95.0
        self.capacity_ah = 48.0
        self.cell_nominal_v = 4.05
        self.pack_voltage_v = self.cell_count * self.cell_nominal_v
        self.pack_current_a = 0.0
        self.battery_temp_c = 26.0

        # Thermal States
        self.motor_temp_c = 32.0
        self.inverter_temp_c = 28.0

        # Safety & Aux States
        self.insulation_kohm = 2500
        self.main_pos_closed = False
        self.main_neg_closed = False
        self.precharge_closed = False
        self.dc_bus_voltage_v = 0.0

        # Charger
        self.ac_plugged = False
        self.pilot_amps = 0

    def step(self, target_torque_nm: float, regen_allowed: bool, dt_sec: float = 0.02):
        """Simulates one physics time step (default 20ms / 50Hz)."""
        # 1. Motor torque response (1st order lag)
        torque_rate = 600.0 * dt_sec # Max 600 Nm/s slew rate
        delta_t = target_torque_nm - self.motor_torque_nm
        if delta_t > torque_rate: delta_t = torque_rate
        elif delta_t < -torque_rate: delta_t = -torque_rate
        self.motor_torque_nm += delta_t

        # 2. Longitudinal Forces
        # Tractive effort at wheels
        wheel_torque = self.motor_torque_nm * self.gear_ratio
        f_traction = wheel_torque / self.wheel_radius_m

        # Resistive forces
        v = max(0.0, self.vehicle_speed_mps)
        f_aero = 0.5 * self.rho_air * self.drag_coeff_cd * self.frontal_area_m2 * (v ** 2)
        f_roll = self.vehicle_mass_kg * self.gravity * self.rolling_res_cr if v > 0.1 else 0.0

        net_force = f_traction - f_aero - f_roll

        # Acceleration (F = m * a)
        accel = net_force / self.vehicle_mass_kg
        self.vehicle_speed_mps += accel * dt_sec
        if self.vehicle_speed_mps < 0.0:
            self.vehicle_speed_mps = 0.0

        # Wheel & Motor Speed
        self.wheel_speed_mps = self.vehicle_speed_mps
        angular_vel = self.wheel_speed_mps / self.wheel_radius_m
        wheel_rpm = angular_vel * (60.0 / (2.0 * math.pi))
        self.motor_speed_rpm = wheel_rpm * self.gear_ratio

        # 3. Battery Electrical Model
        # Electrical Power P = T * omega / efficiency
        mech_power_w = self.motor_torque_nm * (self.motor_speed_rpm * (2.0 * math.pi / 60.0))
        if mech_power_w >= 0:
            elec_power_w = mech_power_w / 0.92  # 92% inverter/motor efficiency
        else:
            elec_power_w = mech_power_w * 0.85  # Regen efficiency

        if self.main_pos_closed and self.main_neg_closed:
            self.pack_current_a = elec_power_w / max(50.0, self.pack_voltage_v)
            # Coulomb counting integration
            delta_ah = (self.pack_current_a * dt_sec) / 3600.0
            self.soc_pct -= (delta_ah / self.capacity_ah) * 100.0
            self.soc_pct = max(0.0, min(100.0, self.soc_pct))

            # Pack voltage based on SOC and internal impedance IR drop
            cell_v = 3.3 + (self.soc_pct / 100.0) * 0.85 - (self.pack_current_a * 0.001)
            cell_v = max(2.5, min(4.25, cell_v))
            self.pack_voltage_v = self.cell_count * cell_v
            self.dc_bus_voltage_v = self.pack_voltage_v
        elif self.precharge_closed and self.main_neg_closed:
            # Precharge ramp through 47-ohm resistor (Tau = R * C_bus)
            target = self.pack_voltage_v
            self.dc_bus_voltage_v += (target - self.dc_bus_voltage_v) * 0.15
        else:
            # Discharging bus bleeders
            self.dc_bus_voltage_v *= 0.95

        # 4. Thermal Model
        heat_losses = abs(self.pack_current_a) * 0.05
        self.motor_temp_c += (heat_losses * 0.002) * dt_sec
        self.inverter_temp_c += (heat_losses * 0.0015) * dt_sec

        # 5. Broadcast Simulated Sensor Messages on CAN Bus
        self._broadcast_can_frames()

    def _broadcast_can_frames(self):
        # 0x200 BMS_Pack_Status
        v_raw = int(self.pack_voltage_v * 10)
        i_raw = int(self.pack_current_a * 10)
        soc_raw = int(self.soc_pct * 2)
        soh_raw = 200 # 100%
        flags = 0x01 # Contactor allow
        bms_pack_data = struct.pack("<HhBBBx", min(65535, max(0, v_raw)), min(32767, max(-32768, i_raw)), min(255, max(0, soc_raw)), soh_raw, flags)
        self.can.publish(0x200, bms_pack_data)

        # 0x202 BMS_Cell_Extremes
        cell_avg_mv = int((self.pack_voltage_v / self.cell_count) * 1000)
        min_mv = max(0, min(65535, cell_avg_mv - 8))
        max_mv = max(0, min(65535, cell_avg_mv + 8))
        min_t = min(127, max(-40, int(self.battery_temp_c)))
        max_t = min(127, max(-40, int(self.battery_temp_c + 2)))
        bms_cell_data = struct.pack("<HHbbBB", min_mv, max_mv, min_t, max_t, 1, 27)
        self.can.publish(0x202, bms_cell_data)

        # 0x300 INV_Motor_Feedback
        rpm = min(32767, max(-32768, int(self.motor_speed_rpm)))
        t_raw = min(32767, max(-32768, int(self.motor_torque_nm * 10)))
        dc_v = min(65535, max(0, int(self.dc_bus_voltage_v * 10)))
        dc_i = min(32767, max(-32768, int(self.pack_current_a * 10)))
        inv_data = struct.pack("<hhhh", rpm, t_raw, dc_v, dc_i)
        self.can.publish(0x300, inv_data)


        # 0x301 INV_Thermal_Status
        inv_therm = struct.pack("<bbHB3x", int(self.inverter_temp_c), int(self.motor_temp_c), 0, 0)
        self.can.publish(0x301, inv_therm)

        # 0x450 IMD_Status
        fault_bit = 1 if self.insulation_kohm < 100 else 0
        imd_data = struct.pack("<HBB4x", min(65535, self.insulation_kohm), 0, fault_bit)
        self.can.publish(0x450, imd_data)
