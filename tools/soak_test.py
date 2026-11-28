"""Long-run test: keeps the board busy for a while and checks it never restarts and doesn't run out of memory.

    python tools/soak_test.py COM4                 30 minutes
    python tools/soak_test.py COM4 --minutes 5 --log soak.txt

Every couple of seconds it does what a user would: next theme, the theme's action, a trip into the menu and
back. Once a minute it asks for the free memory ("m"). A restart, a crash report or memory that keeps going
down means a problem. Close the Serial Monitor first.
"""
import argparse
import random
import re
import time

from device_serial import open_without_reset

# Lines the ESP32 prints when it restarts or crashes
CRASH_SIGNS = ("rst:", "Guru Meditation", "Backtrace", "abort()", "Kart: ")
MEMORY = re.compile(r"MEM heap (\d+) KB \(en az (\d+) KB\), PSRAM (\d+) KB")


def user_action(rng):
    """What a person might do next. The menu is always closed again, so the board stays in the theme screen."""
    roll = rng.random()
    if roll < 0.55:
        return "a"  # next theme
    if roll < 0.80:
        return "b"  # roll the die, make the face smile...
    return "A" + "a" * rng.randint(1, 8) + "A"  # open the menu, look around, close it


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port")
    parser.add_argument("--minutes", type=float, default=30)
    parser.add_argument("--every", type=float, default=2.5, help="seconds between actions")
    parser.add_argument("--log", help="also save everything the board says to this file")
    args = parser.parse_args()

    rng = random.Random()
    end = time.time() + args.minutes * 60
    actions, crashes, memory = 0, [], []
    next_memory_check = time.time()
    log = open(args.log, "w", encoding="utf-8") if args.log else None
    buffer = b""
    with open_without_reset(args.port) as board:
        board.timeout = 0.1
        while time.time() < end:
            if time.time() >= next_memory_check:
                board.write(b"m")
                next_memory_check += 60
            else:
                for key in user_action(rng):
                    board.write(key.encode())
                    time.sleep(0.15)
                actions += 1
            time.sleep(args.every)
            buffer += board.read(65536)
            *lines, buffer = buffer.split(b"\n")
            for raw in lines:
                line = raw.decode("utf-8", errors="replace").strip()
                if log:
                    log.write(line + "\n")
                if any(sign in line for sign in CRASH_SIGNS):
                    crashes.append(line)
                    print(f"!! {line}")
                found = MEMORY.search(line)
                if found:
                    memory.append(tuple(int(v) for v in found.groups()))
                    heap, lowest, psram = memory[-1]
                    print(f"{len(memory):>3}. dk  RAM {heap} KB (en az {lowest} KB), PSRAM {psram} KB, {actions} hareket")
    if log:
        log.close()

    print(f"\n{args.minutes:g} dakika, {actions} hareket, {len(crashes)} yeniden baslama/cokme")
    if memory:
        first, last = memory[0], memory[-1]
        print(f"RAM: {first[0]} -> {last[0]} KB, en dusuk {min(m[1] for m in memory)} KB. PSRAM: {first[2]} -> {last[2]} KB")
    print("GECTI" if not crashes else "KALDI: yukaridaki satirlara bak")


if __name__ == "__main__":
    main()
