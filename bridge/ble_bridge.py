import asyncio
import logging
import time
from .protocol import RX, TX, encode

log = logging.getLogger(__name__)
SCAN_TIMEOUT = 10
SCAN_BACKOFF_INITIAL = 3
SCAN_BACKOFF_MAX = 10


class BridgeLink:
    """Runtime signals shared by the single query task and BLE task."""

    def __init__(self):
        self.connected = asyncio.Event()
        self.refresh = asyncio.Event()
        self.changed = asyncio.Event()
        self.session = 0
        self.fresh_session = None
        self.revision = 0

    def connect(self):
        self.session += 1
        self.connected.set()
        self.refresh.set()
        return self.session

    def disconnect(self, session):
        if session == self.session:
            self.connected.clear()
            self.refresh.set()  # Cancel the query task's interval wait.

    def publish(self, *, success_session=None):
        if success_session is not None:
            self.fresh_session = success_session
        self.revision += 1
        self.changed.set()


def matches_device(dev, adv, device=None):
    # CoreBluetooth may retain the device name while a new advertisement has
    # not yet included its scan response. Use that cached name as a fallback.
    return dev.address == device if device else (adv.local_name or getattr(dev, "name", None)) == "Codex Ink"


async def send_updates(client, state, link, session, disconnected):
    sent_revision = None
    while client.is_connected and not disconnected.is_set():
        # No awaits between clearing the signal and taking the snapshot. Updates
        # arriving during fragmented writes leave the signal set for the next pass.
        link.changed.clear()
        revision = link.revision
        windows = state.windows(failed=link.fresh_session != session)
        if revision != sent_revision:
            if windows:
                data = encode(state.next_sequence(), windows)
                # ATT minimum MTU=23; keep acknowledged 20-byte fragments.
                for offset in range(0, len(data), 20):
                    if disconnected.is_set() or not client.is_connected:
                        return
                    await client.write_gatt_char(RX, data[offset:offset + 20], response=True)
                log.info("BLE quota packet sent (%s bytes)", len(data))
            sent_revision = revision
        waiters = [asyncio.create_task(e.wait()) for e in (link.changed, disconnected)]
        try:
            await asyncio.wait(waiters, return_when=asyncio.FIRST_COMPLETED)
        finally:
            for task in waiters:
                task.cancel()
            await asyncio.gather(*waiters, return_exceptions=True)


async def ble_session(target, state, link, client_type):
    """Keep callbacks scoped to this connection, including late disconnects."""
    loop = asyncio.get_running_loop()
    disconnected = asyncio.Event()
    session = None
    last_request = float("-inf")

    def on_disconnect():
        disconnected.set()
        if session is not None:
            link.disconnect(session)

    def on_refresh():
        nonlocal last_request
        now = time.monotonic()
        if (session == link.session and link.connected.is_set()
                and not disconnected.is_set() and now - last_request >= 5):
            last_request = now
            link.refresh.set()

    try:
        async with client_type(
            target, disconnected_callback=lambda _: loop.call_soon_threadsafe(on_disconnect)
        ) as client:
            await client.start_notify(
                TX, lambda _, data: loop.call_soon_threadsafe(on_refresh) if b"R" in data else None)
            if client.is_connected and not disconnected.is_set():
                session = link.connect()
                # The connection already triggers the first query. Coalesce a
                # queued page-open R notification arriving just after subscribe.
                last_request = time.monotonic()
                log.info("BLE connected")
                await send_updates(client, state, link, session, disconnected)
    finally:
        if session is not None:
            link.disconnect(session)


async def ble_loop(state, link, device=None):
    from bleak import BleakClient, BleakScanner
    backoff = SCAN_BACKOFF_INITIAL
    while True:
        previous_session = link.session
        try:
            target = await BleakScanner.find_device_by_filter(
                lambda dev, adv: matches_device(dev, adv, device),
                timeout=SCAN_TIMEOUT)
            if target is None:
                log.info("waiting for Codex Ink")
            else:
                await ble_session(target, state, link, BleakClient)
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            # CoreBluetooth exceptions may include identifiers; never log payloads or credentials.
            log.warning("BLE reconnect (%s)", type(exc).__name__)
        finally:
            if link.session != previous_session:
                backoff = SCAN_BACKOFF_INITIAL
        log.info("BLE discovery retry in %s seconds", backoff)
        await asyncio.sleep(backoff)
        backoff = min(SCAN_BACKOFF_MAX, backoff * 2)
