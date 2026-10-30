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

## Neutron kinematics

After vertexing, every in-crystal hit of a pair gets a neutron time of flight
and energy. All hits in an event share that event's vertex, and none are
dropped. The neutron energy comes from the nTOF and the vertex-to-crystal flight
path alone. PMT light only places the hit along the bar and feeds PSD.

```
nTOF = mean(top, bottom TDC_corr) - t_reaction
t_reaction = t_pregas + t_gas(vertex)
E_n = m_n c^2 (gamma - 1),  beta = r / (c nTOF)
```

- `t_pregas` is the beam's flight from the MARS scintillator (the TDC start) to
  the entrance window. `TA::pregas_time_ns` computes it from `PreGasDistance_mm`
  and `PreGasEnergy_<beam>_MeV` in `info/taconfig.cfg`.
- `t_gas(vertex)` is the elapsed time in the beam's `<beam>_isobutane_vecref.txt`
  at the vertex's distance from the window. It is read with TAcc's
  `SRIM::BeamTable` file constructor. The beam (6He or 9Li) comes from the run's
  pairing list.
- The physics is in NULIB's `analysis/kinematics.hpp`. That code needs only a
  hit's flight path and a reaction time, so it doesn't care which detector found
  the vertex.
- Hits with `nTOF <= r/c` have no neutron energy (NaN). Hits outside a crystal,
  or without TDC calibration, have no nTOF.

### Reaction energies

Each hit also gets the c.m. energy of its (p,n) reaction, p(6He,n)6Li or p(9Li,n)9Be
(`reactions.hpp`, AME2020 masses). The two-body physics is in NULIB's `kinematics.hpp`.
`E_cm = sqrt(s) - m_beam - m_p` comes from two sources:

- **The vertex:** `Ebeam` is the beam table's energy at the vertex.
- **The neutron alone:** this is the beam energy that sends a neutron of `En` to `theta`.
  It needs an assumed residual state: `Ecm_n` uses the ground state, and `Ecm_n_ias` uses the
  isobaric analog of the beam (6Li 3.563 MeV, 9Be 14.390 MeV). It is NaN if no beam energy
  works.

`Ex` is the missing mass. It gives the residual's excitation from the vertex's beam energy and
the neutron, and so shows which residual state the neutrons populate.

The beam energies behind both tables and the pre-gas energies are derived in
[TAcc's README](../TAcc/README.md#beam-energies).

## Output

Each run writes `databox/joint_<TexNeut run>_<TexAT run>.root` (TexAT
config's `DataBoxDir`), replacing an existing file:

| Object | Contents |
| --- | --- |
| `En_theta` | Neutron energy vs lab polar angle from the vertex, for hits passing `neutron_filter` (2 deg x 0.1 MeV) |
| `nTOF` | nTOF of hits passing `neutron_filter` |
| `gamma_check` | `nTOF - r/c` of non-neutron hits; prompt gammas should peak at 0 |
| `vertex` | TexAT vertex along the beam axis, one entry per pair |
| `flightpath` | Vertex-to-crystal flight path `r` |
| `Ecm` | E_cm from the vertex, for hits passing `neutron_filter` with a neutron energy (as are all below) |
| `En_Ecm_n`, `En_Ecm_n_ias` | E_n vs E_cm from the neutron, residual ground state or IAS |
| `Ecm_Ecm_n`, `Ecm_Ecm_n_ias` | E_cm from the vertex vs from the neutron |
| `Ex` | Residual excitation (missing mass) |
| `hits` | Tree of every in-crystal hit: runs, `dataset` (0 = 6He, 1 = 9Li), `timestamp`, `bar`, `crystal`, `isneutron`, `passes` (the configured filter), `vertex_mm`, `t_reaction`, `tdc_top`/`tdc_bot` (TDC_corr), `r`, `theta`/`phi` (deg), `tof`, `tof_minus_rc`, `En`, `psd`, `light` (summed PMT light), `Ebeam`, `Ecm`, `Ecm_n`, `Ecm_n_ias`, `Ex` |

`gamma_check` only sees the non-neutron hits in events that passed the neutron
cull. For more statistics, run with `neutron_filter = all` in a copy of
`tnconfig.cfg`.

## Open issues

- **TDC_corr time zero (2026-10-30, preliminary).** The nTOF above treats
  `TDC_corr` as counting from the MARS scintillator. NULIB's calibration
  (`timing.hpp`, `calibrate_hit_timing`) actually defines `TDC_corr = 0` as the
  7Li beam passing the target position in run 1660. That differs from the
  scintillator time by the 7Li's 185.0 mm scintillator-to-target flight, about
  3.4 ns at `timing.hpp`'s 54.932 mm/ns. That speed is itself inconsistent with
  the 7Li MARS tune (beta 0.110, 33.0 mm/ns). The resulting systematic shift of
  every nTOF is accepted for the preliminary E_n vs angle plot, and is to be
  pinned down later. The `gamma_check` peak position measures the net offset
  directly.
