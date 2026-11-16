#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "beamtiming.hpp"
#include "bridges.hpp"
#include "jointconfig.hpp"
#include "output.hpp"
#include "analysis/analysis.hpp"
#include "detector/TexNeut.hpp"
#include "progutils/config.hpp"
#include "progutils/dataref.hpp"
#include "progutils/setup.hpp"
#include "analysis/Hit.hpp"
#include "configs.h"
#include "tasetup.h"

int main(int argc, char** argv){
  try{
    if(argc > 2) throw std::invalid_argument("Usage: jointanalysis [jointconfig.cfg]");
    //the joint config names each detector's own config and holds the joint-only settings;
    //the run pairing lists sit beside it
    const jointconfig::JointConfig joint = jointconfig::load(argc > 1 ? argv[1] : "../info/jointconfig.cfg");
    //load NULIB's tnconfig
    TexNeut tn(progutils::TexNeutBarCount);
    config_obj config{};
    progutils::load_experiment(config, tn, joint.NULIBConfig);
    const TN_obj::HitFilter filter = TN_obj::hit_filter_from_string(joint.neutron_filter);

    //load TAcc's taconfig
    TA::Config ta_config;
    ChannelMap cmap;
    TPCinfo tpc;
    TA::load_experiment(ta_config, cmap, tpc, joint.TAccConfig);

    int requestedrun{};
    std::cout<<"Analyse which run (TexNeut or TexAT ID)? ";
    if(!(std::cin>>requestedrun)) throw std::runtime_error("Expected an integer run ID.");

    const runpairing::RunPair pair = bridges::link_run(requestedrun, joint.InfoDir);
    std::cout<<"TexNeut run "<<pair.texneut_run<<" is paired with TexAT run "
             <<pair.texat_run<<"."<<std::endl;

    //the pair lists give the beam; its TAcc settings (beam-time window) replace the shared ones,
    //and a beam_time_window_runs range covering the TexAT run replaces both
    const int dataset = bridges::beam_dataset(pair, joint.InfoDir);
    tpc = ta_config.for_run(dataset, pair.texat_run).tpc_info();
    std::cout<<TA::beam_label(dataset)<<" run: ";
    if(tpc.beam_time_cut()){
      std::cout<<"beam window "<<tpc.beam_time_min_bucket<<"-"<<tpc.beam_time_max_bucket<<" buckets";
    }else{
      std::cout<<"no beam-time window";
    }
    if(const TA::RunBeamWindow* window = ta_config.run_window(pair.texat_run)){
      std::cout<<" (beam_time_window_runs "<<window->first_run<<"-"<<window->last_run<<")";
    }
    std::cout<<".";
    std::cout<<std::endl;

    bridges::LinkedRun run{};
    bridges::load_linked_run(run, pair, config, tn, ta_config, dataset);
    std::cout<<run.texneut_events.size()<<" TexNeut events and "
             <<run.texat_tree->GetEntries()<<" TexAT entries loaded."<<std::endl;

    //only events seen by both detectors, with TexAT pad hits, are useful from here on
    const bridges::CullCounts culled = bridges::cull_unpaired(run);
    std::cout<<"Culled "<<culled.texneut_unpaired<<" TexNeut and "
             <<culled.texat_unpaired<<" TexAT events without a partner, and "
             <<culled.empty_pairs<<" pairs with no TexAT pad hits."<<std::endl;

    //neutron classification needs the analysed TexNeut events
    analysis::analyse_events(run.texneut_events, tn);
    const std::size_t filtered = bridges::cull_by_hit_filter(run, filter);
    std::cout<<"Culled "<<filtered<<" pairs with no in-crystal TexNeut hit passing the '"
             <<joint.neutron_filter<<"' filter."<<std::endl;

    //cheapest cuts first: the clouds read each remaining TexAT entry from disk
    //applying each pencil-beam vertex gives every in-crystal hit its flight path (position.r)
    const bridges::VertexCounts vertexing = bridges::find_vertices(run, cmap, tpc);
    std::cout<<"Culled "<<vertexing.bad_clouds<<" pairs without a TexAT goodcloud and "
             <<vertexing.no_vertex<<" without a vertex estimate."<<std::endl;
    std::cout<<run.texneut_events.size()<<" paired events kept, each with a vertex."<<std::endl;

    //nTOF = TDC_corr - (MARS scintillator to window + window to vertex) for the run's beam
    const beamtiming::BeamTiming beam(joint, ta_config, dataset);
    std::cout<<(dataset == 0 ? "6He" : "9Li")<<" beam: "<<beam.pregas_ns
             <<" ns from the MARS scintillator to the window."<<std::endl;
    bridges::calculate_kinematics(run, beam);

    const std::string outname = output::joint_filename(joint, pair);
    const output::OutputCounts written = output::write_joint_file(run, beam, filter, outname);
    std::cout<<"Wrote "<<written.hits<<" in-crystal hits ("<<written.neutron_energies
             <<" passing '"<<joint.neutron_filter<<"' with a neutron energy) to "
             <<outname<<"."<<std::endl;

  }catch(const std::exception& e){
    std::cerr<<"Error: "<<e.what()<<std::endl;
    return 1;
  }
  return 0;
}
