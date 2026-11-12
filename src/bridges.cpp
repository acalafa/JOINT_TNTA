#include "../include/bridges.hpp"
#include "../../NULIB/include/analysis/analysis.hpp"
#include "../../NULIB/include/analysis/kinematics.hpp"
#include "../../NULIB/include/conversion/evt/interfacing.hpp"
#include "../../NULIB/include/progutils/dataref.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "TBranch.h"
#include "TDirectory.h"

//TAcc headers
#include "analysis.h"
#include "root_mess.h"
#include "trackrecon.h"

namespace{

//erases the pairs keep rejects, keeping texneut_events, texat_entries and (once found)
//vertices aligned and in order; keep sees each pair once, in order
std::size_t keep_pairs(
  bridges::LinkedRun& run,
  const std::function<bool(std::size_t)>& keep,
  const std::string& cut_name
){
  std::vector<TN_obj::Event>& events = run.texneut_events;
  std::vector<Long64_t>& entries = run.texat_entries;
  std::vector<std::array<double, 3>>& vertices = run.vertices;
  const bool has_vertices = !vertices.empty();
  std::size_t kept = 0;
  for(std::size_t i = 0; i < events.size(); ++i){
    if(!keep(i)) continue;
    if(kept != i){
      events[kept] = std::move(events[i]);
      entries[kept] = entries[i];
      if(has_vertices) vertices[kept] = vertices[i];
    }
    ++kept;
  }
  if(kept == 0) throw std::runtime_error("No events left after the " + cut_name + " cut");
  const std::size_t culled = events.size() - kept;
  events.erase(events.begin() + kept, events.end());
  entries.erase(entries.begin() + kept, entries.end());
  if(has_vertices) vertices.erase(vertices.begin() + kept, vertices.end());
  return culled;
}

}

runpairing::RunPair bridges::link_run(int requested_run, const std::string& info_directory){
  runpairing::RunLists run_lists;
  if(!runpairing::load_run_lists(run_lists, info_directory)){
    throw std::runtime_error("Could not load 6hepn.txt and 9lipn.txt run lists from " +
                             info_directory);
  }
  return run_lists.resolve(requested_run);
}

int bridges::beam_dataset(const runpairing::RunPair& pair, const std::string& info_directory){
  runpairing::RunLists run_lists;
  if(!runpairing::load_run_lists(run_lists, info_directory)){
    throw std::runtime_error("Could not load 6hepn.txt and 9lipn.txt run lists from " +
                             info_directory);
  }
  const runpairing::RunSet set = run_lists.membership(pair.texat_run, runpairing::RunSource::TexAT);
  if(set == runpairing::RunSet::SixHe) return 0;
  if(set == runpairing::RunSet::NineLi) return 1;
  throw std::runtime_error("TexAT run " + std::to_string(pair.texat_run) +
                           " is in neither the 6He nor the 9Li run list");
}

void bridges::load_linked_run(
  LinkedRun& output,
  const runpairing::RunPair& pair,
  const config_obj& config,
  const TexNeut& tn,
  const TA::Config& ta_config
){
  const std::string evtfilename = fnames::InputEvtFile(config, pair.texneut_run);
  const std::string rootfilename = get_ROOT_filename(ta_config.RawDataDir.c_str(), pair.texat_run).Data();

  //evt processing is slow and only prints an error for a missing file, so check both first
  if(!std::ifstream(evtfilename)){
    throw std::runtime_error("TexNeut evt file not found: " + evtfilename);
  }
  if(!std::ifstream(rootfilename)){
    throw std::runtime_error("TexAT ROOT file not found: " + rootfilename);
  }

  LinkedRun loaded{};
  loaded.pair = pair;
  {
    //keep later histograms out of the read-only TexAT file
    TDirectory::TContext keep_directory;
    loaded.texat_file.reset(TFile::Open(rootfilename.c_str(), "READ"));
  }
  if(!loaded.texat_file || loaded.texat_file->IsZombie()){
    throw std::runtime_error("Could not open TexAT ROOT file " + rootfilename);
  }
  loaded.texat_file->GetObject("Catalytic", loaded.texat_tree);
  if(!loaded.texat_tree){
    throw std::runtime_error("No Catalytic tree in " + rootfilename);
  }

  evt::fullprocess(evtfilename, tn, loaded.texneut_events);
  if(loaded.texneut_events.empty()){
    throw std::runtime_error("No TexNeut events built from " + evtfilename);
  }

  output = std::move(loaded);
  return;
}

