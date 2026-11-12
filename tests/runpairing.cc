#include "../include/runpairing.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace{

void require(bool condition, const std::string& message){
  if(!condition) throw std::runtime_error(message);
}

template<class Exception, class Action>
void expect_throw(Action action, const std::string& message){
  try{
    action();
  }catch(const Exception&){
    return;
  }
  throw std::runtime_error(message);
}

runpairing::RunPair pair(int tn, int ta){
  runpairing::RunPair result;
  result.texneut_run = tn;
  result.texat_run = ta;
  return result;
}

struct Fixture{
  std::string path = "runpairing_fixture_" + std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
  ~Fixture(){ std::remove(path.c_str()); }
  void write(const std::string& contents){
    std::ofstream file(path, std::ios::binary);
    file << contents;
    require(bool(file), "Could not write test fixture");
  }
};

void check_lookup(const std::string& info_directory){
  using namespace runpairing;
  RunLists lists;
  require(!lists.available(), "Empty lists should be unavailable");
  require(load_run_lists(lists, info_directory), "Could not load real run lists");
  require(lists.available(), "Real lists should be available");
  std::istringstream no_input;
  std::ostringstream output;
  require(lists.resolve(98, no_input, output).texneut_run == 1721, "98 -> 1721");
  require(lists.resolve(1721, no_input, output).texat_run == 98, "1721 -> 98");
  require(lists.resolve(170, no_input, output).texneut_run == 1765, "9Li lookup");
  require(output.str().empty(), "Unique runs must not prompt");
  require(lists.membership(98) == RunSet::SixHe, "TexAT 6He membership");
  require(lists.membership(1721) == RunSet::SixHe, "TexNeut 6He membership");
  require(lists.membership(1765) == RunSet::NineLi, "9Li membership");
  require(lists.membership(999999) == RunSet::None, "Unknown membership");
  require(lists.membership(98, RunSource::TexNeut) == RunSet::None,
          "Membership should honor the explicit detector");
  expect_throw<std::invalid_argument>([&]{ lists.resolve(999999); }, "Unknown ID must fail");
  expect_throw<std::invalid_argument>([&]{ lists.resolve(98, RunSource::TexNeut); },
                                      "Explicit detector must be honored");
  const auto old_size = lists.six_he.size();
  require(!load_run_lists(lists, info_directory + "/does-not-exist"), "Missing files must fail");
  require(lists.six_he.size() == old_size && lists.resolve(98).texneut_run == 1721,
          "Failed bulk loads must preserve previous lists");
}

void check_overlap(){
  using namespace runpairing;
  RunLists lists;
  lists.six_he.push_back(pair(1721, 98));
  require(lists.available(), "A single populated isotope set is usable");
  lists.nine_li.push_back(pair(98, 200));
  std::ostringstream output;
  std::istringstream ta("invalid TeXAT\n");
  require(lists.resolve(98, ta, output).texneut_run == 1721, "TexAT clarification");
  require(output.str().find("Please enter") != std::string::npos,
          "Invalid clarification must retry");
  require(output.str().find("TexNeut 98 -> TexAT 200") != std::string::npos,
          "Prompt should explain both interpretations");
  std::istringstream tn("tn\n");
  require(lists.resolve(98, tn, output).texat_run == 200, "TexNeut clarification");
  std::istringstream no_input;
  std::ostringstream no_output;
  require(lists.resolve(98, no_input, no_output, RunSource::TexAT).texneut_run == 1721,
          "Explicit TexAT selection");
  require(lists.resolve(98, no_input, no_output, RunSource::TexNeut).texat_run == 200,
          "Explicit TexNeut selection");
  require(no_output.str().empty(), "Explicit selections must not prompt");
  require(lists.membership(98, RunSource::TexAT) == RunSet::SixHe, "Overlap TexAT isotope");
  require(lists.membership(98, RunSource::TexNeut) == RunSet::NineLi, "Overlap TexNeut isotope");
  expect_throw<std::runtime_error>([&]{ lists.resolve(98, no_input, output); },
                                  "Unanswered clarification must fail");

  // Public vectors can also be populated directly: never choose the first of
  // several mappings for the same detector ID.
  lists.nine_li.push_back(pair(3000, 98));
  expect_throw<std::runtime_error>([&]{ lists.resolve(98, RunSource::TexAT); },
                                  "Conflicting mappings must fail");
}

void check_loading(){
  using namespace runpairing;
  Fixture fixture;
  std::vector<RunPair> runs;
  fixture.write("# TexNeut TexAT\r\n\r\n1721 98 # comment\r\n1765 170");
  require(load_run_list(runs, fixture.path) && runs.size() == 2, "Valid file forms");
  fixture.write("1722 99\n");
  require(load_run_list(runs, fixture.path) && runs.size() == 1 && runs[0].texat_run == 99,
          "Successful reload must replace previous rows");
  for(const auto& bad : {"", "# empty\n", "1721", "1721 98\n1722", "abc 98\n",
                         "1721 98 extra\n", "0 98\n", "1721 -1\n",
                         "1721 98\n1721 99\n", "1721 98\n1722 98\n",
                         "1721 98\n1721 98\n", "999999999999999999999 98"}){
    fixture.write(bad);
    require(!load_run_list(runs, fixture.path), "Invalid mapping must be rejected");
    require(runs.size() == 1 && runs[0].texat_run == 99,
            "Failed load must preserve previous rows");
  }
}

}

int main(int argc, char** argv){
  try{
    require(argc == 2, "Expected the run-list directory");
    check_lookup(argv[1]);
    check_overlap();
    check_loading();
    std::cout << "Run-pairing checks passed.\n";
  }catch(const std::exception& error){
    std::cerr << error.what() << '\n';
    return 1;
  }
}
