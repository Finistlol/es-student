# Проверка задания п3.2.7 на устройстве: сторожевой таймер, намеренный
# завис и перезапуск платы с причиной сброса и счётчиком перезапусков.
# Обмен записывается в device-3-2-7.log.

import time
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

TASK = "3.2.7"
PROJECT = "321-audio-recorder"
LOG_NAME = "device-3-2-7.log"


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def printable(text):
    return text.replace("\x7f", "<BS>").replace("\b", "<BS>").replace("\r", "").replace("\n", "")


def send_and_read(port, started, exchange, frame, listen_s):
    try:
        port.write(frame.encode("ascii"))
    except serial.SerialException:
        return
    exchange.append((time.monotonic() - started, "-->", printable(frame)))
    deadline = time.monotonic() + listen_s
    received = ""
    while time.monotonic() < deadline:
        try:
            received += port.read(1024).decode("ascii", "replace")
        except serial.SerialException:
            return
        while "\n" in received:
            line, received = received.split("\n", 1)
            line = printable(line).strip()
            if line:
                exchange.append((time.monotonic() - started, "<--", line))
                print(line, end="\r\n")


def wait_board_back(timeout_s):
    # Пока суперцикл висит, USB живёт на прерываниях и порт числится в системе.
    # Сначала ждём, когда плата в самом деле перезапустится — порт исчезнет,
    # потом когда вернётся.
    vanish_by = time.monotonic() + 6
    while time.monotonic() < vanish_by:
        if find_board() is None:
            break
        time.sleep(0.2)
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        port = find_board()
        if port is not None:
            return port
        time.sleep(0.5)
    return None


def open_board(retries=20):
    """Открывает порт с повторами: после тяжёлого потока драйверу USB
    нужна секунда-другая, чтобы снова отдать порт."""
    for _ in range(retries):
        board = find_board()
        if board is not None:
            try:
                return serial.Serial(board.device, timeout=0.05), board
            except serial.SerialException:
                pass
        time.sleep(1)
    return None, None


def main():
    port, board = open_board()
    if port is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.", end="\r\n")
        return

    started = time.monotonic()
    exchange = []

    with port:
        time.sleep(0.2)
        port.reset_input_buffer()

        send_and_read(port, started, exchange, "help\r", 1)
        send_and_read(port, started, exchange, "reset_reason\r", 1)
        send_and_read(port, started, exchange, "wdt_enable 9000\r", 1)
        send_and_read(port, started, exchange, "wdt_enable 2000\r", 1)
        # Прибор должен прожить дольше таймаута: читаем uptime дважды.
        time.sleep(3)
        send_and_read(port, started, exchange, "uptime\r", 1)
        time.sleep(2.6)
        send_and_read(port, started, exchange, "uptime\r", 1)
        # Вечный цикл: плата замолчит и перезапустится сторожем.
        send_and_read(port, started, exchange, "hang\r", 1)

    moment = time.monotonic() - started
    exchange.append((moment, "---", "плата снова на связи... ожидание"))
    print("Ждём перезапуска платы...", end="\r\n")

    board = wait_board_back(15)
    if board is None:
        print("Плата не вернулась после зависания.", end="\r\n")
    else:
        moment = time.monotonic() - started
        exchange.append((moment, "---", "плата снова на связи"))
        port2, board = open_board()
        if port2 is not None:
            with port2:
                time.sleep(2.0)
                port2.reset_input_buffer()
                send_and_read(port2, started, exchange, "reset_reason\r", 1)
                send_and_read(port2, started, exchange, "uptime\r", 1)

    with open(LOG_NAME, "w", encoding="utf-8") as log:
        log.write("задание: " + TASK + "\n")
        log.write("проект: " + PROJECT + "\n")
        log.write("устройство: %04x:%04x\n" % (VENDOR_ID, PRODUCT_ID))
        log.write("серийный номер: " + str(board.serial_number if board else "") + "\n")
        log.write("порт: " + (board.device if board else "") + "\n")
        log.write("начало: " + datetime.now().isoformat(timespec="seconds") + "\n")
        for moment, direction, text in exchange:
            log.write("%8.3f %s %s\n" % (moment, direction, text))
    print("Обмен записан в " + LOG_NAME, end="\r\n")


main()