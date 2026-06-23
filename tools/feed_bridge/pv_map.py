"""Mapping from payload keys to TCS EPICS PV names.

Centralizing PV naming here means the EPICS source and any future tooling share one
definition, and updating to real observatory PV names is a single-file change.

The names below are PLACEHOLDERS — replace them with the actual Gemini TCS PVs.
"""

DEFAULT_PV_MAP = {
    "azim": "tcs:telescope:azimuth",
    "elev": "tcs:telescope:elevation",
    "cass": "tcs:telescope:cassRotator",
    "dome_twist": "tcs:dome:azimuth",
    "dome_open": "tcs:dome:shutterOpen",
}
