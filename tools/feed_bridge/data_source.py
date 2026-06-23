"""Positional data sources for the GeminiStarPlatinum live feed bridge.

A DataSource produces the positional payload that the TCP server streams to the
Unreal sim. The payload schema is the ONLY format to be shared with the C++
side (ULiveDataFeed::ApplyLine):

    {
        "azim"        : float,   # telescope azimuth, degrees
        "elev"        : float,   # telescope elevation, degrees
        "cass"        : float,   # Cassegrain rotator, degrees
        "dome_twist"  : float,   # dome azimuth/twist, degrees
        "top_shutter" : float,   # top shutter swing, degrees
        "bot_shutter" : float,   # bottom shutter swing, degrees
        "vent"        : float,   # west AND east vent slide, Unreal position offset
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

def pct_to_range(p, lo, hi) -> float:
    return (1-p)*lo + p*hi

class DataSource(ABC):
    """Abstract positional data source."""

    @abstractmethod
    def read(self) -> dict:
        """Return a payload dict (see module docstring for schema)."""
        raise NotImplementedError


class MockSource(DataSource):
    """Synthetic motion so the whole pipeline is testable with no hardware.

    Drives each axis with a slow sine sweep within its physical-ish limits and
    toggles the dome open/closed on a fixed period.
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

    def __init__(self, pv_map: dict[str, str]) -> None:
        try:
            import epics  # noqa: F401  (lazy import)
        except ImportError as exc:  # pragma: no cover - depends on environment
            raise RuntimeError(
                "EpicsSource requires pyepics. Install with `pip install pyepics`."
            ) from exc

        from epics import PV  # type: ignore

        self._pvs = {key: PV(name) for key, name in pv_map.items()}

    def read(self) -> dict:
        raw: dict = {}
        for key, pv in self._pvs.items():
            value = pv.get()
            if value is not None:
                raw[key] = float(value)

        payload: dict = {}
        for key in {"azim", "elev", "cass", "dome_twist"}:
            if key in raw:
                payload[key] = raw[key]

        # Shutter and vent values are stored in EPICS as 0-100% open. Convert to
        # Unreal's expected range.
        # Chosen lo, hi values are magic numbers derived from physical limits of telescope and dome.
        if "top_shutter" in raw:
            payload["top_shutter"] = pct_to_range(raw["top_shutter"]/100.0, -7.0, 83.0)
        if "bot_shutter" in raw:
            payload["bot_shutter"] = pct_to_range(raw["bot_shutter"]/100.0, -3.5, -13.0)
        
        # Average the two independent vent slide percentages into a single value for Unreal
        vents = [raw[k] for k in ("vent_west", "vent_east") if k in raw]
        if vents:
            payload["vent"] = pct_to_range(sum(vents/len(vents)/100.0), 0.0, 500.0)
        return payload
