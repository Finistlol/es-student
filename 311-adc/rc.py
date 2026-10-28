# Опыт с RC-цепью, задание п3.1.5.
#
# rc.py --r <кОм> --c <мкФ>: ведёт опыт по 5tau на заряд и разряд,
# команды charge_on и charge_off отправляет сам, сохраняет rc.csv,
# график rc.png и итог rc.log с номиналами и найденной tau.
#
# rc.py --r <кОм> --c <мкФ> --plot rc.csv: пересчитывает итог и график
# по готовой записи, плата не нужна.

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
            return int(parts[1]), float(parts[2]), float(parts[3])
        except ValueError:
            return None
    return None


def collect(port, seconds):
    rows = []
    deadline = time.monotonic() + seconds
    received = ""
    while time.monotonic() < deadline:
        received += port.read(2048).decode("ascii", "replace")
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
        csv.readline()
        for line in csv:
            parts = line.strip().split(",")
            if len(parts) == 3:
                rows.append((float(parts[0]), float(parts[1]), float(parts[2])))
    return rows


# tau по уровням вершины: заряд — от 2 % до 63,2 %, разряд — от 98 % до 36,8 %.
def find_tau(rows):
    times = [row[0] for row in rows]
    volts = [row[1] for row in rows]

    top = max(volts)
    peak = volts.index(top)

    charge_start = None
    charge_end = None
    for i in range(0, peak + 1):
        if charge_start is None and volts[i] >= 0.02 * top:
            charge_start = i
        if charge_start is not None and volts[i] >= 0.632 * top:
            charge_end = i
            break

    discharge_start = None
    discharge_end = None
    for i in range(peak, len(volts)):
        if discharge_start is None and volts[i] <= 0.98 * top:
            discharge_start = i
        if discharge_start is not None and volts[i] <= 0.368 * top:
            discharge_end = i
            break

    tau_charge = None if charge_end is None else times[charge_end] - times[charge_start]
    tau_discharge = None if discharge_end is None else times[discharge_end] - times[discharge_start]
    return tau_charge, tau_discharge


def write_log(name, r_kohm, c_uf, tau_charge, tau_discharge):
    with open(name, "w", encoding="utf-8") as log:
        log.write("R_kohm = %s\n" % r_kohm)
        log.write("C_uf = %s\n" % c_uf)
        log.write("tau_charge_s = %.3f\n" % tau_charge)
        log.write("tau_discharge_s = %.3f\n" % tau_discharge)


def plot(name, rows):
    times = [row[0] for row in rows]
    voltages = [row[1] for row in rows]

    figure, axis = plt.subplots(figsize=(10, 5))
    axis.plot(times, voltages, color="tab:blue")
    axis.set_ylabel("Voltage, V")
    axis.set_xlabel("Time, s")
    axis.set_title("RC circuit")
    axis.grid(True)

    figure.tight_layout()
    figure.savefig(name)
    plt.close(figure)


def run_board(r_kohm, c_uf):
    tau = r_kohm * c_uf / 1000.0

    # Не меньше 20 отсчётов на tau и не быстрее периода телеметрии 10 мс.
    period_ms = max(10, int(tau * 1000.0 / 100.0))
    phase_s = 5.0 * tau

    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.")
        return None

    rows = []
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        try:
            port.write(("tm_start %d\r" % period_ms).encode("ascii"))
            time.sleep(0.1)
            port.write(b"charge_on\r")
            charge_rows = collect(port, phase_s)
            port.write(b"charge_off\r")
            discharge_rows = collect(port, phase_s)
            rows = charge_rows + discharge_rows
        finally:
            port.write(b"tm_stop\r")
            time.sleep(0.2)
            port.reset_input_buffer()

    write_csv("rc.csv", rows)
    return read_csv("rc.csv")


def main():
    parser = argparse.ArgumentParser(description="Опыт с RC-цепью")
    parser.add_argument("--r", type=float, required=True, help="номинал резистора, кОм")
    parser.add_argument("--c", type=float, required=True, help="номинал конденсатора, мкФ")
    parser.add_argument("--plot", metavar="CSV", help="пересчитать итог по готовой записи, без платы")
    args = parser.parse_args()

    if args.plot is not None:
        rows = read_csv(args.plot)
    else:
        rows = run_board(args.r, args.c)

    if not rows:
        return

    tau_charge, tau_discharge = find_tau(rows)
    if tau_charge is None or tau_discharge is None:
        print("Кривая не прошла уровни 63%% и 37%%: проверьте цепь и номиналы.")
        return

    write_log("rc.log", args.r, args.c, tau_charge, tau_discharge)
    plot("rc.png", rows)

    print("Записей: %d" % len(rows))
    print("R·C = %.3f с, tau заряда %.3f с, tau разряда %.3f с" %
          (args.r * args.c / 1000.0, tau_charge, tau_discharge))
    print("Сохранены rc.csv, rc.png, rc.log")


if __name__ == "__main__":
    main()