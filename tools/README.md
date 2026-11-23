# Tools

Helper scripts that run on the computer and talk to the board over its USB serial port.
They need Python 3 and [pyserial](https://pypi.org/project/pyserial/). PlatformIO's own Python already has pyserial:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\dice_fairness.py COM4
```

Close the Serial Monitor before running them: only one program can use the port at a time.
They open the port without restarting the board, so the screen you are on stays put.

| Script | What it does |
|---|---|
| `dice_fairness.py` | Rolls the d20 10,000 times on the board and checks the counts with a chi-square test. `--file log.txt` reads a saved Serial Monitor log instead, `--demo` shows how the test catches a slightly unfair die. |
| `screenshot.py` | Saves what the screen shows as a PNG, straight from the board's memory (about 10 s). |
| `device_serial.py` | Shared code: opening the port without a reset, waiting for a line. |

## Serial commands

You can also type these in the Serial Monitor (115200 baud) and press Enter:

| Command | Does |
|---|---|
| `a` / `b` | Short press of button A / B |
| `t` | d20 fairness test: 10,000 rolls, counts and chi-square |
| `s` | Screenshot (raw pixels, meant for `screenshot.py`) |
| `+` / `-` | Testing only: the next d20 roll is a 20 / a 1 |
