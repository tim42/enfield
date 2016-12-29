
// #include <neam/reflective/reflective.hpp>

//#define N_ALLOW_DEBUG true // we want full debug information
#define N_DISABLE_CHECKS // we don't want anything

#include <enfield/tools/logger/logger.hpp>

#include <enfield/enfield.hpp>


int main(int, char **)
{
  neam::enfield::database<neam::enfield::default_database_conf> db;
  auto entity = db.create_entity();

  return 0;
}
