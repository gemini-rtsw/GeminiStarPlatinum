"""Mapping from payload keys to TCS EPICS PV names.

PV naming is centralized here to ensure consistent standards throughout the python 

The names below are reflective of the actual Gemini EPICS PV keys 
"""

DEFAULT_PV_MAP = {
    "azim"           : "mc:azCurrentPos",
    "elev"           : "mc:elCurrentPos",
    "cass"           : "cr:crCurrentPos",
    "dome_twist"     : "ec:domePos",
    "top_shutter"    : "ec:topShtrPos",
    "bot_shutter"    : "ec:botShtrPos",
    "vent_west"      : "ec:westVentGatePos",
    "vent_east"      : "ec:eastVentGatePos",
}
