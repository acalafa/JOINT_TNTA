#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"

#include "beamtiming.hpp"
#include "../../NULIB/include/analysis/Event.hpp"
#include "../../NULIB/include/analysis/Hit.hpp"
#include "../../NULIB/include/detector/TexNeut.hpp"
#include "../../NULIB/include/progutils/config.hpp"
#include "../../NULIB/include/progutils/runlists.hpp"
#include "../../TAcc/include/configs.h"
#include "../../TAcc/include/taconfig.h"

namespace bridges{
  //both detectors' data for one paired run
  struct LinkedRun{
    progutils::RunPair pair{};
    std::vector<TN_obj::Event> texneut_events{};
    std::unique_ptr<TFile> texat_file{};
    TTree* texat_tree = nullptr; //owned by texat_file; entries are read from disk on demand
    //filled by cull_unpaired: texat_entries[i] is the tree entry of texneut_events[i]
    std::vector<Long64_t> texat_entries{};
    //filled by find_vertices: vertices[i] is texneut_events[i]'s reaction vertex in NULIB's
    //map frame (mm from the TexAT entrance window; x horizontal, y beam axis, z vertical)
    std::vector<std::array<double, 3>> vertices{};
  };

  //why cull_unpaired dropped events; an empty pair costs one event from each detector
  struct CullCounts{
    std::size_t texneut_unpaired{}; //TexNeut events with no TexAT entry at their timestamp
    std::size_t texat_unpaired{};   //TexAT entries with no TexNeut event at their timestamp
    std::size_t empty_pairs{};      //paired, but the TexAT entry has no pad hits
  };

  //why find_vertices dropped pairs
  struct VertexCounts{
    std::size_t bad_clouds{}; //TexAT cloud is not goodcloud
    std::size_t no_vertex{};  //goodcloud, but no usable row charge for a vertex estimate
  };

  //accepts a TexNeut or TexAT run ID via TAcc's RunPairing (6hepn.txt and 9lipn.txt in
  //info_directory), asking which detector is meant when the ID is in both columns
  progutils::RunPair link_run(int requested_run, const std::string& info_directory);

  //the pair's beam from the same run lists: 0 = 6He, 1 = 9Li, as TAcc numbers its tables
  int beam_dataset(const progutils::RunPair& pair, const std::string& info_directory);

  //checks both files exist before reading either, then processes the evt file and opens
  //the TexAT tree; throws if either is missing or empty
  void load_linked_run(
    LinkedRun& output,
    const progutils::RunPair& pair,
    const config_obj& config,
    const TexNeut& tn,
    const TA::Config& ta_config
  );

  //The culls below keep texneut_events, texat_entries and vertices aligned and in order, and
  //throw if no events remain. The read-only tree is left whole: read TexAT data through
  //texat_entries.

  //pairs events by TexNeut timestamp (TexAT's texneut_timestamp branch) and keeps only
  //pairs whose TexAT entry has pad hits: other TexNeut events are erased and texat_entries
  //gets the kept partners. Throws if a timestamp repeats within either detector
  CullCounts cull_unpaired(LinkedRun& run);

  //run after analysis::analyse_events: drops pairs whose TexNeut event has no in-crystal
  //hit passing the filter (config's neutron_filter), since only in-crystal hits get a
  //flight path; hits within kept events are left untouched
  std::size_t cull_by_hit_filter(LinkedRun& run, TN_obj::HitFilter filter);

  //run after cull_by_hit_filter. Per pair: builds TAcc's point cloud (goodhit per hit) with
  //t0 shifted by the TexNeut event's earliest TDC (analysis::earliest_tdc_ns), drops the
  //pair unless the cloud is goodcloud and gives a vertex, then stores a pencil-beam vertex
  //on NULIB's beam axis and applies it, giving each hit's flight path
  VertexCounts find_vertices(LinkedRun& run, const ChannelMap& cmap, const TPCinfo& tpc);

  //run after analysis::analyse_event: replaces the nominal-target flight paths with the
  //event's TexAT reaction vertex, which must already be in NULIB's map frame
  void apply_vertex(TN_obj::Event& event, const std::array<double, 3>& vertex);

  //run after find_vertices: gives every hit its nTOF and neutron energy, all hits of a
  //pair sharing the beam's flight time to that pair's vertex
  void calculate_kinematics(LinkedRun& run, const beamtiming::BeamTiming& beam);
}
