# Проверка задания п3.1.3 на устройстве: запуск и остановка телеметрии,
# период по умолчанию и отклонение неверных аргументов.
# Обмен записывается в device-3-1-3.log.

import time
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

TASK = "3.1.3"
PROJECT = "311-adc"
LOG_NAME = "device-3-1-3.log"

# Порядок важен: после отклонённых tm_start потока быть не должно,
# поэтому телеметрию останавливают до проверок ошибок.
STEPS = [
    ("help\r", 1),
    ("tm_start 50\r", 1.5),
    ("tm_stop\r", 0.5),
    ("tm_start\r", 1.5),
    ("tm_stop\r", 0.5),
    ("tm_start 5\r", 0.5),
    ("tm_start 10 20\r", 0.5),
    ("tm_start 5x\r", 0.5),
    ("get_voltage\r", 1),
]


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def printable(text):
    return text.replace("\x7f", "<BS>").replace("\b", "<BS>").replace("\r", "").replace("\n", "")


def talk(board):
    exchange = []
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        started = time.monotonic()
        received = ""
        for frame, listen_s in STEPS:
            port.write(frame.encode("ascii"))
            exchange.append((time.monotonic() - started, "-->", printable(frame)))
            deadline = time.monotonic() + listen_s
            while time.monotonic() < deadline:
                received += port.read(512).decode("ascii", "replace")
                while "\n" in received:
                    line, received = received.split("\n", 1)
                    line = printable(line).strip()
                    if line:
                        exchange.append((time.monotonic() - started, "<--", line))
                        print(line, end="\r\n")
    return exchange


def write_log(board, exchange):
    with open(LOG_NAME, "w", encoding="utf-8") as log:
        log.write("задание: " + TASK + "\n")
        log.write("проект: " + PROJECT + "\n")
        log.write("устройство: %04x:%04x\n" % (board.vid, board.pid))
        log.write("серийный номер: " + str(board.serial_number) + "\n")
        log.write("порт: " + board.device + "\n")
        log.write("начало: " + datetime.now().isoformat(timespec="seconds") + "\n")
        for moment, direction, text in exchange:
            log.write("%8.3f %s %s\n" % (moment, direction, text))
        log.write("итог: отправлено кадров %d, принято строк %d\n" % (len(STEPS), len(exchange) - len(STEPS)))


board = find_board()

if board is None:
    print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.", end="\r\n")
else:
    print("Плата на порту " + board.device + ", отправляю кадры", end="\r\n")
    exchange = talk(board)
    write_log(board, exchange)
    print("Обмен записан в " + LOG_NAME, end="\r\n")