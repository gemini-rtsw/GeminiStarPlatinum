"""Positional data sources for the GeminiStarPlatinum live feed bridge.

A DataSource produces the positional payload that the TCP server streams to the
Unreal sim. The payload schema is the single source of truth shared with the C++
side (ULiveDataFeed::ApplyLine):

    {
        "azim":       float,   # telescope azimuth, degrees
        "elev":       float,   # telescope elevation, degrees
        "cass":       float,   # Cassegrain rotator, degrees
        "dome_twist": float,   # dome azimuth/twist, degrees
        "dome_open":  bool,    # dome shutter + vents open/closed
    }

Add a new source by subclassing DataSource and implementing read(). Selection
happens in server.py via the --source flag, so nothing else needs to change.
"""

from __future__ import annotations

import math
import time
from abc import ABC, abstractmethod


# Keys the Unreal side understands. Kept here so a source can be validated/documented
# against one list.
PAYLOAD_KEYS = ("azim", "elev", "cass", "dome_twist", "dome_open")


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
            "dome_open": (int(t) // 15) % 2 == 0,         # toggle every 15s
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
        payload: dict = {}
        for key, pv in self._pvs.items():
            value = pv.get()
            if value is None:
                continue
            payload[key] = bool(value) if key == "dome_open" else float(value)
        return payload
