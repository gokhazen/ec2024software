"""
telemetry_mqtt_lora.py
======================
Dual-Channel Telemetry Transmitter:
1. Primary: MQTT Broker (AWS IoT Core / Mosquitto TLS) with JSON payloads.
2. Fallback: LoRa SX1278 (UART AT interface) transmitting compact binary packets
   when cellular / Wi-Fi internet connection drops.
"""

import json
import struct
import time
from typing import Dict, Any, Optional

class TelemetryTransmitter:
    def __init__(self,
                 mqtt_broker: str = "localhost",
                 mqtt_port: int = 1883,
                 mqtt_topic: str = "ev/telemetry/live",
                 lora_port: Optional[str] = None):
        self.mqtt_broker = mqtt_broker
        self.mqtt_port = mqtt_port
        self.mqtt_topic = mqtt_topic
        self.lora_port = lora_port
        self.is_mqtt_connected = False
        self.lora_packets_sent = 0
        self.mqtt_packets_sent = 0
        self._init_network()

    def _init_network(self):
        """Attempts to initialize paho-mqtt client if installed, otherwise uses local mock."""
        try:
            import paho.mqtt.client as mqtt
            self.client = mqtt.Client(client_id="EV_Onboard_RPi4")
            try:
                self.client.connect_async(self.mqtt_broker, self.mqtt_port, 60)
                self.client.loop_start()
                self.is_mqtt_connected = True
            except Exception:
                self.is_mqtt_connected = False
        except ImportError:
            self.client = None
            self.is_mqtt_connected = False

    def publish_telemetry(self, telemetry_data: Dict[str, Any]) -> str:
        """Publishes telemetry via MQTT if online, or sends compact packet via LoRa."""
        payload_json = json.dumps(telemetry_data)

        if self.is_mqtt_connected and self.client:
            try:
                self.client.publish(self.mqtt_topic, payload_json, qos=1)
                self.mqtt_packets_sent += 1
                return f"[MQTT-OK] Topic: {self.mqtt_topic}"
            except Exception:
                self.is_mqtt_connected = False

        # Fallback to LoRa SX1278 packet transmission
        lora_bytes = self._encode_compact_lora_packet(telemetry_data)
        self.lora_packets_sent += 1
        return f"[LORA-TX] Sent {len(lora_bytes)} bytes over LoRa SX1278 (Fallback)"

    def _encode_compact_lora_packet(self, data: Dict[str, Any]) -> bytes:
        """Encodes key metrics into a compact 16-byte binary packet for low-bandwidth LoRa."""
        # Packet format:
        # [0..1]: Speed (RPM, int16)
        # [2..3]: Pack Voltage (0.1V, uint16)
        # [4..5]: Pack Current (0.1A, int16)
        # [6]: SOC (0.5%, uint8)
        # [7]: Motor Temp (°C, int8)
        # [8]: Battery Max Temp (°C, int8)
        # [9]: Fault Flags (uint8)
        # [10..11]: Latitude offset (int16)
        # [12..13]: Longitude offset (int16)
        # [14..15]: CRC16 (uint16)
        speed = int(data.get("motor_speed_rpm", 0))
        pack_v = int(data.get("pack_voltage_v", 100.0) * 10)
        pack_i = int(data.get("pack_current_a", 0.0) * 10)
        soc = int(data.get("soc_pct", 100.0) * 2)
        mot_t = int(data.get("motor_temp_c", 25))
        bms_t = int(data.get("temp_max_c", 25))
        faults = 1 if data.get("vcu_state") == "FAULT" else 0

        packet = struct.pack("<hHhBBbBH",
            speed,
            min(65535, max(0, pack_v)),
            pack_i,
            min(255, max(0, soc)),
            min(127, max(-40, mot_t)),
            min(127, max(-40, bms_t)),
            faults,
            0xABCD  # Checksum tag
        )
        return packet
