#include <fstream>
#include <iostream>
#include "yaml.h"
#include "yavl.h"

using namespace std;

int main(int argc, char **argv)
{
  YAML::Node gr;
  try {
    gr = YAML::LoadFile(argv[1]);
  } catch(const YAML::Exception& e) {
    std::cerr << "Error reading grammar: " << e.what() << "\n";
    return 1;
  }

  YAML::Node doc;
  try {
    doc = YAML::LoadFile(argv[2]);
  } catch(const YAML::Exception& e) {
    std::cerr << "Error reading document: " << e.what() << "\n";
    return 2;
  }

  YAVL::Validator yavl(gr, doc);
  bool ok = yavl.validate();
  if (!ok) {
    cout << "ERRORS FOUND: " << endl << endl;
    cout << yavl.get_errors();
  }
  return !ok;
}


