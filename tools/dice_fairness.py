"""Is the PixelCharm d20 fair? Rolls it 10,000 times on the board and checks the counts with a chi-square test.

    python tools/dice_fairness.py COM4             ask the board (close the Serial Monitor first)
    python tools/dice_fairness.py --file log.txt   read a Serial Monitor log in which you typed "t"
    python tools/dice_fairness.py --demo           see how the test catches a slightly unfair die

The device side is printFairnessTest() in firmware/src/dice.h.
"""
import argparse
import math
import random

SIDES = 20
DEGREES_OF_FREEDOM = SIDES - 1
CRITICAL_VALUE = 30.144  # chi-square for 19 degrees of freedom at the usual 5% level


def chi_square(counts):
    """How far the counts are from a perfectly even spread. 0 = exactly even."""
    expected = sum(counts) / len(counts)
    return sum((count - expected) ** 2 / expected for count in counts)


def p_value(chi2, dof=DEGREES_OF_FREEDOM):
    """The chance that a fair die would land at least this far from even, just by luck.

    This is the upper tail of the chi-square distribution: 1 - P(dof/2, chi2/2), where P is the regularised
    lower incomplete gamma function, summed as a series (standard library only, no SciPy).
    """
    a, x = dof / 2, chi2 / 2
    if x <= 0:
        return 1.0
    term = total = 1 / a
    n = 1
    while term > total * 1e-15:
        term *= x / (a + n)
        total += term
        n += 1
    lower = total * math.exp(a * math.log(x) - x - math.lgamma(a))
    return max(0.0, 1 - lower)


def parse_counts(lines):
    """Finds the D20_TEST_START ... D20_TEST_END block and returns the 20 counts in order."""
    counts, inside = {}, False
    for line in lines:
        line = line.strip()
        if line.startswith("D20_TEST_START"):
            counts, inside = {}, True
        elif line.startswith("D20_TEST_END"):
            if inside and sorted(counts) == list(range(1, SIDES + 1)):
                return [counts[face] for face in range(1, SIDES + 1)]
            inside = False
        elif inside:
            face, count = line.split()
            counts[int(face)] = int(count)
    raise ValueError("no complete D20_TEST_START ... D20_TEST_END block found")


def counts_from_board(port):
    from device_serial import open_without_reset, wait_for_line

    with open_without_reset(port) as board:
        board.write(b"t")
        lines = [wait_for_line(board, "D20_TEST_START", 5)]
        while not lines[-1].startswith("D20_TEST_END"):
            lines.append(board.readline().decode("utf-8", errors="replace"))
        return parse_counts(lines)


def report(counts):
    total = sum(counts)
    expected = total / SIDES
    print(f"{total:,} rolls, a fair die gives each number about {expected:.0f}")
    print("The bars show how far each count is from that: left = fewer, right = more.\n")
    widest = max(abs(count - expected) for count in counts) or 1
    for face, count in enumerate(counts, start=1):
        diff = count - expected
        length = round(abs(diff) / widest * 15)
        left = ("#" * length if diff < 0 else "").rjust(15)
        right = "#" * length if diff > 0 else ""
        print(f"{face:>3} {count:>6} {diff:+6.0f}  {left}|{right}")
    chi2 = chi_square(counts)
    p = p_value(chi2)
    print(f"\nchi-square = {chi2:.2f} (fair-looking below {CRITICAL_VALUE}), p = {p:.3f}")
    if chi2 < CRITICAL_VALUE:
        print("PASS: nothing unfair found. (A test can't prove fairness; it can only fail to find bias.)")
    else:
        print("FAIL: this spread would happen to a fair die less than 5% of the time.")
        print("      A fair die still fails about 1 run in 20, so run it again before you worry.")
    return chi2 < CRITICAL_VALUE


def demo():
    """Why the firmware doesn't just use '% 20': a die built from one random byte (0-255) with % 20."""
    print("A die made from one random byte with '% 20': 256 = 12 x 20 + 16, so 1-16 come up")
    print("13 times in 256 (5.08%) and 17-20 only 12 times in 256 (4.69%). Fair would be 5.00%.\n")
    for rolls in (10_000, 100_000):
        fair = [0] * SIDES
        biased = [0] * SIDES
        for _ in range(rolls):
            fair[random.randrange(SIDES)] += 1
            biased[random.getrandbits(8) % SIDES] += 1
        for name, counts in (("fair", fair), ("byte % 20", biased)):
            chi2 = chi_square(counts)
            verdict = "looks fair" if chi2 < CRITICAL_VALUE else "CAUGHT"
            print(f"{rolls:>7,} rolls  {name:<10} chi-square {chi2:7.2f}  p = {p_value(chi2):.3f}  {verdict}")
    print("\nSmall biases need many rolls to show up. The firmware avoids the problem instead of hoping it's small:")
    print("rollDie() in dice.h throws away the few random numbers that would cause it.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port", nargs="?", help="the board's serial port, e.g. COM4 or /dev/ttyACM0")
    parser.add_argument("--file", help="a saved Serial Monitor log instead of a live board")
    parser.add_argument("--demo", action="store_true", help="simulate a fair and an unfair die on the computer")
    args = parser.parse_args()

    if args.demo:
        demo()
    elif args.file:
        with open(args.file, encoding="utf-8", errors="replace") as log:
            report(parse_counts(log))
    elif args.port:
        report(counts_from_board(args.port))
    else:
        parser.error("give a port, --file or --demo")


if __name__ == "__main__":
    main()
