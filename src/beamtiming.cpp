#include "../include/beamtiming.hpp"

#include "tasetup.h"

beamtiming::BeamTiming::BeamTiming(const TA::Config& config, int dataset):
  dataset(dataset),
  pregas_ns(TA::pregas_time_ns(config, dataset)),
  gas(TA::beam_table_filename(config, dataset))
{}

double beamtiming::BeamTiming::reaction_time_ns(double vertex_mm) const{
  //TDC_corr is taken to count from the MARS scintillator (README, Open issues)
  return pregas_ns + gas.getBeamTime(vertex_mm);
}

double beamtiming::BeamTiming::beam_energy_MeV(double vertex_mm) const{
  return gas.getBeamEnergy(vertex_mm);
}
