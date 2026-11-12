#pragma once

#include "jointconfig.hpp"
#include "../../TAcc/include/srims.h"
#include "../../TAcc/include/taconfig.h"

namespace beamtiming{
  //the beam's flight from the MARS scintillator (the TDC start) to a TexAT reaction vertex
  struct BeamTiming{
    //the pre-gas flight comes from the joint config, the in-gas flight from the beam table
    //for the dataset (0 = 6He, 1 = 9Li) in TAcc's ExpInfoDir; throws if the table is missing
    BeamTiming(const jointconfig::JointConfig& config, const TA::Config& ta_config, int dataset);

    //reaction time [ns] on the TDC_corr axis for a vertex vertex_mm downstream of the
    //entrance window (the shared frame's y); throws outside the beam table
    double reaction_time_ns(double vertex_mm) const;
    //beam kinetic energy [MeV] at the vertex; throws outside the beam table
    double beam_energy_MeV(double vertex_mm) const;

    int dataset;
    double pregas_ns;    //MARS scintillator to entrance window
    SRIM::BeamTable gas; //energy and elapsed time from the entrance window
  };
}
