#pragma once

#include <string>

namespace jointconfig{
  //jointanalysis's one config file. Each detector keeps its own config, referenced here and
  //loaded unchanged; only joint decisions live in this file.
  //Format: key = value, one per line; blank lines and lines starting with # are skipped.
  //Relative paths are resolved from this file's folder.
  struct JointConfig{
    std::string NULIBConfig{}; //TexNeut: NULIB's tnconfig.cfg
    std::string TAccConfig{};  //TexAT: TAcc's taconfig.cfg
    std::string DataBoxDir{};  //joint output folder; default ../databox/
    std::string InfoDir{};     //this file's folder: holds the 6hepn.txt and 9lipn.txt pair lists

    //hits that must be present for a pair to be kept: all, neutrons or nonneutrons.
    //NULIB's own neutron_filter (for its plots and logs) is not used
    std::string neutron_filter = "neutrons";

    //beam flight from the MARS scintillator (TDC start) to the TexAT entrance window
    double PreGasDistance_mm{};
    double PreGasEnergy_6he_MeV{}; //total MeV after the scintillator
    double PreGasEnergy_9li_MeV{};
  };

  //throws on a missing file, an unknown or repeated key, or a missing or invalid value
  JointConfig load(const std::string& filename);
}
