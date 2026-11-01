# Скрипт занятия 3.2: приём потока отсчётов прибора 321-audio-recorder.
#
# Без аргументов: принимает поток 5 секунд, разбирает пакеты, проверяет
# номера и считает скорость, итог пишет в stream.log.
# --wav <сек>: принимает поток и сохраняет запись в recording.wav
# (16 бит, 44100 Гц), итог — в recording.log. Задание п3.2.6.
# --wav8 <сек>: то же, но 8 бит в recording-8bit.wav и recording-8bit.log.

import argparse
import struct
import time
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

MARKER = 0x5AA5
SAMPLES_PER_PACKET = 512
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
            if i + HEADER_LEN + 2 > len(buf):
                break
            marker, number, count = struct.unpack_from(HEADER_FMT, buf, i)
            end = i + HEADER_LEN + count * 2
            if end > len(buf):
                break
            samples = struct.unpack_from("<%dH" % count, buf, i + HEADER_LEN)
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
    """Читает поток секунд, возвращает (пакеты, строки, время первого и последнего)."""
    all_packets = []
    all_lines = []
    buf = started_state
    first_moment = None
    last_moment = None
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        # Читаем только то, что уже пришло: чтение без искусственных пауз,
        # иначе CDC на плате переполняется между редкими опросами.
        waiting = port.in_waiting
        if waiting:
            buf.extend(port.read(waiting))
        else:
            time.sleep(0.002)
        if first_moment is None and b"\xa5\x5a" in buf:
            first_moment = time.monotonic()
        packets, lines, buf = parse_stream(buf)
        if packets:
            last_moment = time.monotonic()
        all_packets.extend(packets)
        all_lines.extend(lines)
    return all_packets, all_lines, buf, first_moment, last_moment


def board_header(board, task):
    lines = [
        "задание: " + task,
        "проект: 321-audio-recorder",
        "устройство: %04x:%04x" % (board.vid, board.pid),
        "серийный номер: " + str(board.serial_number),
        "порт: " + board.device,
        "начало: " + datetime.now().isoformat(timespec="seconds"),
    ]
    return lines


def run_stream_test(board):
    packets = []
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        # Защита от прошлого прерванного запуска: остановить идущий поток.
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        port.reset_input_buffer()
        port.write(b"stream_start\r")
        # Прогрев: полсекунды прибор разгоняется, пакеты этого окна
        # учитываются в итоге, но в измерение скорости не входят.
        warmup_until = time.monotonic() + 0.6
        warmup_buf = bytearray()
        while time.monotonic() < warmup_until:
            waiting = port.in_waiting
            if waiting:
                warmup_buf.extend(port.read(waiting))
            else:
                time.sleep(0.002)
        warmup_packets, warmup_lines, warmup_rest = parse_stream(warmup_buf)
        stream_packets, lines, rest, first_moment, last_moment = collect(port, 5.0, warmup_rest)
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        tail_packets, tail_lines, _, _, tail_last = collect(port, 1.0, rest)
        packets = warmup_packets + stream_packets + tail_packets
        lines = warmup_lines + lines + tail_lines
        if tail_last is not None:
            last_moment = tail_last

    numbers = [p[0] for p in packets]
    samples_total = sum(p[1] for p in packets)
    missing = 0
    if numbers:
        missing = (numbers[-1] - numbers[0] + 1) - len(numbers)

    # Скорость по моментам прихода первого и последнего пакета окна:
    # прогревные пакеты в знаменатель и числитель не входят.
    window_samples = sum(p[1] for p in stream_packets + tail_packets)
    if first_moment is not None and last_moment is not None and last_moment > first_moment:
        window_s = last_moment - first_moment
        rate = (window_samples - 512) / window_s if window_samples > 512 else 0
    else:
        window_s = 5.0
        rate = window_samples / window_s

    with open("stream.log", "w", encoding="utf-8") as log:
        for line in board_header(board, "3.2.4"):
            log.write(line + "\n")
        log.write("<-- stream: started, 44100 Hz, 16 bit\n")
        for line in lines:
            if line.startswith("stream: stopped"):
                log.write("<-- %s\n" % line)
        log.write("принято: пакетов %d, отсчётов %d, пропущено пакетов %d\n" %
                  (len(packets), samples_total, missing))
        log.write("скорость: %d отсчётов в секунду за %.1f с\n" % (rate, window_s))

    print("Пакетов: %d, отсчётов: %d, пропущено: %d, скорость: %d" %
          (len(packets), samples_total, missing, rate))


def run_wav(board, seconds, bits):
    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        port.write(b"stream_start\r")
        time.sleep(0.5)
        packets, lines, rest, first_moment, last_moment = collect(port, seconds, bytearray())
        port.write(b"stream_stop\r")
        time.sleep(1.0)
        tail, tail_lines, _, _, _ = collect(port, 1.0, rest)
        packets = packets + tail
        lines = lines + tail_lines

    samples = []
    for _, count, data in packets:
        samples.extend(data)

    rate = 44100
    if bits == 16:
        name_wav = "recording.wav"
        name_log = "recording.log"
        payload = struct.pack("<%dH" % len(samples), *samples)
        audio = payload
        bits_tag = "16 bit"
    else:
        name_wav = "recording-8bit.wav"
        name_log = "recording-8bit.log"
        audio = bytes((s >> 4) & 0xFF for s in samples)
        bits_tag = "8 bit"

    channels = 1
    byte_rate = rate * channels * bits // 8
    block_align = channels * bits // 8
    data_len = len(audio)

    with open(name_wav, "wb") as wav:
        wav.write(b"RIFF")
        wav.write(struct.pack("<I", 36 + data_len))
        wav.write(b"WAVE")
        wav.write(b"fmt ")
        wav.write(struct.pack("<IHHIIHH", 16, 1, channels, rate, byte_rate, block_align, bits))
        wav.write(b"data")
        wav.write(struct.pack("<I", data_len))
        wav.write(audio)

    duration_s = data_len / byte_rate
    with open(name_log, "w", encoding="utf-8") as log:
        for line in board_header(board, "3.2.6" if bits == 16 else "3.2.6д"):
            log.write(line + "\n")
        log.write("<-- stream: started, 44100 Hz, 16 bit\n")
        log.write("файл: %s\n" % name_wav)
        log.write("формат: %d бит, %d Гц, %d каналов\n" % (bits, rate, channels))
        log.write("длительность: %.2f с\n" % duration_s)
        log.write("отсчётов: %d\n" % len(samples))

    print("%s: %.2f с, %d отсчётов" % (name_wav, duration_s, len(samples)))


def main():
    parser = argparse.ArgumentParser(description="Приём потока отсчётов")
    parser.add_argument("--wav", type=float, metavar="SEC", help="запись в recording.wav, секунд")
    parser.add_argument("--wav8", type=float, metavar="SEC", help="запись в recording-8bit.wav")
    args = parser.parse_args()

    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку проекта.")
        return

    if args.wav is not None:
        run_wav(board, args.wav, 16)
    elif args.wav8 is not None:
        run_wav(board, args.wav8, 8)
    else:
        run_stream_test(board)


main()