# Скрипт занятия 3.2: приём потока отсчётов прибора 321-audio-recorder.
#
# Без аргументов: принимает поток 5 секунд, разбирает пакеты, проверяет
# номера и считает скорость, итог пишет в stream.log.
# --wav <сек> [--bits 8]: принимает поток и сохраняет запись
# в recording.wav (16 бит) или recording-8bit.wav (8 бит), 44100 Гц,
# одним каналом; итог — в одноимённый .log.

import argparse
import struct
import time
from datetime import datetime

import serial
from serial.tools import list_ports
import wave

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

MARKER = 0x5AA5
SAMPLE_RATE = 44100
HEADER_FMT = "<HHH"
HEADER_LEN = struct.calcsize(HEADER_FMT)


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def printable(text):
    return text.replace("\x7f", "<BS>").replace("\b", "<BS>").replace("\r", "").replace("\n", "")


def parse_stream(buf):
    """Разбирает пакеты в буфере, возвращает (пакеты, текстовые строки, остаток)."""
    packets = []
    lines = []
    rest = bytearray()

    i = 0
    text = bytearray()
    while i < len(buf):
        if buf[i] == 0xA5 and i + 1 < len(buf) and buf[i + 1] == 0x5A:
            if i + HEADER_LEN + 1 > len(buf):
                break
            marker, number, count = struct.unpack_from(HEADER_FMT, buf, i)
            data_bytes = count if count <= 512 else count  # 8 бит: байт на отсчёт
            sample_bytes = count * 2 if count <= 512 else count
            end = i + HEADER_LEN + sample_bytes
            if end > len(buf):
                break
            samples = buf[i + HEADER_LEN:end]
            packets.append((number, count, samples))
            if text:
                for line in text.decode("ascii", "replace").splitlines():
                    line = printable(line).strip()
                    if line:
                        lines.append(line)
                text = bytearray()
            i = end
        else:
            if text or buf[i] >= 0x20 or buf[i] == 0x0A or buf[i] == 0x0D:
                text.append(buf[i])
            i += 1

    if text:
        for line in text.decode("ascii", "replace").splitlines():
            line = printable(line).strip()
            if line:
                lines.append(line)

    rest = bytearray(buf[i:])
    return packets, lines, rest


def collect(port, seconds, started_state):
    """Читает поток секунд; пакеты — кортежи (номер, отсчётов, байты данных)."""
    all_packets = []
    all_lines = []
    buf = started_state
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        waiting = port.in_waiting
        if waiting:
            buf.extend(port.read(waiting))
        else:
            time.sleep(0.002)
        packets, lines, buf = parse_stream(buf)
        all_packets.extend(packets)
        all_lines.extend(lines)
    return all_packets, all_lines, buf


def stop_stream(port):
    """Останавливает идущий поток и собирает ответ."""
    port.write(b"stream_stop\r")
    packets, lines, _ = collect(port, 1.2, bytearray())
    return packets, lines


def board_header(board, task):
    return [
        "задание: " + task,
        "проект: 321-audio-recorder",
        "устройство: %04x:%04x" % (board.vid, board.pid),
        "серийный номер: " + str(board.serial_number),
        "порт: " + board.device,
        "начало: " + datetime.now().isoformat(timespec="seconds"),
    ]


def packet_stats(packets):
    numbers = [p[0] for p in packets]
    samples_total = sum(p[1] for p in packets)
    missing = 0
    if numbers:
        missing = (numbers[-1] - numbers[0] + 1) - len(numbers)
    return len(packets), samples_total, missing


def find_line(lines, prefix):
    for line in lines:
        if line.startswith(prefix):
            return line
    return None


