# JOINT_TNTA

Build with `cmake -S . -B build` and `cmake --build build`. Run
`jointanalysis` from `build/`. By default it loads `../info/jointconfig.cfg`; to select
another joint config:

```sh
./jointanalysis /path/to/jointconfig.cfg
```

The run prompt accepts a TexNeut or TexAT run ID.

## Inputs

`info/jointconfig.cfg` is the only config jointanalysis reads directly. It points to each
detector's own config, which is loaded unchanged, and holds the settings that belong to
the joint analysis alone. Relative paths are resolved from the joint config's folder.

| Key | Meaning |
| --- | --- |
| `NULIBConfig` | Required: NULIB's `tnconfig.cfg` (`../../NULIB/info/tnconfig.cfg`): TexNeut raw data, channel, crystal-position (TexAT origin) and PMT offset maps; finger and PSD cuts |
| `TAccConfig` | Required: TAcc's `taconfig.cfg` (`../../TAcc/info/taconfig.cfg`): TexAT raw data, pad map, TPC parameters and cloud cuts; the `<beam>_isobutane_vecref.txt` beam tables in its `ExpInfoDir` |
| `DataBoxDir` | Joint output folder (default `../databox/`) |
| `neutron_filter` | Pairs are kept only if their TexNeut event has an in-crystal hit of this kind: `all`, `neutrons` (default) or `nonneutrons`. NULIB's own `neutron_filter` is not used |
| `PreGasDistance_mm` | Required: beam flight from the MARS scintillator (TDC start) to the entrance window |
| `PreGasEnergy_6he_MeV`, `PreGasEnergy_9li_MeV` | Required: total beam energy after the scintillator, for that flight |

Both detector configs resolve their own relative paths from their own folders, so each
codebase's maps, cuts and tables are read in place and any change there (for example
TAcc's `min_side_hits` or beam-time cut) applies to the joint analysis too.

Run pairing belongs to the joint analysis alone: neither NULIB nor TAcc knows the other
detector's run IDs. The pair lists `6hepn.txt` and `9lipn.txt` are read from the joint
config's folder (`info/`). Each row is `TexNeut_run TexAT_run`; blank lines and `#` comments
are allowed, and IDs must be positive and unique within each detector's column across both
lists. NULIB keeps its own TexNeut-only set lists. `runpairing.hpp` resolves a run ID from
either detector, asking `ta`/`tn` when an ID occurs in both columns, and gives the beam
(6He or 9Li) from the list it is in. Its checks run with `ctest --test-dir build`.

See [NULIB's README](../NULIB/README.MD) and
[TAcc's configuration documentation](../TAcc/README.md#configuration) for the keys of
each detector config.

jointanalysis timestamp-matches raw TexNeut and TexAT events directly; it reads no
timestamp/TDC logs. The SRIM settings that
generate the beam tables are in TAcc's `srimconfig.cfg`, which jointanalysis doesn't read.
Regenerate a table in TAcc and jointanalysis picks it up without copying anything.

The joint analysis reconstructs vertices directly from the raw TexAT tree.

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
  the entrance window. `beamtiming.cpp` computes it from `PreGasDistance_mm`
  and `PreGasEnergy_<beam>_MeV` in `info/jointconfig.cfg`, with the same bare-nucleus
  masses as TAcc's beam tables.
- `t_gas(vertex)` is the elapsed time in the beam's `<beam>_isobutane_vecref.txt`
  at the vertex's distance from the window. It is read from TAcc's `info/` with the
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

`theta_cm` is the neutron's c.m. angle: the lab neutron (`En`, `theta`) is boosted along the
beam axis using the vertex's `Ebeam`, so no residual state is assumed. It is measured from the
beam direction, like `theta`. The normal-kinematics A(p,n) angle, measured from the proton's
direction, is `180 - theta_cm`.

The beam energies behind both tables and the pre-gas energies are derived in
[TAcc's README](../TAcc/README.md#beam-energies).

## Output

Each run writes `databox/joint_<TexNeut run>_<TexAT run>.root` (the joint
config's `DataBoxDir`), replacing an existing file:

| Object | Contents |
| --- | --- |
| `En_theta` | Neutron energy vs lab polar angle from the vertex, for hits passing `neutron_filter` (0.25 deg x 0.05 MeV) |
| `nTOF` | nTOF of hits passing `neutron_filter` |
| `gamma_check` | `nTOF - r/c` of non-neutron hits; prompt gammas should peak at 0 |
| `vertex` | TexAT vertex along the beam axis, one entry per pair |
| `flightpath` | Vertex-to-crystal flight path `r` |
| `Ecm` | E_cm from the vertex, for hits passing `neutron_filter` with a neutron energy (as are all below) |
| `En_Ecm_n`, `En_Ecm_n_ias` | E_n vs E_cm from the neutron, residual ground state or IAS |
| `Ecm_Ecm_n`, `Ecm_Ecm_n_ias` | E_cm from the vertex vs from the neutron |
| `Ex` | Residual excitation (missing mass) |
| `theta_cm`, `En_theta_cm` | Neutron c.m. angle (1 deg bins), and E_n against it |
| `theta_cm_theta` | Neutron c.m. angle vs lab angle |
| `hits` | Tree of every in-crystal hit: runs, `dataset` (0 = 6He, 1 = 9Li), `timestamp`, `bar`, `crystal`, `isneutron`, `passes` (the configured filter), `vertex_mm`, `t_reaction`, `tdc_top`/`tdc_bot` (TDC_corr), `r`, `theta`/`phi` (deg), `tof`, `tof_minus_rc`, `En`, `psd`, `light` (summed PMT light), `Ebeam`, `Ecm`, `Ecm_n`, `Ecm_n_ias`, `Ex`, `theta_cm` (deg) |

`gamma_check` only sees the non-neutron hits in events that passed the neutron
cull. For more statistics, run with `neutron_filter = all` in a copy of
`jointconfig.cfg`.

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
