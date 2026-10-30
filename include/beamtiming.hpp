#pragma once

#include "../../TAcc/include/srims.h"
#include "../../TAcc/include/taconfig.h"

namespace beamtiming{
  //the beam's flight from the MARS scintillator (the TDC start) to a TexAT reaction vertex
  struct BeamTiming{
    //reads ExpInfoDir's beam table for the dataset (0 = 6He, 1 = 9Li) and the config's
    //pre-gas flight; throws if either is missing
    BeamTiming(const TA::Config& config, int dataset);

    //reaction time [ns] on the TDC_corr axis for a vertex vertex_mm downstream of the
    //entrance window (NULIB's map y); throws outside the beam table
    double reaction_time_ns(double vertex_mm) const;
    //beam kinetic energy [MeV] at the vertex; throws outside the beam table
    double beam_energy_MeV(double vertex_mm) const;

    int dataset;
    double pregas_ns;    //MARS scintillator to entrance window
    SRIM::BeamTable gas; //energy and elapsed time from the entrance window
  };
}
