"""Test EPICS access through Hawi's workstation tunnel. Just run: python3 epics_tunnel_test.py"""
import os

HOST = "10.26.70.200"  # Hawi's workstation
os.environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"
os.environ["EPICS_CA_ADDR_LIST"] = ""
os.environ["EPICS_CA_NAME_SERVERS"] = f"{HOST}:5064 {HOST}:5065"

import epics  # must be imported AFTER the env vars above

for pv in ("tcs:currentAz", "tcs:currentEl", "cr:crCurrentPos", "ec:domePos", "ec:topShtrPos", "ec:botShtrPos", "ec:westVentGatePos", "ec:eastVentGatePos"):
    p = epics.PV(pv, connection_timeout=8.0)
    if p.wait_for_connection(timeout=8.0):
        print(f"[ OK ] {pv:16} = {p.get(timeout=8.0)!r}")
    else:
        print(f"[FAIL] {pv:16} -- no connection")
    p.disconnect()