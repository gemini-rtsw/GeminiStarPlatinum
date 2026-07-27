"""Positional data sources for the GeminiStarPlatinum live feed bridge.

A DataSource produces the positional payload that the TCP server streams to the
Unreal sim. The payload schema is the ONLY format to be shared with the C++
side (ULiveDataFeed::ApplyLine):

    {
        "azim"        : float,   # telescope azimuth, degrees
        "elev"        : float,   # telescope elevation, degrees in the SIM frame:
                                 #   0 = zenith, -90 = horizon (= TCS altitude - 90)
        "cass"        : float,   # Cassegrain rotator, degrees
        "dome_twist"  : float,   # dome azimuth/twist, degrees
        "top_shutter" : float,   # top shutter swing, degrees
        "bot_shutter" : float,   # bottom shutter swing, degrees
        "vent"        : float,   # west AND east vent slide, Unreal position offset
        "t"           : float,   # wall-clock time of payload, seconds since epoch
        "age"         : float,   # seconds since the oldest value in this payload was last updated
        "stale"       : bool,    # True if age > STALE_AFTER_S, False otherwise
    }

Add a new source by subclassing DataSource and implementing read(). Selection
happens in server.py via the --source flag.
"""

from __future__ import annotations

import math
import time
from abc import ABC, abstractmethod


# Keys the Unreal side understands. Kept here so a source can be validated/documented
# against one list.
PAYLOAD_KEYS = ("azim", "elev", "cass", "dome_twist", "top_shutter", "bot_shutter", "vent")

def lin_map(x, x0, x1, y0, y1) -> float:
    """Affine map of x from [x0, x1] to [y0, y1]. Unclamped; the sim models clamp."""
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0)

def pct_to_range(p, lo, hi) -> float:
    return lin_map(p, 0.0, 1.0, lo, hi)

# Shutter calibration anchors: real TCS PV reading (degrees) <-> sim frame (degrees).
# Sim endpoints come from UDomeModel::SetOpen / TopShutterSwing / BotShutterSwing limits.
# CLOSED readings measured on the real TCS. OPEN readings are PROVISIONAL: they assume the
# PV tracks the same physical hinge 1:1 (offset-only map) — replace with measured values
# once the shutters are observed fully open.
TOP_SHUTTER_DEG_CLOSED = 11.5
TOP_SHUTTER_DEG_OPEN   = 101.5  # provisional (= closed + 90 deg sim travel)
TOP_SHUTTER_SIM_CLOSED = -7.0
TOP_SHUTTER_SIM_OPEN   = 83.0
BOT_SHUTTER_DEG_CLOSED = 12.1
BOT_SHUTTER_DEG_OPEN   = 2.6    # provisional (= closed - 9.5 deg sim travel)
BOT_SHUTTER_SIM_CLOSED = -3.5
BOT_SHUTTER_SIM_OPEN   = -13.0

class DataSource(ABC):
    """Abstract positional data source."""

    @abstractmethod
    def read(self) -> dict:
        """Return a payload dict (see module docstring for schema)."""
        raise NotImplementedError


class MockSource(DataSource):
    """Synthetic motion so the whole pipeline is testable with no hardware.

    Drives each axis with a slow sine sweep within its physical-ish limits and
    sweeps the shutter/vents' positional ranges.
    """

    def __init__(self) -> None:
        self._t0 = time.monotonic()

    def read(self) -> dict:
        t = time.monotonic() - self._t0
        return {
            "azim": 180.0 * math.sin(t * 0.10),          # [-180, 180]
            "elev": -60.0 + 40.0 * math.sin(t * 0.07),   # sweeps around -60
            "cass": 120.0 * math.sin(t * 0.05),
            "dome_twist": 180.0 * math.sin(t * 0.08),
            "top_shutter": 38.0 + 30.0 * math.sin(t*0.05),
            "bot_shutter": -8.25 + 4.75 * math.sin(t*0.05),
            "vent": 250.0 + 150.0 * math.sin(t*0.09),
        }


