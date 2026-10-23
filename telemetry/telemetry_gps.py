"""
telemetry_gps.py
================
GPS NMEA-0183 Protocol Parser ($GPRMC and $GPGGA sentences) over UART.
Includes a simulation fallback generator for testing telemetry without hardware.
"""

from typing import Dict, Any, Optional
import math
import time

class GPSParser:
    def __init__(self, port: Optional[str] = None, baudrate: int = 9600):
        self.port = port
        self.baudrate = baudrate
        self.latitude = 41.0082   # Default: Istanbul Latitude
        self.longitude = 28.9784  # Default: Istanbul Longitude
        self.altitude_m = 45.0
        self.speed_kmh = 0.0
        self.heading_deg = 0.0
        self.satellites = 8
        self.fix_valid = True
        self.sim_time = 0.0

    @staticmethod
    def parse_nmea_sentence(sentence: str) -> Optional[Dict[str, Any]]:
        """Parses standard $GPRMC or $GPGGA sentences."""
        if not sentence or not sentence.startswith("$"):
            return None

        parts = sentence.strip().split("*")[0].split(",")
        cmd = parts[0]

        if cmd in ("$GPRMC", "$GNRMC"):
            # $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,,,a*hh
            if len(parts) >= 10 and parts[2] == 'A':  # Valid fix
                raw_lat = parts[3]
                lat_dir = parts[4]
                raw_lon = parts[5]
                lon_dir = parts[6]
                speed_knots = float(parts[7]) if parts[7] else 0.0
                heading = float(parts[8]) if parts[8] else 0.0

                # Convert ddmm.mmmm to decimal degrees
                lat = float(raw_lat[:2]) + float(raw_lat[2:]) / 60.0
                if lat_dir == 'S': lat = -lat

                lon = float(raw_lon[:3]) + float(raw_lon[3:]) / 60.0
                if lon_dir == 'W': lon = -lon

                return {
                    "valid": True,
                    "latitude": round(lat, 6),
                    "longitude": round(lon, 6),
                    "speed_kmh": round(speed_knots * 1.852, 1),
                    "heading_deg": round(heading, 1)
                }

        elif cmd in ("$GPGGA", "$GNGGA"):
            # $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
            if len(parts) >= 10 and parts[6] != '0':
                satellites = int(parts[7]) if parts[7] else 0
                alt = float(parts[9]) if parts[9] else 0.0
                return {
                    "valid": True,
                    "satellites": satellites,
                    "altitude_m": alt
                }

        return None

    def update_simulated_track(self, vehicle_speed_kmh: float, dt_sec: float = 0.1) -> Dict[str, Any]:
        """Simulates GPS position advancing along a circular test track."""
        self.speed_kmh = vehicle_speed_kmh
        self.sim_time += dt_sec

        speed_mps = vehicle_speed_kmh / 3.6
        distance_m = speed_mps * dt_sec

        # Advance heading
        self.heading_deg = (self.heading_deg + (distance_m / 20.0) * 5.0) % 360.0
        rad = math.radians(self.heading_deg)

        # Delta degrees (1 degree lat ~= 111,000 meters)
        delta_lat = (distance_m * math.cos(rad)) / 111000.0
        delta_lon = (distance_m * math.sin(rad)) / (111000.0 * math.cos(math.radians(self.latitude)))

        self.latitude += delta_lat
        self.longitude += delta_lon

        return {
            "latitude": round(self.latitude, 6),
            "longitude": round(self.longitude, 6),
            "altitude_m": self.altitude_m,
            "speed_kmh": round(self.speed_kmh, 1),
            "heading_deg": round(self.heading_deg, 1),
            "satellites": self.satellites,
            "fix_valid": self.fix_valid
        }
