# Test IOC

A simple pure-Python EPICS soft IOC for testing Channel Access against the
GeminiStarPlatinum project. It serves dome, mount, and Cassegrain-rotator
channels so other applications (such as the Unreal client) can read and write
them without needing the real control system.

It is built on [caproto](https://caproto.github.io/caproto/), a pure-Python
Channel Access implementation. There is **no EPICS base install required** —
just Python and one `pip install`, which makes it easy on Windows.

## Channels served

| Description             | Channel               |
| ----------------------- | --------------------- |
| Top shutter position    | `ec:topShtrPos`       |
| Bottom shutter position | `ec:botShtrPos`       |
| Dome position           | `ec:domePos`          |
| East vent gate          | `ec:eastVentGatePos`  |
| West vent gate          | `ec:westVentGatePos`  |
| Mount elevation         | `mc:elCurrentPos`     |
| Mount azimuth           | `mc:azCurrentPos`     |
| CRCS angle              | `cr:crCurrentPos`     |

All channels are analog (double) and writable. Putting a value to any of
them updates the value the IOC serves.

## Running on Windows

### 1. Install Python

Install Python 3.8+ from [python.org](https://www.python.org/downloads/) (or
the Microsoft Store). In the python.org installer, tick **"Add Python to
PATH"**.

Verify in a Command Prompt or PowerShell:

```
python --version
```

### 2. Install dependencies

From this `TestIOC` folder:

```
python -m pip install -r requirements.txt
```

This installs `caproto`, which is pure Python — no compiler and no EPICS base
needed.

### 3. Run the IOC

Static values:

```
python test_ioc.py
```

Or with simulated motion (dome rotates, shutters breathe, mount slews), which
is handy for watching live updates:

```
python test_ioc.py --simulate
```

The IOC prints the channels it is serving and runs until you press `Ctrl-C`.

### 4. Networking notes (important on Windows)

Channel Access uses UDP broadcasts to find PVs. If clients run on **other
machines**, make sure:

- Windows Firewall allows `python.exe` on UDP/TCP port **5064** and UDP
  **5065**. You will usually get a firewall prompt the first time the IOC
  runs — click **Allow** (allow on private networks at least).
- If broadcast auto-discovery does not work across your subnet, point clients
  at this host explicitly by setting these environment variables on the
  **client** side:

  ```
  set EPICS_CA_ADDR_LIST=<ip-of-this-machine>
  set EPICS_CA_AUTO_ADDR_LIST=NO
  ```

- To bind the IOC to a specific interface (e.g. when the machine has several),
  set this on the **server** side before launching:

  ```
  set EPICS_CAS_INTF_ADDR_LIST=<ip-to-serve-on>
  ```

## Running on Linux

caproto is pure Python, so the IOC runs the same way on Linux.

### 1. Install dependencies

Most distros ship Python 3 already (`python3 --version`). From this `TestIOC`
folder, ideally in a virtual environment:

```
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt
```

(You can skip the venv and `pip install --user -r requirements.txt` instead if
you prefer.)

### 2. Run the IOC

```
python3 test_ioc.py            # static values
python3 test_ioc.py --simulate # with simulated motion
```

Press `Ctrl-C` to stop.

### 3. Networking notes

The same Channel Access environment variables apply, using shell `export`
syntax instead of `set`. If clients are on other machines and broadcast
auto-discovery fails, point them at this host (on the **client** side):

```
export EPICS_CA_ADDR_LIST=<ip-of-this-machine>
export EPICS_CA_AUTO_ADDR_LIST=NO
```

To bind the IOC to a specific interface (on the **server** side, before
launching):

```
export EPICS_CAS_INTF_ADDR_LIST=<ip-to-serve-on>
```

If you have a firewall (`firewalld`, `ufw`, ...), allow UDP/TCP **5064** and
UDP **5065** so remote clients can reach the IOC.

## Testing the channels

caproto ships its own command-line CA client, so you can test from another
Command Prompt without installing EPICS base:

```
python -m caproto.commandline.get ec:domePos
python -m caproto.commandline.put mc:azCurrentPos 270
python -m caproto.commandline.monitor ec:topShtrPos
```

If you have EPICS base tools installed, the standard commands work too:

```
caget ec:domePos
caput mc:azCurrentPos 270
camonitor ec:topShtrPos
```

Or use any Channel Access client (pyepics, CS-Studio, the Unreal client, ...).

## Quick test with pyepics

```
python -m pip install pyepics
python -c "import epics; print(epics.caget('ec:domePos'))"
```
