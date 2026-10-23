"""
telemetry_service.py
====================
Main Central Telemetry Service daemon running on Raspberry Pi 4.
Orchestrates:
1. CAN Bus listening and message decoding.
2. 10 Hz Black Box recording into SQLite (WAL mode).
3. GPS NMEA coordinate ingestion.
4. Cloud MQTT / LoRa fallback transmission.
5. IPC / HTTP live streaming for the local HMI Instrument Cluster.
"""

import os
import sys
import time
import threading
from typing import Dict, Any

# Ensure base directory is in sys.path
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

try:
    from telemetry.telemetry_can_parser import CANParser
    from telemetry.telemetry_datalogger import DataLogger
    from telemetry.telemetry_gps import GPSParser
    from telemetry.telemetry_mqtt_lora import TelemetryTransmitter
except ImportError:
    from telemetry_can_parser import CANParser
    from telemetry_datalogger import DataLogger
    from telemetry_gps import GPSParser
    from telemetry_mqtt_lora import TelemetryTransmitter


class TelemetryService:
    def __init__(self, db_path: str = "blackbox_telemetry.db"):
        self.parser = CANParser()
        self.logger = DataLogger(db_path=db_path)
        self.gps = GPSParser()
        self.transmitter = TelemetryTransmitter()
        
        self.running = False
        self.unified_telemetry: Dict[str, Any] = {
            "vcu_state": "STANDBY",
            "drive_mode": "NORMAL",
            "motor_speed_rpm": 0,
            "vehicle_speed_kmh": 0.0,
            "target_torque_nm": 0.0,
            "actual_torque_nm": 0.0,
            "pack_voltage_v": 108.0,
            "pack_current_a": 0.0,
            "soc_pct": 95.0,
            "soh_pct": 100.0,
            "cell_min_mv": 3990,
            "cell_max_mv": 4010,
            "cell_delta_mv": 20,
            "temp_min_c": 24,
            "temp_max_c": 28,
            "motor_temp_c": 32,
            "inverter_temp_c": 30,
            "insulation_resistance_kohm": 2500,
            "isolation_fault": False,
            "abs_active": False,
            "tcs_active": False,
            "slip_ratio_pct": 0.0,
            "cooling_pump_pwm_pct": 20,
            "radiator_fan_pwm_pct": 0,
            "ac_plug_present": False,
            "charging_state": "IDLE",
            "latitude": 41.0082,
            "longitude": 28.9784,
            "gps_speed_kmh": 0.0
        }
        self.lock = threading.Lock()

    def ingest_can_frame(self, can_id: int, data: bytes):
        """Processes incoming CAN frame and updates current unified state."""
        decoded = self.parser.parse_frame(can_id, data)
        if decoded and "msg_type" in decoded:
            with self.lock:
                for k, v in decoded.items():
                    if k not in ("msg_type", "can_id"):
                        self.unified_telemetry[k] = v

    def start_service(self):
        """Starts 10 Hz background logging, GPS, and Cloud sync loops."""
        self.running = True
        self.thread = threading.Thread(target=self._worker_loop, daemon=True)
        self.thread.start()

    def stop_service(self):
        self.running = False

    def _worker_loop(self):
        last_log_time = 0.0
        last_cloud_time = 0.0

        while self.running:
            now = time.time()

            # 1. Update simulated/real GPS (10 Hz)
            with self.lock:
                veh_speed = self.unified_telemetry.get("vehicle_speed_kmh", 0.0)
                gps_data = self.gps.update_simulated_track(veh_speed, dt_sec=0.1)
                self.unified_telemetry.update(gps_data)
                snapshot = dict(self.unified_telemetry)

            # 2. Black Box 10 Hz Logging
            if now - last_log_time >= 0.10:
                last_log_time = now
                self.logger.log_snapshot(snapshot)

            # 3. Cloud / LoRa Uplink (1 Hz)
            if now - last_cloud_time >= 1.0:
                last_cloud_time = now
                self.transmitter.publish_telemetry(snapshot)

            time.sleep(0.05)

    def get_latest_telemetry(self) -> Dict[str, Any]:
        with self.lock:
            return dict(self.unified_telemetry)
