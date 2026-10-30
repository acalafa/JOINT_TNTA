#pragma once

#include "../../NULIB/include/analysis/kinematics.hpp"

namespace reactions{
  //p(6He,n)6Li for dataset 0, p(9Li,n)9Be for dataset 1; throws otherwise
  kinematics::Reaction pn_reaction(int dataset);
  //excitation [MeV] of the residual's isobaric analog of the beam's ground state:
  //6Li 3.563 (0+, T=1) or 9Be 14.390 (3/2-, T=3/2)
  double analog_state_MeV(int dataset);
}
