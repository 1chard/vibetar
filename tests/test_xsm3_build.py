"""Inspect the compiled USB security interface and linked XSM3 implementation."""
import unittest
from test_bluetooth_build import Elf, ROOT

class Xsm3BuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=ROOT/'build/vibetar.elf'
        if not path.exists():raise unittest.SkipTest('Build firmware first')
        cls.elf=Elf(path)

    def test_both_guitar_profiles_have_security_interface_and_original_endpoints(self):
        for name,subtype in [('xinput_configuration',7),('rock_band_configuration',6)]:
            data=self.elf.object(name)
            self.assertEqual(len(data),63)
            self.assertEqual(int.from_bytes(data[2:4],'little'),63)
            self.assertEqual(data[4],2)
            self.assertEqual(data[22],subtype)
            self.assertEqual(data[36],0x81)
            self.assertEqual(data[43],0x02)
            self.assertEqual(data[48:],bytes([9,4,1,0,0,255,253,19,4,6,65,0,1,1,3]))

    def test_xsm3_string_and_crypto_port_are_linked(self):
        for name in ['guitar_xsm3_init','guitar_xsm3_verify','guitar_xsm3_reset','get_rand_32','ExCryptSha']:
            self.assertIn(name,self.elf.symbols)
        self.assertIn(b'Xbox Security Method 3, Version 1.00, \xa9 2005 Microsoft Corporation. All rights reserved.',self.elf.data)

if __name__=='__main__':unittest.main()
