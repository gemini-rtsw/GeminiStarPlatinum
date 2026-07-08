#!/usr/bin/env python
"""Test EPICS soft IOC for GeminiStarPlatinum.

Serves a set of dome / mount / CRCS channels over Channel Access so that
other applications (e.g. the Unreal client) can read and write them without
the real control system.

Built on caproto, a pure-Python Channel Access implementation. It needs no
EPICS base install and installs from a single `pip install caproto`, which
makes it painless on Windows.

Run (static values):
    python test_ioc.py

Run with simulated motion (dome rotates, shutters breathe, mount slews):
    python test_ioc.py --simulate

Every PV is writable, so you can also poke values with any CA client
(caput, pyepics, CS-Studio, the Unreal client, ...) and read them back.
"""

import argparse
import math

from caproto.server import PVGroup, pvproperty, run


class GeminiTestIOC(PVGroup):
    """Group of test PVs. caproto serves each pvproperty as one channel."""

    # Enclosure controller (ec) -----------------------------------------------
    top_shtr_pos = pvproperty(value=0.0, name="ec:topShtrPos", precision=2, units="%")
    bot_shtr_pos = pvproperty(value=0.0, name="ec:botShtrPos", precision=2, units="%")
    dome_pos = pvproperty(value=0.0, name="ec:domePos", precision=2, units="deg")
    east_vent = pvproperty(value=0.0, name="ec:eastVentGatePos", precision=2, units="%")
    west_vent = pvproperty(value=0.0, name="ec:westVentGatePos", precision=2, units="%")

    # Mount controller (mc) ---------------------------------------------------
    el_pos = pvproperty(value=45.0, name="mc:elCurrentPos", precision=4, units="deg")
    az_pos = pvproperty(value=180.0, name="mc:azCurrentPos", precision=4, units="deg")

    # Cassegrain rotator (cr) -------------------------------------------------
    cr_pos = pvproperty(value=0.0, name="cr:crCurrentPos", precision=4, units="deg")

    def __init__(self, *args, simulate=False, **kwargs):
        super().__init__(*args, **kwargs)
        self._simulate = simulate

    @dome_pos.startup
    async def dome_pos(self, instance, async_lib):
        """Optional simulation loop. Only sweeps values when --simulate is set."""
        if not self._simulate:
            return
        t = 0.0
        while True:
            await self.dome_pos.write((t * 5.0) % 360.0)
            await self.top_shtr_pos.write(50.0 + 50.0 * math.sin(t * 0.2))
            await self.bot_shtr_pos.write(50.0 + 50.0 * math.sin(t * 0.2 + 0.5))
            await self.az_pos.write((180.0 + t * 2.0) % 360.0)
            await self.el_pos.write(-45.0 - 20.0 * math.sin(t * 0.1))
            await self.cr_pos.write((t * 3.0) % 360.0)
            t += 1.0
            await async_lib.library.sleep(1.0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="continuously sweep the PV values instead of leaving them static",
    )
    # caproto's run() reads networking config from EPICS_CA_* env vars; we
    # don't add its full CLI here to keep this simple.
    args = parser.parse_args()

    # Empty prefix: the channel names in each pvproperty (e.g. "ec:domePos")
    # already carry their own namespace and are served verbatim.
    ioc = GeminiTestIOC(prefix="", simulate=args.simulate)

    print("Test IOC running. Serving channels:")
    for pv in ioc.pvdb:
        print(f"  {pv}")
    print("\nPress Ctrl-C to stop.")

    run(ioc.pvdb, log_pv_names=False)


if __name__ == "__main__":
    main()
