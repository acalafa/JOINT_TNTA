# JOINT_TNTA

Build with `cmake -S . -B build` and `cmake --build build`. Run
`jointanalysis` from `build/`. By default it loads `../info/tnconfig.cfg`
for TexNeut and `../info/taconfig.cfg` for TexAT. To select other files:

```sh
./jointanalysis /path/to/tnconfig.cfg /path/to/taconfig.cfg
```

The run prompt still accepts a TexNeut or TexAT run ID.

TexAT's config uses the same parser and setup utilities as TAcc; see
[TAcc's configuration documentation](../TAcc/README.md#configuration) for
all keys, defaults, and units. It controls TexAT ROOT input (`RawDataDir`),
the pad map (`ChannelMap`), the run-pairing table directory (`ExpInfoDir`),
and the five TPC parameters formerly stored in `TPCinfo.cfg`. Paths are
resolved relative to the TexAT config file as documented there. The local
`TPCinfo.cfg` has been replaced by those named values in `taconfig.cfg`.

TexNeut continues to use its own `tnconfig.cfg`, including its detector maps
and `neutron_filter` for selecting reconstructed hits. TAcc's `newtracks`
reads TexNeut's `_neutrons` timestamp logs from its `ExpInfoDir`.
Jointanalysis uses timestamp-matched raw events directly, so it does not
read those timestamp logs. The SRIM beam-table settings are kept in TAcc's
`srimconfig.cfg`, which jointanalysis does not read.

Both builds link TAcc's shared configuration implementation. Joint loading
uses the TexAT config for its ROOT input and pad map rather than borrowing
TexNeut's data or information directory. The current joint analysis still
reconstructs vertices directly from the raw TexAT tree.
