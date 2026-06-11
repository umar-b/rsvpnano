# RSVP Nano — umar-b build

An ESP32-S3 pocket reading device that shows text one word at a time (RSVP — Rapid Serial Visual Presentation). Books live on a microSD card; a browser-first workflow converts and uploads them.

This is a fork of [ionutdecebal/rsvpnano](https://github.com/ionutdecebal/rsvpnano) by the original device's designer — all credit for the hardware concept and the firmware foundation goes there. The [original README](https://github.com/ionutdecebal/rsvpnano#readme) documents the upstream releases; this one documents the fork build, currently `v0.1.0`.

## What this build adds

On top of upstream `v0.0.5`:

**Motion & standby**
- Set the device down flat → it enters standby by itself; set it down screen-down → the screen turns off entirely.
- Lift it from the table → it wakes. Tap-to-wake works too.
- Flick the device while reading → rewind to the previous sentence.
- Configurable idle timeout that also covers menu screens.

**Reading**
- Ramp-in: speed climbs back up gradually after every pause.
- Long words are split across frames instead of flashing past.
- Clause-aware pacing and a per-book WPM memory (each book remembers its own speed).
- Optional context line while paused showing the sentence around your position.
- Footer can show current pace vs. your average.
- Phantom-word, tracking, anchor, and guide typography tuning with live preview using text from your current book.

**Library**
- Filters (All / In progress / Unread / Finished) remembered per page.
- "Surprise me" opens a random (preferably unread) book.
- Long-press a book for mark-finished and delete (with confirmation).
- Per-book bookmarks with a reader picker and companion API.

**Stats & rewards**
- Device clock with calendar-aware reading history, daily streaks, and a daily word goal.
- Reading statistics screen and companion endpoint.
- Achievements, a celebration when you finish a book, and a boot greeting.

**Quotes**
- Star the current sentence while reading; starred sentences are saved to the card and can be browsed and exported from the web companion.

**Focus Timer**
- Audio jingles on block completion and reading sprints that count words read per block.
- Audio mute and volume settings.

**Standby screensavers**
- Word rain, DVD bounce, and book cover join Life, Maze, and Voronoi (or screen off).
- Interactive Life: set standby touch to "Play" and taps stamp gliders instead of waking.
- AMOLED burn-in jitter protection.

**Connectivity**
- Up to 5 saved home Wi-Fi networks; the device picks the strongest reachable one.
- OTA updates point at this fork's releases, with resilient downloads on marginal Wi-Fi, a firmware-version display, and a check-now button.

**Look**
- Purple accent color throughout (focus letter, selections, highlights).

## Flash the firmware

Use the hosted flasher in Chrome or Edge on desktop:

<https://umar-b.github.io/rsvpnano/>

Connect the device over USB and follow the installer prompts. The flasher installs the latest release published from this repository. OTA updates from the device's `Settings -> Firmware update` come from the same place.

## Prepare the SD card

Use a microSD card formatted as FAT32 (8–32 GB is the safe range; exFAT is not supported). Create:

```text
/books/books
/books/articles
/config
```

Books go in `/books/books`, articles in `/books/articles`. The `SD card check` tool in the main menu diagnoses mount status, write access, and folder layout if something doesn't show up.

On first open the firmware may create `.ridx`/`.rdat` sidecar files next to a book — these are the word index for long books. Leave them; they rebuild automatically if the book changes.

## Convert and add books

Convert `.epub`, `.txt`, `.md`, and `.html` to `.rsvp` with the browser converter on the [flasher page](https://umar-b.github.io/rsvpnano/), then get files onto the device one of three ways:

1. **SD card** — power off, copy files from your computer, reinsert.
2. **USB transfer** — main menu → `USB transfer`, copy files, eject, hold `PWR` to exit.
3. **Web companion** — main menu → `Companion sync`, join the `RSVP-Nano-xxxxxx` Wi-Fi the device shows, open the URL on screen (usually `http://192.168.4.1`). The companion has pages for Books, Articles, Starred Sentences, Settings, RSS, and Help. It works from any phone or desktop browser.

## Wi-Fi, RSS, and OTA

Save up to 5 home networks from the web companion, the on-device Wi-Fi settings, or `/config/ota.conf`. The device scans and connects to the strongest saved network for RSS checks and OTA updates.

RSS feeds are managed from the companion, then fetched from the device menu with `RSS feeds`; new articles land in `/books/articles`.

## Controls

### Buttons

- `PWR` short press: open the main menu / go back.
- `PWR` hold: exit full-screen pages (Companion sync, USB transfer, Focus Timer) or power off from the reader/menu.
- `BOOT` short press: cycle brightness. `BOOT` hold: cycle display theme.
- `PWR` + `BOOT` together: standby.

### Touch (reader)

- Hold the screen: read. Release: pause. Double-tap while paused: locked continuous play.
- Tap the far-left edge: rewind to sentence start (or the previous sentence).
- Swipe left/right while paused: scrub; hold and move vertically: browse surrounding text.
- Swipe up/down while paused: WPM up/down.
- Tap the bottom-right footer: cycle progress / time remaining / battery / pace-vs-average.
- Tap the top-right battery label: cycle percentage / time remaining / voltage.

### Motion

- Flick while reading: previous sentence.
- Face-down while playing: pause (motion gestures toggle in Display settings).
- Set down flat: standby. Set down screen-down: screen off. Lift: wake.

## Main menu

`Resume · Chapters · Mark finished · Bookmarks · Starred · Reading stats · Books · Articles · Focus Timer · Settings · SD card check · RSS feeds · Companion sync · USB transfer · Power off`

Settings are grouped into `Display` (theme, brightness, screensaver, reading labels, language, motion, daily goal), `Typography` (font, size, phantom words, purple highlight, tracking, anchor, guides, preview), `Word pacing` (reading mode, ramp-in, WPM, long-word/complexity/punctuation/clause delays, pause behaviour), `Wi-Fi`, and `Firmware update`.

## Focus Timer

Orientation-guided work/break blocks. Start a block, flip or place the device as prompted, and read normally during work blocks — the timer keeps counting in the background and plays a completion arpeggio with a word-count summary when the block ends. Reading sprints track words per block.

## iPhone companion app

The upstream iPhone app source lives in [`ios/RSVPNanoCompanion`](ios/RSVPNanoCompanion/README.md) and can be installed with Xcode. Its distribution status is tracked in the [upstream repository](https://github.com/ionutdecebal/rsvpnano). The web companion covers the same workflows from any browser, including Android.

## Build from source

```bash
pio run                      # build (waveshare_esp32s3_usb_msc env)
pio run -t upload            # flash a connected device
pio test -e native_test      # run the host-side test suite
pio device monitor           # serial output (use the debug_serial env for USB logs)
```

Release assets for the browser flasher and OTA:

```bash
python3 tools/export_web_firmware.py --version v0.1.0
```

Tagging `v*` builds and publishes a GitHub Release automatically; the web flasher redeploys on publish.

## License

MIT, same as upstream. See [LICENSE](LICENSE).

The embedded OpenDyslexic and Atkinson Hyperlegible typefaces are included under the SIL Open Font License — see [third_party/opendyslexic/OFL.txt](third_party/opendyslexic/OFL.txt) and [third_party/atkinson-hyperlegible/OFL.txt](third_party/atkinson-hyperlegible/OFL.txt).