bridges::CullCounts bridges::cull_unpaired(LinkedRun& run){
  //read through two small branches alone: the waveform branch is most of the file
  TTree& tree = *run.texat_tree;
  TBranch* timestamp_branch = tree.GetBranch("texneut_timestamp");
  TBranch* cobo_branch = tree.GetBranch("cobo"); //one element per pad hit, as in TAcc
  if(!timestamp_branch || !cobo_branch){
    throw std::runtime_error("TexAT tree needs texneut_timestamp and cobo branches");
  }
  Long64_t texat_timestamp = 0;
  std::vector<int>* cobo = nullptr;
  tree.SetBranchAddress("texneut_timestamp", &texat_timestamp);
  tree.SetBranchAddress("cobo", &cobo);

  struct TexATEntry{
    Long64_t entry{};
    bool has_pads = false;
    bool used = false;
  };
  std::unordered_map<std::uint64_t, TexATEntry> texat_by_timestamp{};
  const Long64_t texat_count = tree.GetEntries();
  texat_by_timestamp.reserve(static_cast<std::size_t>(texat_count));
  bool repeated = false;
  std::uint64_t timestamp = 0;
  for(Long64_t entry = 0; entry < texat_count && !repeated; ++entry){
    timestamp_branch->GetEntry(entry);
    cobo_branch->GetEntry(entry);
    timestamp = static_cast<std::uint64_t>(texat_timestamp);
    TexATEntry texat_entry{};
    texat_entry.entry = entry;
    texat_entry.has_pads = !cobo->empty();
    repeated = !texat_by_timestamp.emplace(timestamp, texat_entry).second;
  }
  //the locals go out of scope, so leave no addresses behind
  tree.ResetBranchAddress(timestamp_branch);
  tree.ResetBranchAddress(cobo_branch);
  delete cobo;
  if(repeated){
    throw std::runtime_error("TexAT timestamp " + std::to_string(timestamp) +
                             " repeats: pairing would be ambiguous");
  }

  //compact in place, keeping TexNeut order
  CullCounts counts{};
  std::vector<TN_obj::Event>& events = run.texneut_events;
  std::vector<Long64_t> texat_entries{};
  std::size_t kept = 0;
  for(std::size_t i = 0; i < events.size(); ++i){
    const auto partner = texat_by_timestamp.find(events[i].timestamp);
    if(partner == texat_by_timestamp.end()){
      ++counts.texneut_unpaired;
      continue;
    }
    if(partner->second.used){
      throw std::runtime_error("TexNeut timestamp " + std::to_string(events[i].timestamp) +
                               " repeats: pairing would be ambiguous");
    }
    partner->second.used = true;
    if(!partner->second.has_pads){
      ++counts.empty_pairs;
      continue;
    }
    texat_entries.push_back(partner->second.entry);
    if(kept != i) events[kept] = std::move(events[i]);
    ++kept;
  }
  if(kept == 0){
    throw std::runtime_error("No timestamp-matched events with TexAT pad hits");
  }
  events.erase(events.begin() + kept, events.end());
  counts.texat_unpaired =
    static_cast<std::size_t>(texat_count) - kept - counts.empty_pairs;

  run.texat_entries.swap(texat_entries);
  run.vertices.clear(); //pairing starts over
  return counts;
}

std::size_t bridges::cull_by_hit_filter(LinkedRun& run, TN_obj::HitFilter filter){
  return keep_pairs(run, [&](std::size_t i){
    for(const TN_obj::Hit& hit : run.texneut_events[i].hits){
      if(hit.incrystal && TN_obj::passes_filter(hit, filter)) return true;
    }
    return false;
  }, "TexNeut in-crystal hit filter");
}

bridges::VertexCounts bridges::find_vertices(
  LinkedRun& run,
  const ChannelMap& cmap,
  const TPCinfo& tpc
){
  //TAcc's cloud building takes non-const configs
  ChannelMap cloud_cmap = cmap;
  TTree& tree = *run.texat_tree;
  RAW_InputBranches branches;
  branches.link_root_branches(&tree);
  tree.SetBranchStatus("wave", 0); //nearly all of the file, and clouds never read it

  VertexCounts counts{};
  std::vector<std::array<double, 3>> vertices{};
  run.vertices.clear();
  try{
    counts.bad_clouds = keep_pairs(run, [&](std::size_t i){
      TN_obj::Event& event = run.texneut_events[i];
      //the event's t0 is taconfig's shifted by TexNeut's earliest TDC; it only moves drift
      //height, and cloud membership and the row vertex don't use it
      const float event_t0_ns = tpc.t0_ns - static_cast<float>(analysis::earliest_tdc_ns(event));
      tree.GetEntry(run.texat_entries[i]);
      TPCinfo cloud_tpc = tpc;
      const TA_obj::PointCloud cloud(branches, cloud_cmap, cloud_tpc, event_t0_ns);
      if(!cloud.goodcloud) return false;

      const double vertex_row = Analysis::estimate_vertex_z(cloud);
      if(!std::isfinite(vertex_row)){
        ++counts.no_vertex;
        return false;
      }
      //pencil beam: TexAT's beam axis (its z) is NULIB's y; x and the vertical stay on the
      //nominal beam axis, as for NULIB's nominal target
      const std::array<double, 3> vertex{{
        0.0, window_z_mm(vertex_row), TexATGeometry::nominal_vertex_y_mm
      }};
      apply_vertex(event, vertex);
      vertices.push_back(vertex);
      return true;
    }, "TexAT vertex");
  }catch(...){
    tree.ResetBranchAddresses();
    tree.SetBranchStatus("wave", 1);
    throw;
  }
  //branches deletes its vectors on return, so leave no addresses behind
  tree.ResetBranchAddresses();
  tree.SetBranchStatus("wave", 1);

  counts.bad_clouds -= counts.no_vertex; //keep_pairs counted every dropped pair
  run.vertices.swap(vertices);
  return counts;
}

void bridges::apply_vertex(TN_obj::Event& event, const std::array<double, 3>& vertex){
  //vertex and TexNeut map must share the TexAT entrance window origin, axes and units
  for(TN_obj::Hit& hit : event.hits){
    if(hit.incrystal) analysis::calculate_hit_flightpath(hit, vertex);
  }
  return;
}

void bridges::calculate_kinematics(LinkedRun& run, const beamtiming::BeamTiming& beam){
  if(run.vertices.size() != run.texneut_events.size()){
    throw std::logic_error("calculate_kinematics needs a vertex for every pair: run find_vertices first");
  }
  for(std::size_t i = 0; i < run.texneut_events.size(); ++i){
    //vertex y is the beam axis, measured from the entrance window like the beam table
    kinematics::calculate_event_kinematics(run.texneut_events[i],
                                           beam.reaction_time_ns(run.vertices[i][1]));
  }
  return;
}
