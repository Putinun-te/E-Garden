# E-Garden LoRa protocol (v2)

All nodes talk over one LoRa channel: 433 MHz, sync word `0xAB`. The radio (SX127x) is half-duplex: a node cannot receive while it transmits, so every node goes back to receive mode right after sending, and commands are acknowledged and retried.

The codec is in [`libraries/EGarden/src/EGardenProtocol.cpp`](../libraries/EGarden/src/EGardenProtocol.cpp) and is unit tested on a PC (`pio test -e native`).

## Frame

One LoRa packet carries one ASCII frame, at most 200 bytes:

```
EG2,<src>,<dst>,<seq>,<type>,<key=value;key=value...>*<crc>
```

| Part | Meaning |
|---|---|
| `EG2` | Protocol marker and version |
| `src`, `dst` | Node addresses, 0–255. `dst = 0` is broadcast |
| `seq` | Sender's message counter, 0–65535, wraps around |
| `type` | `AIR`, `WTR`, `CMD`, `ACK` or `STA` |
| body | Up to 8 `key=value` pairs separated by `;`. Keys are 1–7 characters `a-z0-9`; values are 1–15 printable characters without `, ; * =` |
| `crc` | CRC-16/CCITT-FALSE of every byte before `*`, as 4 upper-case hex digits |

A frame with a wrong CRC, a bad header, an unknown type or a malformed field is dropped and logged. A node only acts on frames whose `dst` is its own address or broadcast.

Example: `EG2,2,1,42,WTR,ph=6.1;ec=1650;t=27.5*1A2B` (the CRC here is illustrative).

## Addresses

| Address | Node |
|---|---|
| 1 | E-Garden Hub (Master node) |
| 2 | Water sensor node |
| 3 | Air sensor node |
| 4 | Pump node |

## Message types

### `AIR`: air sensor node to hub

| Key | Unit | Notes |
|---|---|---|
| `t` | °C | Air temperature |
| `h` | %RH | Humidity |
| `lux` | lx | Light |
| `bat` | % | Battery estimate |
| `err` | | Present only if a sensor failed, e.g. `aht`, `bh1750` or `aht+bh1750`. The failed sensor's fields are left out |

### `WTR`: water sensor node to hub

| Key | Unit | Notes |
|---|---|---|
| `ph` | pH | |
| `ec` | sensor units | Raw register / 10, as in v1 |
| `t` | °C | Water temperature from the pH probe |
| `err` | | `ph`, `ec` or `ph+ec` when a Modbus read failed (timeout or bad CRC). The failed fields are left out |

### `CMD`: hub to a node

| `op` | Other keys | Target | Meaning |
|---|---|---|---|
| `pump` | `on=1` or `on=0` | Pump node | Switch the water pump. The pump node turns itself off after 90 s regardless |
| `dose` | `ch=A`, `B` or `ACID`; `n=1..5` | Water node | Run `n` dose units on one channel |
| `stop` | `ch=A`, `B`, `ACID` or `ALL` | Water node | Stop a running dose immediately |

### `ACK`: node to hub, reply to a `CMD`

| Key | Notes |
|---|---|
| `ack` | `seq` of the command being acknowledged |
| `ok` | `1` = accepted, `0` = refused |
| `err` | Reason when refused: `bad_cmd`, `busy`, `limit_cmd`, `limit_hour`, `too_soon`, `no_reading`, `stale`, `out_of_range` |

### `STA`: node status, sent after a change

| Node | Keys |
|---|---|
| Pump | `on=0/1`, `left` = seconds until the auto-off |
| Water | `dose` = running channel or `none`, `left` = units left |

## Reliability

- The hub sends a `CMD`, then waits for the matching `ACK`. Without an ACK it resends up to 3 times, after 1.5 s plus a random 0–500 ms backoff to avoid repeated collisions.
- Receivers remember the last `seq` per sender. A repeated command (same `src` and `seq`) is acknowledged again but not executed twice, so a lost ACK cannot double a dose.
- Sensor nodes do not wait for an ACK; a lost reading is replaced by the next one.

## Changes from v1

v1 frames looked like `3@2/6.50&1650$27.50#`: fields were identified only by their position between `@ / & $ # ^`, there was no destination, no checksum and no acknowledgement. The water node also ignored the `Stop(...)` commands the hub sent. v2 is not compatible with v1, so all four nodes must be flashed together.
