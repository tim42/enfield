
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <enfield/tools/logger/logger.hpp>
#include <enfield/tools/chrono.hpp>

#include <enfield/enfield.hpp>
#include <enfield/concept/serializable.hpp>

#include "conf.hpp"
#include "auto_updatable.hpp"
#include "components.hpp"

constexpr size_t entity_count = 10000;
constexpr size_t frame_count = 10000;

void init_entities(neam::enfield::database<sample::db_conf> &db, std::list<neam::enfield::entity<sample::db_conf>> &list)
{
  for (size_t i = 0; i < entity_count; ++i)
  {
    list.emplace_back(db.create_entity());
    auto &entity = list.back();

    // insert a bunch of attached objects (some of which are auto-updatable)
    entity.add<sample::comp_1>();
//     if (i % 2 == 0)
      entity.add<sample::comp_2>();
  }
}

int main(int, char **)
{
  neam::cr::out.log_level = neam::cr::stream_logger::verbosity_level::debug;

  neam::enfield::database<sample::db_conf> db;

  // Add the auto-updatable system
  db.add_system<sample::auto_updatable::system>();

  // just used to hold entities (that way they aren't destroyed)
  std::list<neam::enfield::entity<sample::db_conf>> entity_list;

  neam::cr::out.log() << LOGGER_INFO << "creating a bunch of entities [" << entity_count << "]..." << std::endl;

  // create a bunch of entities
  init_entities(db, entity_list);

  neam::cr::out.log() << LOGGER_INFO << "running a bit the systems [" << frame_count << " frames]..." << std::endl;

  neam::cr::chrono chr;

  // run the systems for quite a bit
  // NOTE: this could be multithreaded for a lower run time, but as we're quite fast, I guess that's OK
  for (size_t i = 0; i < frame_count; ++i)
    db.run_systems();

  const double dt = chr.delta();

  neam::cr::out.log() << LOGGER_INFO << "done."
                      "Average frame duration: " << ((dt / double(frame_count)) * 1000.) << "ms, "
                      "time per entity: " << ((dt / double(frame_count * entity_count)) * 1000.) << "ms" << std::endl;

  // die
  return 0;
}
