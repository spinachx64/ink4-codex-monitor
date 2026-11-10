from types import SimpleNamespace
import unittest
from unittest.mock import patch
from bridge.ble_bridge import matches_device
from bridge.main import arguments

class Ink4BridgeTests(unittest.TestCase):
    def test_default_discovery_avoids_watch_and_unrelated_nus_devices(self):
        dev=SimpleNamespace(address='ink4-id')
        self.assertTrue(matches_device(dev,SimpleNamespace(local_name='Codex Ink')))
        for name in ('Codex Watch','Other UART',None):
            self.assertFalse(matches_device(dev,SimpleNamespace(local_name=name)))
        self.assertTrue(matches_device(dev,SimpleNamespace(local_name=None),'ink4-id'))
        self.assertFalse(matches_device(dev,SimpleNamespace(local_name='Codex Ink'),'other-id'))

    def test_local_defaults_keep_five_minute_poll_and_separate_state(self):
        with patch('sys.argv',['bridge']):
            args=arguments()
        self.assertEqual(args.interval,300)
        self.assertIn('CodexInkMonitor',args.state)
        self.assertFalse(args.enable_probe)
