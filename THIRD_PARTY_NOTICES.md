# Third-party notices

The root MIT License covers original Vibetar code. It does not replace licenses,
copyright notices, or conditions attached to the following components.

| Component | Location / source | Notices |
| --- | --- | --- |
| libxsm3 | Submodule at `third_party/libxsm3` | [LGPL-2.1](third_party/libxsm3/LICENSE.txt); preserve source notices |
| Pico SDK 2.2.0 | External build dependency | See `LICENSE.TXT` and notices in the SDK checkout |
| TinyUSB and other SDK libraries | Pico SDK submodules | See each component's license at the revision used |
| BTstack HIDS adapters | `src/hids_device_features.c` and `src/hids_client_diagnostics.c` | Source notices and [HIDS notice copy](third_party/BTSTACK_HIDS_NOTICE.txt) |

The HIDS adapters derive from BTstack as included in Pico SDK 2.2.0. Their
notices include a noncommercial-use condition and a commercial-licensing contact.
Preserve these notices when redistributing the files or binaries containing them.
The MIT License for original project code does not change these conditions.

The build generates an adapted copy of `excrypt_sha.c` under
`build/generated/libxsm3` and integrates libxsm3 through `src/xsm3_port.c` without
modifying the submodule. Keep the corresponding source, submodule revision, and
build instructions with distributed versions.

Guitar Hero, Rock Band, Xbox, and Raspberry Pi are names of their respective
owners. Vibetar is an independent project.
