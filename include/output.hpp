#pragma once

#include <cstddef>
#include <string>

#include "beamtiming.hpp"
#include "bridges.hpp"
#include "jointconfig.hpp"

namespace output{
  //what write_joint_file wrote
  struct OutputCounts{
    std::size_t hits{};             //in-crystal hits in the tree
    std::size_t neutron_energies{}; //of those, hits passing the filter with a finite energy
  };

  //the joint config's DataBoxDir/joint_<TexNeut run>_<TexAT run>.root
  std::string joint_filename(const jointconfig::JointConfig& config, const runpairing::RunPair& pair);

  //run after bridges::calculate_kinematics. Writes a "hits" tree of every in-crystal hit
  //(passes records the filter) and histograms: neutron energy vs lab angle and nTOF for
  //hits passing the filter, TOF - r/c for non-neutron hits (gammas should sit at 0),
  //and the vertex and flight-path distributions. Replaces an existing file
  OutputCounts write_joint_file(
    const bridges::LinkedRun& run,
    const beamtiming::BeamTiming& beam,
    TN_obj::HitFilter filter,
    const std::string& filename
  );
}
