# Скрипт занятия 3.1: телеметрия прибора 311-adc.
#
# Без аргументов: запускает поток телеметрии на 10 секунд, сохраняет записи
# в telemetry.csv и строит график telemetry.png.
# --plot <файл.csv>: строит график по готовому CSV, плата не нужна.
# --rc: опыт с RC-цепью — 5 секунд заряда и 5 секунд разряда,
# команды charge_on и charge_off отправляет сам, результат в rc.csv и rc.png.

import argparse
import time

import serial
from serial.tools import list_ports

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

CSV_HEADER = "time_s,voltage_v,temp_c"


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def parse_tm(line):
    # tm: <мкс> <В> <°C>
    parts = line.split()
    if len(parts) == 4 and parts[0] == "tm:":
        try:
            us = int(parts[1])
            voltage = float(parts[2])
            temp = float(parts[3])
            return us, voltage, temp
        except ValueError:
            return None
    return None


def collect(port, seconds):
    """Собирает записи телеметрии seconds секунд, поток уже запущен."""
    rows = []
    deadline = time.monotonic() + seconds
    received = ""
    while time.monotonic() < deadline:
        received += port.read(1024).decode("ascii", "replace")
        while "\n" in received:
            line, received = received.split("\n", 1)
            sample = parse_tm(line.strip())
            if sample is not None:
                rows.append(sample)
    return rows


def write_csv(name, rows):
    with open(name, "w", encoding="utf-8") as csv:
        csv.write(CSV_HEADER + "\n")
        for us, voltage, temp in rows:
            csv.write("%.6f,%.3f,%.1f\n" % (us / 1000000.0, voltage, temp))


def read_csv(name):
    rows = []
    with open(name, encoding="utf-8") as csv:
        header = csv.readline()
        for line in csv:
            parts = line.strip().split(",")
            if len(parts) == 3:
                rows.append((float(parts[0]), float(parts[1]), float(parts[2])))
    return rows


def plot(name_png, rows):
    times = [row[0] for row in rows]
    voltages = [row[1] for row in rows]
    temps = [row[2] for row in rows]

    figure, (ax_v, ax_t) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

    ax_v.plot(times, voltages, color="tab:blue")
    ax_v.set_ylabel("Voltage, V")
    ax_v.grid(True)

    ax_t.plot(times, temps, color="tab:red")
    ax_t.set_ylabel("Temperature, C")
    ax_t.set_xlabel("Time, s")
    ax_t.grid(True)

    figure.tight_layout()
    figure.savefig(name_png)
    plt.close(figure)


def run_telemetry(board):
    rows = []
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        try:
            port.write(b"tm_start\r")
            rows = collect(port, 10)
        finally:
            port.write(b"tm_stop\r")
            time.sleep(0.2)
            port.reset_input_buffer()

    write_csv("telemetry.csv", rows)
    plot("telemetry.png", rows)
    print("Записей: %d, telemetry.csv и telemetry.png сохранены" % len(rows))


def run_rc(board):
    rows = []
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        try:
            # Период 20 мс: за 10 секунд опыта наберётся около 500 записей.
            port.write(b"tm_start 20\r")
            time.sleep(0.1)
            port.write(b"charge_on\r")
            charge_rows = collect(port, 5)
            port.write(b"charge_off\r")
            discharge_rows = collect(port, 5)
            rows = charge_rows + discharge_rows
        finally:
            port.write(b"tm_stop\r")
            time.sleep(0.2)
            port.reset_input_buffer()

    write_csv("rc.csv", rows)
    plot("rc.png", rows)
    print("Записей: %d, rc.csv и rc.png сохранены" % len(rows))


def main():
    parser = argparse.ArgumentParser(description="Телеметрия прибора 311-adc")
    parser.add_argument("--plot", metavar="CSV", help="построить график по готовому CSV, без платы")
    parser.add_argument("--rc", action="store_true", help="опыт с RC-цепью: заряд и разряд")
    args = parser.parse_args()

    if args.plot is not None:
        rows = read_csv(args.plot)
        plot("telemetry.png", rows)
        print("График по %s сохранён в telemetry.png, записей %d" % (args.plot, len(rows)))
        return

    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.")
        return

    if args.rc:
        run_rc(board)
    else:
        run_telemetry(board)


if __name__ == "__main__":
    main()