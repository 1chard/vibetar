"""Check the two compiled roles share USB personalities and isolate BLE roles."""
import unittest
from test_bluetooth_build import Elf, ROOT, report_bits

class ReceiverBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tx=Elf(ROOT/'build/vibetar.elf')
        cls.rx=Elf(ROOT/'build/vibetar-receiver.elf')

    def test_shared_usb_and_authentication(self):
        for name in ('xinput_configuration','rock_band_configuration','hid_report_descriptor_base'):
            self.assertEqual(self.tx.object(name),self.rx.object(name))
        tx_bits=report_bits(self.tx.object('hid_report_descriptor_wireless'))
        rx_bits=report_bits(self.rx.object('hid_report_descriptor_wireless'))
        self.assertEqual({k:v for k,v in rx_bits.items() if k[0] not in (29,30,31)},tx_bits)
        self.assertEqual(rx_bits[(29,'feature')],256)
        self.assertEqual(rx_bits[(30,'feature')],128)
        for name in ('guitar_xsm3_init','guitar_xsm3_verify','guitar_xsm3_reset','ExCryptSha'):
            self.assertIn(name,self.rx.symbols)
        self.assertEqual(report_bits(self.rx.object('hid_report_descriptor_base'))[(28,'feature')],64)

    def test_central_only_receiver_and_reserved_flash(self):
        self.assertIn('hids_client_connect',self.rx.symbols)
        self.assertNotIn('hids_client_connect',self.tx.symbols)
        self.assertNotIn('hids_device_send_input_report_for_id',self.rx.symbols)
        self.assertLessEqual(self.rx.symbols['__flash_binary_end'][0],0x101fc000)
        self.assertIn(b'Vibetar Receiver HID',self.rx.data)

    def test_required_reports_are_within_client_capacity(self):
        data=self.tx.object('profile_data');pos=1;refs=[]
        while pos+2<=len(data):
            size=int.from_bytes(data[pos:pos+2],'little')
            if not size:break
            if int.from_bytes(data[pos+6:pos+8],'little')==0x2908:refs.append(data[pos+8:pos+size])
            pos+=size
        self.assertEqual(refs[:3],[bytes([1,1]),bytes([26,1]),bytes([27,2])])

    def test_both_uf2_targets_contain_rp2040_flash_blocks(self):
        for name in ('vibetar','vibetar-receiver'):
            data=(ROOT/'build'/f'{name}.uf2').read_bytes()
            self.assertEqual(len(data)%512,0)
            for offset in range(0,len(data),512):
                block=data[offset:offset+512]
                self.assertEqual(block[:8],bytes.fromhex('5546320a57515d9e'))
                address=int.from_bytes(block[12:16],'little')
                self.assertGreaterEqual(address,0x10000000)
                self.assertLess(address,0x101fc000)
                self.assertEqual(block[28:32],bytes.fromhex('56ff8be4'))

if __name__=='__main__':unittest.main()
