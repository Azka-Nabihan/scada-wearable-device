#!/usr/bin/env python3
"""
SCaDA Serial Receiver Logger
Membaca stream JSON dari Base Station (ESP32 LoRa Receiver) via port serial USB,
menampilkan data telemetri di terminal, dan mencatatnya ke file CSV untuk analisis jangkauan & sinyal.
"""

import sys
import time
import json
import csv
import argparse
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] Modul 'pyserial' belum terinstal.")
    print("Silakan jalankan: pip install pyserial")
    sys.exit(1)


def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]


def main():
    parser = argparse.ArgumentParser(description="SCaDA Base Station Serial Receiver")
    parser.add_argument("-p", "--port", type=str, default=None, help="Nama COM Port (contoh: COM3 atau /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-o", "--output", type=str, default=None, help="Nama file CSV output log")
    args = parser.parse_args()

    port = args.port
    if not port:
        available_ports = list_serial_ports()
        if not available_ports:
            print("[ERROR] Tidak ada port serial yang terdeteksi. Hubungkan Base Station ESP32 ke USB.")
            sys.exit(1)
        port = available_ports[0]
        print(f"[INFO] Port otomatis dipilih: {port} (dari {available_ports})")

    csv_filename = args.output or f"lora_telemetry_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    csv_header = ["timestamp", "node_id", "hr", "spo2", "temp", "rr", "pass", "risk", "rssi", "snr"]

    print(f"[INFO] Menghubungkan ke {port} pada {args.baud} baud...")
    print(f"[INFO] Menyimpan log telemetri ke: {csv_filename}")
    print("[INFO] Tekan Ctrl+C untuk menghentikan pemantauan.\n")

    try:
        ser = serial.Serial(port, args.baud, timeout=1.0)
    except Exception as e:
        print(f"[ERROR] Gagal membuka port {port}: {e}")
        sys.exit(1)

    packet_count = 0

    with open(csv_filename, mode="w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(csv_header)
        csv_file.flush()

        try:
            while True:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                # Cek jika baris berupa JSON telemetri
                if line.startswith("{") and line.endswith("}"):
                    try:
                        data = json.loads(line)
                        if data.get("type") == "telemetry":
                            now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                            packet_count += 1

                            node_id = data.get("node_id", "-")
                            hr = data.get("hr", "-")
                            spo2 = data.get("spo2", "-")
                            temp = data.get("temp", "-")
                            rr = data.get("rr", "-")
                            pass_st = data.get("pass", "-")
                            risk = data.get("risk", "-")
                            rssi = data.get("rssi", "-")
                            snr = data.get("snr", "-")

                            # Cetak format ringkas di terminal
                            print(f"[{now_str}] #{packet_count:04d} | Node:{node_id} | HR:{hr} bpm | SpO2:{spo2}% | Temp:{temp}C | PASS:{pass_st} | RSSI:{rssi} dBm | SNR:{snr} dB")

                            # Simpan ke CSV
                            writer.writerow([now_str, node_id, hr, spo2, temp, rr, pass_st, risk, rssi, snr])
                            csv_file.flush()
                        else:
                            print(f"[LOG] {line}")
                    except json.JSONDecodeError:
                        print(f"[RAW] {line}")
                else:
                    print(f"[RAW] {line}")

        except KeyboardInterrupt:
            print(f"\n[INFO] Pemantauan dihentikan. Total paket diterima: {packet_count}")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
