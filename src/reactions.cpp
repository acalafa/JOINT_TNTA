#include "reactions.hpp"

#include <stdexcept>
#include <string>

namespace{
  constexpr double MeV_per_u = 931.49410242;    //CODATA 2018
  constexpr double electron_MeV = 0.51099895;   //CODATA 2018
  constexpr double proton_MeV = 938.27208816;   //CODATA 2018

  //AME2020 atomic mass less the electrons; electron binding is neglected
  double nuclear_MeV(double atomic_mass_u, int Z){
    return atomic_mass_u*MeV_per_u - Z*electron_MeV;
  }

  void check_dataset(int dataset){
    if(dataset != 0 && dataset != 1){
      throw std::invalid_argument("No (p,n) reaction for dataset " + std::to_string(dataset));
    }
  }
}

kinematics::Reaction reactions::pn_reaction(int dataset){
  check_dataset(dataset);
  if(dataset == 0) return {nuclear_MeV(6.018885889, 2), proton_MeV, nuclear_MeV(6.0151228874, 3)};
  return {nuclear_MeV(9.026790191, 3), proton_MeV, nuclear_MeV(9.012183062, 4)};
}

double reactions::analog_state_MeV(int dataset){
  check_dataset(dataset);
  return dataset == 0 ? 3.56288 : 14.3903; //TUNL evaluations
}
