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
| `prepare_images.py` | Turns pictures and animated GIFs in `themes-private/` into 240x240 files in `firmware/data/themes/`, plus 64x64 menu thumbnails and `index.json` (the theme list the board reads). Needs Pillow: `pip install pillow`. Then PlatformIO's "Upload Filesystem Image" copies them to the board. Both folders are git-ignored: art you don't own never goes into the repo. |
| `make_sample_themes.py` | Draws the sample themes in `themes-sample/` (night sky, sunset, pixel heart, retro grid, the blinking face as a GIF). Free to use, like the rest of the code. |
| `soak_test.py` | Long-run test: keeps tapping through themes and the menu for 30 minutes, checks for restarts and shrinking memory. |
| `device_serial.py` | Shared code: opening the port without a reset, waiting for a line. |

## Serial commands

You can also type these in the Serial Monitor (115200 baud) and press Enter:

| Command | Does |
|---|---|
| `a` / `b` | Short press of button A / B |
| `A` | Long press of button A (opens / closes the theme menu) |
| `t` | d20 fairness test: 10,000 rolls, counts and chi-square |
| `s` | Screenshot (raw pixels, meant for `screenshot.py`) |
| `+` / `-` | Testing only: the next d20 roll is a 20 / a 1 |
| `d` | Touch readings and thresholds of A and B |
| `m` | Free RAM (and the lowest it has been), free PSRAM, seconds since start |
