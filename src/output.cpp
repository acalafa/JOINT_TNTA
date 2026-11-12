#include "../include/output.hpp"
#include "../include/reactions.hpp"
#include "../../NULIB/include/analysis/timing.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>

#include "TDirectory.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TMath.h"

std::string output::joint_filename(const jointconfig::JointConfig& config, const progutils::RunPair& pair){
  return config.DataBoxDir + "joint_" + std::to_string(pair.texneut_run) + "_" +
         std::to_string(pair.texat_run) + ".root";
}

output::OutputCounts output::write_joint_file(
  const bridges::LinkedRun& run,
  const beamtiming::BeamTiming& beam,
  TN_obj::HitFilter filter,
  const std::string& filename
){
  std::unique_ptr<TFile> file(TFile::Open(filename.c_str(), "RECREATE"));
  if(!file || file->IsZombie()){
    throw std::runtime_error("Could not create joint output file " + filename);
  }
  OutputCounts counts{};
  {
    //everything below is owned by the file, which deletes it on Close
    TDirectory::TContext into_file(file.get());
    TH2D* en_theta = new TH2D("En_theta",
      "Neutron energy vs lab angle;#theta_{lab} [deg];E_{n} [MeV]", 160, 0.0, 40.0, 400, 0.0, 20.0);
    TH2D* en_theta_cm = new TH2D("En_theta_cm",
      "Neutron energy vs c.m. angle (vertex beam energy);#theta_{c.m.} [deg];E_{n} [MeV]",
      180, 0.0, 180.0, 400, 0.0, 20.0);
    TH2D* theta_cm_lab = new TH2D("theta_cm_theta",
      "Neutron c.m. vs lab angle;#theta_{lab} [deg];#theta_{c.m.} [deg]", 160, 0.0, 40.0, 180, 0.0, 180.0);
    TH1D* theta_cm_1d = new TH1D("theta_cm",
      "Neutron c.m. angle (vertex beam energy);#theta_{c.m.} [deg];Hits", 180, 0.0, 180.0);
    TH1D* tof = new TH1D("nTOF", "Neutron time of flight;nTOF [ns];Hits", 1200, -50.0, 250.0);
    TH1D* gamma_check = new TH1D("gamma_check",
      "Non-neutron hits: TOF - r/c (gammas at 0);TOF - r/c [ns];Hits", 800, -50.0, 150.0);
    TH1D* vertex = new TH1D("vertex",
      "TexAT vertex;beam axis from the entrance window [mm];Pairs", 230, 190.0, 420.0);
    TH1D* flightpath = new TH1D("flightpath",
      "Vertex-to-crystal flight path;r [mm];Hits", 400, 0.0, 2000.0);
    TH1D* ecm = new TH1D("Ecm",
      "E_{c.m.} from the vertex;E_{c.m.} [MeV];Hits", 120, 0.0, 6.0);
    TH2D* en_ecm_n = new TH2D("En_Ecm_n",
      "Neutron energy vs E_{c.m.} from the neutron (residual g.s.);E_{c.m.} [MeV];E_{n} [MeV]",
      200, 0.0, 10.0, 400, 0.0, 20.0);
    TH2D* en_ecm_n_ias = new TH2D("En_Ecm_n_ias",
      "Neutron energy vs E_{c.m.} from the neutron (residual IAS);E_{c.m.} [MeV];E_{n} [MeV]",
      200, 0.0, 10.0, 400, 0.0, 20.0);
    TH2D* ecm_ecm_n = new TH2D("Ecm_Ecm_n",
      "E_{c.m.}: vertex vs neutron (residual g.s.);E_{c.m.} from the neutron [MeV];E_{c.m.} from the vertex [MeV]",
      200, 0.0, 10.0, 120, 0.0, 6.0);
    TH2D* ecm_ecm_n_ias = new TH2D("Ecm_Ecm_n_ias",
      "E_{c.m.}: vertex vs neutron (residual IAS);E_{c.m.} from the neutron [MeV];E_{c.m.} from the vertex [MeV]",
      200, 0.0, 10.0, 120, 0.0, 6.0);
    TH1D* ex = new TH1D("Ex",
      "Residual excitation (missing mass);E_{x} [MeV];Hits", 300, -10.0, 20.0);

    int tn_run = run.pair.texneut_run;
    int ta_run = run.pair.texat_run;
    int dataset = beam.dataset;
    ULong64_t timestamp = 0;
    int bar = 0, crystal = 0;
    bool isneutron = false, passes = false;
    double vertex_mm = 0, t_reaction = 0, tdc_top = 0, tdc_bot = 0;
    double r = 0, theta = 0, phi = 0, hit_tof = 0, tof_minus_rc = 0, En = 0, psd = 0, light = 0;
    double Ebeam = 0, Ecm = 0, Ecm_n = 0, Ecm_n_ias = 0, Ex = 0, theta_cm = 0;
    const kinematics::Reaction reaction = reactions::pn_reaction(dataset);
    const double analog_state = reactions::analog_state_MeV(dataset);
    TTree* hits = new TTree("hits", "In-crystal TexNeut hits with TexAT vertices");
    hits->Branch("tn_run", &tn_run, "tn_run/I");
    hits->Branch("ta_run", &ta_run, "ta_run/I");
    hits->Branch("dataset", &dataset, "dataset/I"); //0 = 6He, 1 = 9Li
    hits->Branch("timestamp", &timestamp, "timestamp/l");
    hits->Branch("bar", &bar, "bar/I");
    hits->Branch("crystal", &crystal, "crystal/I");
    hits->Branch("isneutron", &isneutron, "isneutron/O");
    hits->Branch("passes", &passes, "passes/O"); //passes the joint config's neutron_filter
    hits->Branch("vertex_mm", &vertex_mm, "vertex_mm/D");
    hits->Branch("t_reaction", &t_reaction, "t_reaction/D");
    hits->Branch("tdc_top", &tdc_top, "tdc_top/D");
    hits->Branch("tdc_bot", &tdc_bot, "tdc_bot/D");
    hits->Branch("r", &r, "r/D");
    hits->Branch("theta", &theta, "theta/D"); //deg
    hits->Branch("phi", &phi, "phi/D");       //deg
    hits->Branch("tof", &hit_tof, "tof/D");
    hits->Branch("tof_minus_rc", &tof_minus_rc, "tof_minus_rc/D");
    hits->Branch("En", &En, "En/D");
    hits->Branch("psd", &psd, "psd/D");
    hits->Branch("light", &light, "light/D"); //summed PMT light, not the neutron energy
    hits->Branch("Ebeam", &Ebeam, "Ebeam/D"); //beam energy at the vertex
    hits->Branch("Ecm", &Ecm, "Ecm/D");       //from the vertex
    hits->Branch("Ecm_n", &Ecm_n, "Ecm_n/D"); //from the neutron, residual ground state
    hits->Branch("Ecm_n_ias", &Ecm_n_ias, "Ecm_n_ias/D"); //from the neutron, residual IAS
    hits->Branch("Ex", &Ex, "Ex/D");          //residual excitation from vertex and neutron
    hits->Branch("theta_cm", &theta_cm, "theta_cm/D"); //deg, neutron from the beam direction in c.m.

    const double deg_per_rad = TMath::RadToDeg();
    for(std::size_t i = 0; i < run.texneut_events.size(); ++i){
      const TN_obj::Event& event = run.texneut_events[i];
      vertex_mm = run.vertices[i][1];
      t_reaction = beam.reaction_time_ns(vertex_mm);
      Ebeam = beam.beam_energy_MeV(vertex_mm);
      Ecm = kinematics::cm_energy_MeV(reaction, Ebeam);
      vertex->Fill(vertex_mm);
      timestamp = event.timestamp;
      for(const TN_obj::Hit& hit : event.hits){
        if(!hit.incrystal) continue;
        bar = hit.barid;
        crystal = hit.crystalid;
        isneutron = hit.isneutron;
        passes = TN_obj::passes_filter(hit, filter);
        tdc_top = hit.tophit.TDC_corr;
        tdc_bot = hit.bottomhit.TDC_corr;
        r = hit.position.r;
        theta = hit.position.theta*deg_per_rad;
        phi = hit.position.phi*deg_per_rad;
        hit_tof = hit.TOF;
        tof_minus_rc = hit.TOF - r/timing::c_mm_per_ns;
        En = hit.neutron_energy;
        psd = hit.psd;
        light = hit.energy;
        //NaN neutron energies stay NaN through all three
        Ecm_n = kinematics::cm_energy_MeV(reaction,
          kinematics::beam_energy_from_neutron_MeV(reaction, En, hit.position.theta, 0.0));
        Ecm_n_ias = kinematics::cm_energy_MeV(reaction,
          kinematics::beam_energy_from_neutron_MeV(reaction, En, hit.position.theta, analog_state));
        Ex = kinematics::residual_excitation_MeV(reaction, Ebeam, En, hit.position.theta);
        theta_cm = kinematics::neutron_cm_angle_rad(reaction, Ebeam, En, hit.position.theta)*deg_per_rad;
        hits->Fill();
        ++counts.hits;

        flightpath->Fill(r);
        if(!isneutron && std::isfinite(tof_minus_rc)) gamma_check->Fill(tof_minus_rc);
        if(!passes) continue;
        if(std::isfinite(hit_tof)) tof->Fill(hit_tof);
        if(std::isfinite(En)){
          en_theta->Fill(theta, En);
          ecm->Fill(Ecm);
          ex->Fill(Ex);
          if(std::isfinite(theta_cm)){
            en_theta_cm->Fill(theta_cm, En);
            theta_cm_lab->Fill(theta, theta_cm);
            theta_cm_1d->Fill(theta_cm);
          }
          if(std::isfinite(Ecm_n)){
            en_ecm_n->Fill(Ecm_n, En);
            ecm_ecm_n->Fill(Ecm_n, Ecm);
          }
          if(std::isfinite(Ecm_n_ias)){
            en_ecm_n_ias->Fill(Ecm_n_ias, En);
            ecm_ecm_n_ias->Fill(Ecm_n_ias, Ecm);
          }
          ++counts.neutron_energies;
        }
      }
    }
    file->Write();
  }
  file->Close();
  return counts;
}
