"""
telemetry_datalogger.py
=======================
High-reliability Black Box Data Logger for Electric Vehicle Telemetry.
Logs vehicle state parameters at 10 Hz into an SQLite database with WAL mode
(power-cut resilience) and provides automated daily CSV export.
"""

import sqlite3
import time
import os
import csv
from typing import Dict, Any

class DataLogger:
    def __init__(self, db_path: str = "blackbox_telemetry.db", csv_dir: str = "logs_csv"):
        self.db_path = db_path
        self.csv_dir = csv_dir
        os.makedirs(self.csv_dir, exist_ok=True)
        self._init_sqlite()

    def _init_sqlite(self):
        """Initializes SQLite with Write-Ahead Logging (WAL) for power loss protection."""
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("PRAGMA journal_mode=WAL;")
            cursor.execute("PRAGMA synchronous=NORMAL;")
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS vehicle_telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp REAL NOT NULL,
                    iso_time TEXT NOT NULL,
                    vcu_state TEXT,
                    drive_mode TEXT,
                    speed_rpm INTEGER,
                    vehicle_speed_kmh REAL,
                    target_torque_nm REAL,
                    actual_torque_nm REAL,
                    pack_voltage_v REAL,
                    pack_current_a REAL,
                    soc_pct REAL,
                    soh_pct REAL,
                    min_cell_mv INTEGER,
                    max_cell_mv INTEGER,
                    cell_delta_mv INTEGER,
                    motor_temp_c INTEGER,
                    inverter_temp_c INTEGER,
                    battery_temp_c INTEGER,
                    imd_insulation_kohm INTEGER,
                    slip_ratio_pct REAL,
                    abs_active INTEGER,
                    tcs_active INTEGER,
                    cooling_pump_pct INTEGER,
                    radiator_fan_pct INTEGER,
                    latitude REAL,
                    longitude REAL,
                    gps_speed_kmh REAL
                );
            """)
            conn.commit()

    def log_snapshot(self, snapshot: Dict[str, Any]):
        """Inserts a single 10Hz unified snapshot into the database."""
        now = time.time()
        iso = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(now))

        speed_rpm = snapshot.get("motor_speed_rpm", 0)
        # 12000 RPM at 1:7.5 gear ratio with 0.3m wheel radius -> ~180 km/h
        veh_speed_kmh = round((speed_rpm / 7.5) * (2 * 3.14159 * 0.3) * (60.0 / 1000.0), 1)

        row = (
            now,
            iso,
            snapshot.get("vcu_state", "STANDBY"),
            snapshot.get("drive_mode", "NORMAL"),
            speed_rpm,
            veh_speed_kmh,
            snapshot.get("target_torque_nm", 0.0),
            snapshot.get("actual_torque_nm", 0.0),
            snapshot.get("pack_voltage_v", 100.0),
            snapshot.get("pack_current_a", 0.0),
            snapshot.get("soc_pct", 100.0),
            snapshot.get("soh_pct", 100.0),
            snapshot.get("cell_min_mv", 3700),
            snapshot.get("cell_max_mv", 3700),
            snapshot.get("cell_delta_mv", 0),
            snapshot.get("motor_temp_c", 25),
            snapshot.get("inverter_temp_c", 25),
            snapshot.get("temp_max_c", 25),
            snapshot.get("insulation_resistance_kohm", 2500),
            snapshot.get("slip_ratio_pct", 0.0),
            1 if snapshot.get("abs_active", False) else 0,
            1 if snapshot.get("tcs_active", False) else 0,
            snapshot.get("cooling_pump_pwm_pct", 0),
            snapshot.get("radiator_fan_pwm_pct", 0),
            snapshot.get("latitude", 41.0082),
            snapshot.get("longitude", 28.9784),
            snapshot.get("gps_speed_kmh", 0.0)
        )

        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("""
                INSERT INTO vehicle_telemetry (
                    timestamp, iso_time, vcu_state, drive_mode, speed_rpm, vehicle_speed_kmh,
                    target_torque_nm, actual_torque_nm, pack_voltage_v, pack_current_a,
                    soc_pct, soh_pct, min_cell_mv, max_cell_mv, cell_delta_mv,
                    motor_temp_c, inverter_temp_c, battery_temp_c, imd_insulation_kohm,
                    slip_ratio_pct, abs_active, tcs_active, cooling_pump_pct,
                    radiator_fan_pct, latitude, longitude, gps_speed_kmh
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, row)
            conn.commit()

    def export_to_csv(self, filename: str = None) -> str:
        """Exports all logged database records to a CSV file."""
        if not filename:
            filename = f"ev_blackbox_{time.strftime('%Y%m%d_%H%M%S')}.csv"
        file_path = os.path.join(self.csv_dir, filename)

        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM vehicle_telemetry ORDER BY id ASC")
            rows = cursor.fetchall()
            headers = [desc[0] for desc in cursor.description]

            with open(file_path, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(headers)
                writer.writerows(rows)

        return file_path
