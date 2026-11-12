#include "beamtiming.hpp"

#include <stdexcept>

#include "tasetup.h"

namespace{

//bare-nucleus masses as TAcc's beam tables use them: NIST atomic mass minus the electrons,
//neglecting binding energy
double bare_mass_u(int dataset){
  constexpr double electron_u = 0.0005485799090441;
  if(dataset == 0) return 6.01888589 - 2.0*electron_u;
  if(dataset == 1) return 9.02679019 - 3.0*electron_u;
  throw std::invalid_argument("Expected dataset 0 (6He) or 1 (9Li)");
}

double pregas_time_ns(const jointconfig::JointConfig& config, int dataset){
  const double energy_MeV = dataset == 0 ? config.PreGasEnergy_6he_MeV : config.PreGasEnergy_9li_MeV;
  return config.PreGasDistance_mm / SRIM::beam_speed_mm_per_ns(energy_MeV, bare_mass_u(dataset));
}

}

beamtiming::BeamTiming::BeamTiming(
  const jointconfig::JointConfig& config, const TA::Config& ta_config, int dataset
):
  dataset(dataset),
  pregas_ns(pregas_time_ns(config, dataset)),
  gas(TA::beam_table_filename(ta_config, dataset))
{}

double beamtiming::BeamTiming::reaction_time_ns(double vertex_mm) const{
  //TDC_corr is taken to count from the MARS scintillator (README, Open issues)
  return pregas_ns + gas.getBeamTime(vertex_mm);
}

double beamtiming::BeamTiming::beam_energy_MeV(double vertex_mm) const{
  return gas.getBeamEnergy(vertex_mm);
}