class EpicsSource(DataSource):
    """Reads live values from a TCS EPICS feed via Channel Access (pyepics).

    pyepics is imported lazily so the mock path has no hard dependency on it.
    PV names are supplied as a mapping {payload_key: pv_name}; see pv_map.py.
    """

    CONNECT_TIMEOUT_S = 8.0

    def __init__(self, pv_map: dict[str, str]) -> None:
        # LOAD-BEARING: this import must stay lazy. libca reads EPICS_CA_NAME_SERVERS /
        # EPICS_CA_ADDR_LIST / EPICS_CA_AUTO_ADDR_LIST once, when its context is created on
        # first import of `epics`. server.py sets those from --name_servers/--ca_addr in
        # main(), which runs after its own top-level `import data_source` -- so importing
        # epics at module level here (or anywhere imported at module level) would silently
        # break both flags and fall back to UDP broadcast. cf. tools/test_listen.py.
        try:
            import epics  # noqa: F401  (lazy import)
        except ImportError as exc:  # pragma: no cover - depends on environment
            raise RuntimeError(
                "EpicsSource requires pyepics. Install with `pip install pyepics`."
            ) from exc

        from epics import PV  # type: ignore

        self._pvs = {key: PV(name) for key, name in pv_map.items()}

        # Over the tunnel, initial connection takes seconds. read() only guards with
        # pv.connected, so without this a dead tunnel or a wrong PV name is indistinguishable
        # from "no motion" -- the bridge just streams empty payloads. Report per PV instead.
        # Non-fatal: the server still runs, and serve_client carries last known values forward.
        #
        # One shared deadline, not a full timeout each: PV() above already kicked off every
        # search concurrently on pyepics' CA thread, so waiting per PV would serialize an
        # 8 s stall per dead channel (~1 min of dead air before the bridge starts listening).
        deadline = time.monotonic() + self.CONNECT_TIMEOUT_S
        for key, pv in self._pvs.items():
            remaining = max(0.0, deadline - time.monotonic())
            if pv.wait_for_connection(timeout=remaining):
                print(f"[ OK ] {pv_map[key]:20} = {pv.get(timeout=self.CONNECT_TIMEOUT_S)!r}")
            else:
                print(f"[FAIL] {pv_map[key]:20} -- no connection")

    def read(self) -> dict:
        raw: dict = {}
        for key, pv in self._pvs.items():
            value = pv.get(timeout=0.2) if pv.connected else None
            if value is not None:
                raw[key] = float(value)

        payload: dict = {}
        for key in {"azim", "elev", "cass", "dome_twist"}:
            if key in raw:
                payload[key] = raw[key]

        # Shutter PVs report degrees in the TCS frame. Map to the sim frame using the
        # calibration anchors above (closed measured, open provisional).
        if "top_shutter" in raw:
            payload["top_shutter"] = lin_map(raw["top_shutter"],
                                             TOP_SHUTTER_DEG_CLOSED, TOP_SHUTTER_DEG_OPEN,
                                             TOP_SHUTTER_SIM_CLOSED, TOP_SHUTTER_SIM_OPEN)
        if "bot_shutter" in raw:
            payload["bot_shutter"] = lin_map(raw["bot_shutter"],
                                             BOT_SHUTTER_DEG_CLOSED, BOT_SHUTTER_DEG_OPEN,
                                             BOT_SHUTTER_SIM_CLOSED, BOT_SHUTTER_SIM_OPEN)

        # Vent PVs report 0-100% open. Average the two independent vent slides into a
        # single value for Unreal. Sim endpoints are magic numbers from physical limits.
        vents = [raw[k] for k in ("vent_west", "vent_east") if k in raw]
        if vents:
            payload["vent"] = pct_to_range(sum(vents)/len(vents)/100.0, 0.0, 500.0)

        # TCS reports altitude (0 = horizon, +90 = zenith); the sim frame is 0 = zenith,
        # -90 = horizon. Same mapping as UObservatoryCoordinator::MapAltToElevTarget.
        # Deliberately WITHOUT that function's ElevZeroOffset term: that offset is the
        # operator's manual pointing adjustment, not a frame calibration, so it must not be
        # baked into measured telemetry.
        if "elev" in payload:
            payload["elev"] = raw["elev"] - 90.0


        return payload
