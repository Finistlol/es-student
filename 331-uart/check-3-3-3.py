# Проверка задания п3.3.3 на устройстве: два UART перекрёстно,
# допуск скорости и чётность.
# Обмен записывается в device-3-3-3.log.

import time
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

TASK = "3.3.3"
PROJECT = "331-uart"
LOG_NAME = "device-3-3-3.log"

STEPS = [
    ("\r", 0.5),
    ("help\r", 1),
    ("link ping\r", 1),
    ("parity none none\r", 0.5),
    ("baud_shift 0\r", 0.5),
    ("text\r", 1.5),
    ("baud_shift 2\r", 0.5),
    ("text\r", 1.5),
    ("baud_shift -2\r", 0.5),
    ("text\r", 1.5),
    ("baud_shift -10\r", 0.5),
    ("text\r", 1.5),
    ("baud_shift 0\r", 0.5),
    ("parity even odd\r", 0.5),
    ("text\r", 1.5),
    ("parity none none\r", 0.5),
    ("text\r", 1.5),
    ("baud_shift 60\r", 0.5),
    ("parity none\r", 0.5),
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
                received += port.read(2048).decode("ascii", "replace")
                while "\n" in received:
                    line, received = received.split("\n", 1)
                    line = printable(line).strip()
                    if line:
                        exchange.append((time.monotonic() - started, "<--", line))
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