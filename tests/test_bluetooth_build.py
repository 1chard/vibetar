"""Inspect both runtime USB identities inside the single universal ELF."""
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class Elf:
    def __init__(self, path):
        self.data = path.read_bytes()
        assert self.data[:6] == b'\x7fELF\x01\x01'
        offset = struct.unpack_from('<I', self.data, 32)[0]
        size, count = struct.unpack_from('<HH', self.data, 46)
        self.sections = [struct.unpack_from('<10I', self.data, offset+i*size) for i in range(count)]
        self.symbols = {}
        for section in self.sections:
            if section[1] != 2: continue
            strings = self.sections[section[6]]
            names = self.data[strings[4]:strings[4]+strings[5]]
            for pos in range(section[4], section[4]+section[5], section[9]):
                name, value, length, _, _, index = struct.unpack_from('<IIIBBH', self.data, pos)
                label = names[name:names.find(b'\0', name)].decode()
                self.symbols[label] = (value, length, index)

    def object(self, fragment):
        matches = [(k, v) for k, v in self.symbols.items() if fragment in k and v[1]]
        assert len(matches) == 1, matches
        value, length, index = matches[0][1]
        section = self.sections[index]
        offset = section[4] + value - section[3]
        return self.data[offset:offset+length]

def report_bits(descriptor):
    report_id, size, count = 0, 0, 0
    bits = {}
    pos = 0
    while pos < len(descriptor):
        prefix = descriptor[pos]; pos += 1
        length = (0, 1, 2, 4)[prefix & 3]
        value = int.from_bytes(descriptor[pos:pos+length], 'little'); pos += length
        if prefix == 0x85: report_id = value
        elif prefix == 0x75: size = value
        elif prefix == 0x95: count = value
        elif prefix & 0xfc in (0x80, 0x90, 0xb0):
            key = (report_id, {0x80:'input',0x90:'output',0xb0:'feature'}[prefix & 0xfc])
            bits[key] = bits.get(key, 0) + size*count
    return bits

class BluetoothBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=ROOT/'build/vibetar.elf'
        if not path.exists():raise unittest.SkipTest('Build universal firmware first')
        cls.w=Elf(path)

    def test_both_runtime_capabilities_exist_in_one_binary(self):
        w = report_bits(self.w.object('hid_report_descriptor_wireless'))
        pico = report_bits(self.w.object('hid_report_descriptor_base'))
        for rid, length in [(21,8), (22,8), (23,24), (24,8), (32,24)]:
            self.assertEqual(w[(rid,'feature')], length*8)
            self.assertNotIn((rid,'feature'), pico)
        self.assertEqual({k:v for k,v in w.items() if k[0]<21 or k[0] in (28,33)}, pico)
        self.assertTrue(any('hids_device_send_input_report_for_id' in s for s in self.w.symbols))

    def test_both_hid_configurations_advertise_correct_runtime_lengths(self):
        for suffix in ('base','wireless'):
            config=self.w.object('hid_configuration_'+suffix)
            descriptor=self.w.object('hid_report_descriptor_'+suffix)
            self.assertEqual(len(config),34)
            self.assertEqual(int.from_bytes(config[25:27],'little'),len(descriptor))

    def test_ble_map_reuses_usb_features_and_identifies_transport(self):
        bits = report_bits(self.w.object('report_map'))
        usb=report_bits(self.w.object('hid_report_descriptor_wireless'))
        self.assertEqual({k:v for k,v in bits.items() if k[0] not in (25,26,27,32)}, {k:v for k,v in usb.items() if k[0] not in (28,32)})
        self.assertEqual(bits[(26,'input')],128)
        self.assertEqual(bits[(27,'output')],64)
        self.assertEqual(bits[(25,'feature')],64)
        self.assertEqual(report_bits(self.w.object('hid_report_descriptor_wireless'))[(1,'input')],88)

    def test_ble_feature_permissions_follow_hids_read_write_requirement(self):
        data=self.w.object('profile_data');pos=1;report_flags=None;found={}
        while pos+2<=len(data):
            length=int.from_bytes(data[pos:pos+2],'little')
            if not length:break
            flags,_,uuid=struct.unpack_from('<HHH',data,pos+2)
            value=data[pos+8:pos+length]
            if uuid==0x2a4d:report_flags=flags
            if uuid==0x2908 and value[1]==3:found[value[0]]=report_flags
            pos+=length
        self.assertEqual(set(found),set(range(2,26))|{33})
        for rid,flags in found.items():
            # SDK 2.2.0 flags: dynamic, encrypted read/write, 16-byte key.
            self.assertEqual(flags,0xf50b)

    def test_linker_keeps_program_out_of_bonds_and_settings(self):
        self.assertLessEqual(self.w.symbols['__flash_binary_end'][0],0x101fc000)

    def test_gatt_contains_input_report_reference_and_no_keyboard_reports(self):
        data = self.w.object('profile_data')
        self.assertEqual(data[0],1)
        attributes=[];pos=1
        while pos+2<=len(data):
            length=int.from_bytes(data[pos:pos+2],'little')
            if not length:break
            uuid=int.from_bytes(data[pos+6:pos+8],'little')
            attributes.append((uuid,data[pos+8:pos+length]))
            pos+=length
        self.assertIn((0x2800,b'\x12\x18'),attributes)
        self.assertIn((0x2908,b'\x01\x01'),attributes)
        self.assertEqual(sum(uuid==0x2a4d for uuid,_ in attributes),28)
        for rid in range(2,26):self.assertIn((0x2908,bytes([rid,3])),attributes)
        self.assertFalse(any(uuid in (0x2a22,0x2a32,0x2a33) for uuid,_ in attributes))

if __name__=='__main__':unittest.main()
