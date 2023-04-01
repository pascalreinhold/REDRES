#include "engine.hpp"
#include <stdexcept>
#include <iostream>

int main(int argc, char* argv[]){
  const char* db_filepath = (argc >= 2) ? argv[1] : "";
  const char* assets_dir_path = (argc >= 3) ? argv[2] : nullptr;

  if(argc > 3) {
    std::cout << "Unnecessary command line arguments:\n";
    for(int i = 3; i < argc; i++) std::cout << argv[i] << std::endl;
  }

  try {
    rcc::Engine engine(db_filepath, assets_dir_path);
    engine.init();
    engine.run();
  } catch (const std::exception& err){
    std::cerr << err.what() << "\n";
    return 1;
  }

  return 0;
}