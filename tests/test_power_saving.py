import asyncio
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

from bridge.ble_bridge import BridgeLink, ble_loop, ble_session, send_updates
from bridge.main import poll, run
from bridge.quota_normalizer import Window
from bridge.quota_provider import QuotaError
from bridge.state import State
from scripts.update_firmware import MANAGED_CONFIG, configure, update
from scripts.integrate_power import replace_once


def windows(used=25):
    return {'h': Window(used, 1800000000), 'w': None}


class FakeClient:
    def __init__(self, disconnected_callback=None, notify_error=None):
        self.is_connected = True
        self.disconnected_callback = disconnected_callback
        self.notify_error = notify_error
        self.notify = None
        self.on_write = None
        self.chunks = []
        self.packets = []
        self.buffer = bytearray()

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        self.is_connected = False
        if self.disconnected_callback:
            self.disconnected_callback(self)

    async def start_notify(self, characteristic, callback):
        if self.notify_error:
            raise self.notify_error
        self.notify = callback

    async def write_gatt_char(self, characteristic, data, response):
        assert response and len(data) <= 20
        self.chunks.append(data)
        self.buffer.extend(data)
        if data.endswith(b'\n'):
            self.packets.append(json.loads(self.buffer))
            self.buffer.clear()
        if self.on_write:
            await self.on_write(self)
        await asyncio.sleep(0)


class PowerSavingTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.state = State(Path(self.tmp.name) / 'state.json')
        self.link = BridgeLink()
        self.args = SimpleNamespace(interval=300, enable_probe=False, codex='codex')
        self.provider = SimpleNamespace(fetch=AsyncMock(return_value=windows()), last_source='test')
        # Drive real asyncio deadlines with a virtual monotonic clock. Advancing
        # an hour exercises timeout behavior without sleeping or changing wall time.
        loop = asyncio.get_running_loop()
        debug = loop.get_debug()
        loop.set_debug(False)
        self.addCleanup(loop.set_debug, debug)
        self.now = loop.time()
        clock = SimpleNamespace(monotonic=lambda: self.now)
        mock = patch.object(loop, 'time', lambda: self.now)
        mock.start()
        self.addCleanup(mock.stop)
        for name in ('bridge.main.time', 'bridge.ble_bridge.time'):
            mock = patch(name, clock)
            mock.start()
            self.addCleanup(mock.stop)

    async def settle(self):
        for _ in range(40):
            await asyncio.sleep(0)

    async def advance(self, seconds):
        await self.settle()
        self.now += seconds
        await self.settle()

    async def stop(self, task):
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

    def start(self, awaitable):
        task = asyncio.create_task(awaitable)
        self.addAsyncCleanup(self.stop, task)
        return task

    def start_poll(self):
        return self.start(poll(self.args, self.state, self.provider, self.link))

    async def test_offline_waits_without_queries_then_polls_every_five_minutes(self):
        self.start_poll()
        await self.advance(3600)
        self.provider.fetch.assert_not_awaited()
        self.assertFalse(self.state.path.exists())
        self.link.connect()
        await self.settle()
        self.assertEqual(self.provider.fetch.await_count, 1)
        await self.advance(299)
        self.assertEqual(self.provider.fetch.await_count, 1)
        await self.advance(1)
        self.assertEqual(self.provider.fetch.await_count, 2)
        self.link.disconnect(self.link.session)
        await self.advance(3600)
        self.assertEqual(self.provider.fetch.await_count, 2)

    async def test_disconnect_during_throttle_does_not_start_query(self):
        session = self.link.connect()
        self.start_poll()
        await self.advance(1)
        self.link.refresh.set()
        await self.settle()
        self.link.disconnect(session)
        await self.advance(4)
        self.assertEqual(self.provider.fetch.await_count, 1)
        self.link.connect()
        await self.settle()
        self.assertEqual(self.provider.fetch.await_count, 2)

    async def test_failure_backoff_survives_manual_requests_and_reconnection(self):
        self.state.update(windows())
        updated_at = self.state.data['updated_at']
        self.provider.fetch.side_effect = [QuotaError('network_error'), QuotaError('network_error'), windows()]
        with patch('bridge.main.random.uniform', return_value=1):
            session = self.link.connect()
            self.start_poll()
            await self.settle()
            self.assertEqual(self.link.revision, 1)
            self.assertEqual(self.state.windows()['h'].source, 'stale')
            self.assertEqual(self.state.data['updated_at'], updated_at)
            await self.advance(1)
            self.link.refresh.set()
            await self.settle()
            self.link.disconnect(session)
            self.link.connect()
            await self.advance(8)
            self.assertEqual(self.provider.fetch.await_count, 1)
            await self.advance(1)
            self.assertEqual(self.provider.fetch.await_count, 2)
            self.assertEqual(self.link.revision, 1)  # Repeated stale failure is not published.
            self.assertEqual(self.state.data['updated_at'], updated_at)
            await self.advance(20)
            self.assertEqual(self.provider.fetch.await_count, 3)
            self.assertEqual(self.link.revision, 2)
            self.assertEqual(self.state.windows()['h'].source, 'api')

    async def test_manual_requests_during_fetch_are_coalesced(self):
        release = asyncio.Event()

        async def fetch():
            await release.wait()
            return windows()

        self.provider.fetch.side_effect = fetch
        self.link.connect()
        self.start_poll()
        await self.settle()
        self.link.refresh.set()
        await self.settle()
        release.set()
        await self.advance(10)
        self.assertEqual(self.provider.fetch.await_count, 1)
        self.assertEqual(self.link.revision, 1)

    async def test_query_can_finish_offline_without_starting_another(self):
        release = asyncio.Event()

        async def fetch():
            await release.wait()
            return windows(40)

        self.provider.fetch.side_effect = fetch
        session = self.link.connect()
        self.start_poll()
        await self.settle()
        self.link.disconnect(session)
        release.set()
        await self.advance(3600)
        self.assertEqual(self.provider.fetch.await_count, 1)
        self.assertEqual(self.state.windows()['h'].used, 40)

    async def test_reconnect_during_fetch_requires_a_query_in_the_new_session(self):
        release = asyncio.Event()

        async def fetch():
            await release.wait()
            return windows()

        self.provider.fetch.side_effect = fetch
        session = self.link.connect()
        self.start_poll()
        await self.settle()
        self.link.disconnect(session)
        new_session = self.link.connect()
        release.set()
        await self.settle()
        self.assertEqual(self.provider.fetch.await_count, 1)
        self.assertEqual(self.link.fresh_session, session)
        await self.advance(5)
        self.assertEqual(self.provider.fetch.await_count, 2)
        self.assertEqual(self.link.fresh_session, new_session)

    async def test_once_queries_without_bluetooth_and_releases_file_lock(self):
        self.args.state = str(self.state.path)
        self.args.provider = 'app-server'
        self.args.once = True
        output = io.StringIO()
        with patch('bridge.main.CodexQuotaProvider', return_value=self.provider), \
                patch('bridge.main.ble_loop', new_callable=AsyncMock) as ble, \
                patch('sys.stdout', output):
            await run(self.args)
            await run(self.args)
        self.assertEqual(self.provider.fetch.await_count, 2)
        ble.assert_not_awaited()
        packets = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual(len(packets), 2)
        self.assertGreater(packets[1]['q'], packets[0]['q'])

    async def test_cached_packet_is_stale_and_idle_does_not_resend_or_write(self):
        self.state.update({'h': windows()['h'], 'w': Window(None, 1800001000, 'estimated')})
        saved = self.state.path.read_text()
        sequence = self.state.data['sequence']
        client = FakeClient()
        session = self.link.connect()
        with patch.object(self.state, 'save', wraps=self.state.save) as save:
            self.start(send_updates(client, self.state, self.link, session, asyncio.Event()))
            await self.settle()
            self.assertEqual(len(client.packets), 1)
            self.assertEqual(client.packets[0]['h']['s'], 'stale')
            self.assertEqual(client.packets[0]['w']['s'], 'estimated')
            await self.advance(3600)
            self.assertEqual(len(client.packets), 1)
            self.assertEqual(save.call_count, 1)
        self.assertEqual(self.state.data['sequence'], sequence + 1)
        persisted = json.loads(self.state.path.read_text())
        original = json.loads(saved)
        self.assertEqual(persisted['windows'], original['windows'])
        self.assertEqual(persisted['updated_at'], original['updated_at'])

    async def test_initial_snapshot_coalesces_pending_result_and_identical_success_sends(self):
        session = self.link.connect()
        self.state.update(windows())
        self.link.publish(success_session=session)
        client = FakeClient()
        self.start(send_updates(client, self.state, self.link, session, asyncio.Event()))
        await self.settle()
        self.assertEqual(len(client.packets), 1)
        self.assertEqual(client.packets[0]['h']['s'], 'api')
        self.state.update(windows())
        self.link.publish(success_session=session)
        await self.settle()
        self.assertEqual(len(client.packets), 2)
        self.assertEqual(client.packets[0]['h'], client.packets[1]['h'])
        self.assertGreater(client.packets[1]['q'], client.packets[0]['q'])

    async def test_update_during_fragments_preserves_snapshot_and_delivers_latest(self):
        self.state.update(windows(25))
        session = self.link.connect()
        self.link.publish(success_session=session)
        client = FakeClient()

        async def update_during_write(client):
            if len(client.chunks) == 1:
                self.state.update(windows(40))
                self.link.publish(success_session=session)

        client.on_write = update_during_write
        self.start(send_updates(client, self.state, self.link, session, asyncio.Event()))
        await self.settle()
        self.assertEqual([p['h']['u'] for p in client.packets], [25, 40])

    async def test_disconnect_stops_fragments_and_cancel_reclaims_event_waiters(self):
        self.state.update(windows())
        session = self.link.connect()
        disconnected = asyncio.Event()
        client = FakeClient()

        async def disconnect_during_write(client):
            disconnected.set()
            client.is_connected = False

        client.on_write = disconnect_during_write
        task = self.start(send_updates(client, self.state, self.link, session, disconnected))
        await self.settle()
        self.assertTrue(task.done())
        self.assertEqual(len(client.chunks), 1)
        before = asyncio.all_tasks()
        sender = self.start(send_updates(FakeClient(), self.state, self.link, session, asyncio.Event()))
        await self.settle()
        await self.stop(sender)
        self.assertEqual(asyncio.all_tasks(), before)

    async def test_empty_cache_waits_until_a_result_arrives(self):
        client = FakeClient()
        session = self.link.connect()
        self.start(send_updates(client, self.state, self.link, session, asyncio.Event()))
        await self.advance(600)
        self.assertEqual(client.chunks, [])
        self.assertFalse(self.state.path.exists())
        self.state.update({'h': None, 'w': None})
        self.link.publish(success_session=session)
        await self.settle()
        self.assertEqual(len(client.packets), 1)
        self.assertIsNone(client.packets[0]['h'])

    async def test_missing_devices_and_connection_errors_share_capped_backoff(self):
        scanner = SimpleNamespace(find_device_by_filter=AsyncMock(
            side_effect=[None, OSError(), None, OSError(), None, None, None, None]))
        fake_bleak = SimpleNamespace(BleakScanner=scanner, BleakClient=None)
        with patch.dict('sys.modules', {'bleak': fake_bleak}):
            task = self.start(ble_loop(self.state, self.link))
            await self.settle()
            for attempt, delay in enumerate((3, 6, 10, 10, 10, 10, 10), start=1):
                await self.advance(delay - 1)
                self.assertEqual(scanner.find_device_by_filter.await_count, attempt)
                await self.advance(1)
                self.assertEqual(scanner.find_device_by_filter.await_count, attempt + 1)
            await self.stop(task)

    async def test_successful_subscription_resets_discovery_backoff(self):
        clients = []

        def factory(target, disconnected_callback):
            client = FakeClient(disconnected_callback)
            clients.append(client)
            return client

        scanner = SimpleNamespace(find_device_by_filter=AsyncMock(side_effect=[None, None, object(), None]))
        fake_bleak = SimpleNamespace(BleakScanner=scanner, BleakClient=factory)
        with patch.dict('sys.modules', {'bleak': fake_bleak}):
            task = self.start(ble_loop(self.state, self.link))
            await self.advance(3)
            await self.advance(6)
            self.assertTrue(self.link.connected.is_set())
            clients[0].is_connected = False
            clients[0].disconnected_callback(clients[0])
            await self.settle()
            self.assertFalse(self.link.connected.is_set())
            await self.advance(2)
            self.assertEqual(scanner.find_device_by_filter.await_count, 3)
            await self.advance(1)
            self.assertEqual(scanner.find_device_by_filter.await_count, 4)
            await self.stop(task)

    async def test_subscription_failure_never_enables_queries(self):
        def factory(target, disconnected_callback):
            return FakeClient(disconnected_callback, notify_error=OSError())

        with self.assertRaises(OSError):
            await ble_session(object(), self.state, self.link, factory)
        self.assertFalse(self.link.connected.is_set())
        self.assertEqual(self.link.session, 0)

    async def test_page_open_refresh_during_subscription_queries_once_immediately(self):
        class StartupRefreshClient(FakeClient):
            async def start_notify(self, characteristic, callback):
                await super().start_notify(characteristic, callback)
                # Firmware flushes its queued page-open request as soon as TX
                # is subscribed, potentially before start_notify has returned.
                callback(None, b'R')
                await asyncio.sleep(0)

        clients = []

        def factory(target, disconnected_callback):
            client = StartupRefreshClient(disconnected_callback)
            clients.append(client)
            return client

        self.start_poll()
        self.start(ble_session(object(), self.state, self.link, factory))
        await self.settle()
        self.assertTrue(self.link.connected.is_set())
        self.provider.fetch.assert_awaited_once()
        self.assertEqual(len(clients[0].packets), 1)
        self.assertEqual(clients[0].packets[0]['h']['s'], 'api')
        await self.advance(10)
        self.provider.fetch.assert_awaited_once()

    async def test_delayed_page_open_notification_coalesces_with_initial_query(self):
        clients = []

        def factory(target, disconnected_callback):
            client = FakeClient(disconnected_callback)
            clients.append(client)
            return client

        self.start_poll()
        self.start(ble_session(object(), self.state, self.link, factory))
        await self.advance(2)
        clients[0].notify(None, b'R')
        await self.advance(8)
        self.provider.fetch.assert_awaited_once()
        # A later manual request still works before the 5-minute periodic poll.
        clients[0].notify(None, b'R')
        await self.settle()
        self.assertEqual(self.provider.fetch.await_count, 2)

    async def test_cancel_and_late_callbacks_do_not_clear_a_new_connection(self):
        clients = []

        def factory(target, disconnected_callback):
            client = FakeClient(disconnected_callback)
            clients.append(client)
            return client

        first = self.start(ble_session(object(), self.state, self.link, factory))
        await self.settle()
        await self.stop(first)
        self.assertFalse(self.link.connected.is_set())
        self.start(ble_session(object(), self.state, self.link, factory))
        await self.settle()
        self.link.refresh.clear()
        clients[0].disconnected_callback(clients[0])
        clients[0].notify(None, b'R')
        await self.settle()
        self.assertTrue(self.link.connected.is_set())
        self.assertFalse(self.link.refresh.is_set())
        await self.advance(5)
        clients[1].notify(None, b'R')
        await self.settle()
        self.assertTrue(self.link.refresh.is_set())
        self.link.refresh.clear()
        clients[1].notify(None, b'R')
        await self.settle()
        self.assertFalse(self.link.refresh.is_set())
        await self.advance(5)
        clients[1].notify(None, b'R')
        await self.settle()
        self.assertTrue(self.link.refresh.is_set())