def run_stream_test(board):
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        # Защита от прошлого прерванного запуска: остановить идущий поток.
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        port.reset_input_buffer()
        port.write(b"stream_start\r")
        # Прогрев: пакеты этого окна в итоге считаются, в скорость — нет.
        warmup_until = time.monotonic() + 0.6
        warmup_buf = bytearray()
        while time.monotonic() < warmup_until:
            waiting = port.in_waiting
            if waiting:
                warmup_buf.extend(port.read(waiting))
            else:
                time.sleep(0.002)
        warmup_packets, warmup_lines, warmup_rest = parse_stream(warmup_buf)
        window_packets, lines, rest = collect(port, 5.0, warmup_rest)
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        tail_packets, tail_lines, _ = collect(port, 1.0, rest)
        packets = warmup_packets + window_packets + tail_packets
        lines = warmup_lines + lines + tail_lines

    count, samples_total, missing = packet_stats(packets)
    window_samples = sum(p[1] for p in window_packets + tail_packets)
    window_s = 5.0
    rate = window_samples / window_s

    with open("stream.log", "w", encoding="utf-8") as log:
        for line in board_header(board, "3.2.4"):
            log.write(line + "\n")
        log.write("<-- stream: started, 44100 Hz, 16 bit\n")
        stopped = find_line(lines, "stream: stopped")
        if stopped:
            log.write("<-- %s\n" % stopped)
        log.write("принято: пакетов %d, отсчётов %d, пропущено пакетов %d\n" %
                  (count, samples_total, missing))
        log.write("скорость: %d отсчётов в секунду за %.1f с\n" % (rate, window_s))

    print("Пакетов: %d, отсчётов: %d, пропущено: %d, скорость: %d" %
          (count, samples_total, missing, rate))


def run_wav(board, seconds, bits):
    if bits == 16:
        name_wav = "recording.wav"
        name_log = "recording.log"
        task = "3.2.6"
    else:
        name_wav = "recording-8bit.wav"
        name_log = "recording-8bit.log"
        task = "3.2.6д"

    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        # Защита от прошлого прерванного запуска.
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        port.reset_input_buffer()

        port.write(b"stream_start 8\r" if bits == 8 else b"stream_start\r")
        packets, lines, _ = collect(port, seconds, bytearray())
        stop_packets, stop_lines = stop_stream(port)
        packets = packets + stop_packets
        lines = lines + stop_lines

    count, samples_total, missing = packet_stats(packets)
    duration = samples_total / float(SAMPLE_RATE)

    # Отсчёты: 8-битный поток даёт байт на отсчёт, 16-битный — полуслово.
    audio = bytearray()
    for _, _, data in packets:
        if bits == 8:
            audio.extend(data)
        else:
            for i in range(0, len(data), 2):
                audio.extend(data[i:i + 2])

    with wave.open(name_wav, "wb") as wav:
        wav.setnchannels(1)
        if bits == 16:
            wav.setsampwidth(2)
        else:
            wav.setsampwidth(1)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(bytes(audio))

    with open(name_log, "w", encoding="utf-8") as log:
        for line in board_header(board, task):
            log.write(line + "\n")
        log.write("<-- stream: started, 44100 Hz, %d bit\n" % bits)
        stopped = find_line(lines, "stream: stopped")
        if stopped:
            log.write("<-- %s\n" % stopped)
        log.write("файл: %s, 44100 Гц, %.2f с\n" % (name_wav, duration))
        log.write("принято: пакетов %d, отсчётов %d, пропущено пакетов %d\n" %
                  (count, samples_total, missing))

    print("%s: %.2f с, %d отсчётов, %d пакетов" % (name_wav, duration, samples_total, count))


def main():
    parser = argparse.ArgumentParser(description="Приём потока отсчётов")
    parser.add_argument("--wav", type=float, metavar="SEC", help="запись в recording.wav, секунд")
    parser.add_argument("--bits", type=int, choices=[8, 16], default=16, help="разрядность записи")
    parser.add_argument("--wav8", type=float, metavar="SEC", help="запись в recording-8bit.wav")
    args = parser.parse_args()

    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.")
        return

    if args.wav is not None:
        run_wav(board, args.wav, args.bits)
    elif args.wav8 is not None:
        run_wav(board, args.wav8, 8)
    else:
        run_stream_test(board)


main()