class FirmwareConfigTests(unittest.TestCase):
    def test_integration_patch_is_idempotent_and_rejects_ambiguous_upstream(self):
        old = 'before\nunique anchor\nafter\n'
        updated = replace_once(old, 'unique anchor', 'managed replacement', 'policy')
        self.assertEqual(replace_once(updated, 'unique anchor', 'managed replacement', 'policy'), updated)
        for incompatible in ('anchor removed upstream', old + old):
            with self.assertRaisesRegex(ValueError, 'unsupported firmware integration anchor: policy'):
                replace_once(incompatible, 'unique anchor', 'managed replacement', 'policy')

    def test_config_is_idempotent_and_keeps_unmanaged_options(self):
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / 'sdkconfig'
            config.write_text('# vendor settings\nCONFIG_CPU_FREQUENCY=240\n'
                              'CONFIG_NEWLIB_NANO_FORMAT=y\n'
                              'CONFIG_LIBC_NEWLIB_NANO_FORMAT=y\n'
                              '# CONFIG_BT_ENABLED is not set\nCONFIG_BT_ENABLED=n\n'
                              '# CONFIG_BT_CTRL_MODEM_SLEEP is not set\n'
                              'CONFIG_BT_CTRL_LPCLK_SEL_RTC_SLOW=y\n')
            configure(config)
            first = config.read_text()
            configure(config)
            self.assertEqual(config.read_text(), first)
            self.assertIn('CONFIG_CPU_FREQUENCY=240\n', first)
            for key, value in MANAGED_CONFIG.items():
                lines = [line for line in first.splitlines()
                         if line.startswith(key + '=') or line == f'# {key} is not set']
                if value == 'n':
                    self.assertIn(lines, ([], [f'# {key} is not set']))
                else:
                    self.assertEqual(lines, [f'{key}={value}'])
            self.assertIn('CONFIG_BT_CTRL_MODEM_SLEEP=y\n', first)
            self.assertIn('CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y\n', first)
            self.assertIn('CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y\n', first)
            self.assertIn('# CONFIG_NEWLIB_NANO_FORMAT is not set\n', first)
            self.assertIn('# CONFIG_LIBC_NEWLIB_NANO_FORMAT is not set\n', first)

    def test_kconfig_output_does_not_get_rewritten_for_hidden_disabled_choices(self):
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / 'sdkconfig'
            original = '# Kconfig output\n' + ''.join(
                f'{key}={value}\n' for key, value in MANAGED_CONFIG.items() if value != 'n')
            config.write_text(original)
            with patch.object(Path, 'write_text') as write:
                configure(config)
            write.assert_not_called()

    def test_update_copies_only_codex_sources_and_managed_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'project'
            source = root / 'firmware/codex_monitor'
            source.mkdir(parents=True)
            (source / 'screen.cc').write_text('new source')
            output = Path(tmp) / 'build-copy'
            (output / 'main/codex_monitor').mkdir(parents=True)
            (output / 'main/codex_monitor/screen.cc').write_text('old source')
            vendor = output / 'main/official.cc'
            vendor.write_text('user customization')
            (output / 'CODEX_MONITOR_BASELINE.txt').write_text('baseline\n')
            (output / 'sdkconfig').write_text('CONFIG_USER_OPTION=y\n')
            with patch('scripts.update_firmware.ROOT', root), patch('sys.stdout', io.StringIO()), \
                    patch('scripts.update_firmware.integrate_power') as integrate:
                update(output)
                update(output)
            self.assertEqual(integrate.call_count, 2)
            integrate.assert_called_with(output.resolve())
            self.assertEqual((output / 'main/codex_monitor/screen.cc').read_text(), 'new source')
            self.assertEqual(vendor.read_text(), 'user customization')
            self.assertIn('CONFIG_USER_OPTION=y\n', (output / 'sdkconfig').read_text())

    def test_update_rejects_an_unprepared_official_checkout(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, 'not a prepared'):
                update(Path(tmp))

    def test_repeated_failure_does_not_write_or_renew_query_timestamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            state = State(Path(tmp) / 'cache.json')
            state.update(windows())
            updated_at = state.data['updated_at']
            with patch.object(state, 'save', wraps=state.save) as save:
                self.assertTrue(state.mark_failed())
                self.assertFalse(state.mark_failed())
                self.assertEqual(save.call_count, 1)
            self.assertEqual(state.data['updated_at'], updated_at)
            self.assertEqual(state.windows()['h'].source, 'stale')
