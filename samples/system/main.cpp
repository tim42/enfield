
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <list>
#include <ntools/logger/logger.hpp>
#include <ntools/id/string_id.hpp>
#include <ntools/chrono.hpp>
#include <ntools/_tests/task_manager_helper.hpp>

#include <enfield/enfield.hpp>
// #include <enfield/concept/serializable.hpp>
#include <enfield/system/system_manager.hpp>

#include "conf.hpp"
#include "auto_updatable.hpp"
#include "components.hpp"

constexpr size_t frame_count = 250;
constexpr size_t thread_count = 7;
constexpr size_t entity_count = 16384 * 16 * (thread_count + 1);

void init_entities(neam::enfield::database<sample::db_conf> &db, std::list<neam::enfield::entity<sample::db_conf>> &list)
{
  uint64_t rnd = uint64_t(entity_count) << 32;
  for (size_t i = 0; i < entity_count; ++i)
  {
    rnd += rnd * rnd | 5;
    list.emplace_back(db.create_entity());
    auto &entity = list.back();

    // insert a bunch of attached objects (some of which are auto-updatable)
    entity.add<sample::comp_1>();
    if (((rnd >> 48) & 0x1) == 1 /*&& i < 200*/)
    {
      auto& c2 = entity.add<sample::comp_2>();
      if (i % 2)
        c2.update();
    }
    else //if (i < 700)
    {
      if ((rnd >> 49) & 1)
        entity.add<sample::comp_3>();
    }
  }
}

int main(int, char **)
{
  neam::cr::get_global_logger().min_severity = neam::cr::logger::severity::debug;
  neam::cr::get_global_logger().register_callback(neam::cr::print_log_to_console, nullptr);

  {
    neam::tm_helper_t tmh;
    neam::threading::task_manager& tm = tmh.tm;

    {
      neam::threading::task_group_dependency_tree tgd;
      tgd.add_task_group("cleanup-group"_rid);
      tgd.add_task_group("system-group"_rid);

      // system depends on cleanup
      tgd.add_dependency("system-group"_rid, "cleanup-group"_rid);

      // auto tree = tgd.compile_tree();
  //     tree.print_debug();

      tmh.setup(thread_count, std::move(tgd));
    }

    neam::enfield::database<sample::db_conf> db;
    neam::enfield::system_manager<sample::db_conf> sysmgr;

    // Add the auto-updatable system
    sysmgr.add_system<sample::auto_updatable::system>(db);
    sysmgr.add_system<sample::auto_updatable::system>(db);
    sysmgr.add_system<sample::auto_updatable::system>(db);

    // just used to hold entities (that way they aren't destroyed)
    std::list<neam::enfield::entity<sample::db_conf>> entity_list;

    neam::cr::out().log("creating a bunch of entities [{}]...", entity_count);

    // create a bunch of entities
    init_entities(db, entity_list);
    db.apply_component_db_changes();
    db.optimize();

    neam::cr::out().log("running a bit the systems [{} frames]...", frame_count);
    neam::cr::out().log("Using {} threads...", thread_count + 1);

    std::atomic<unsigned> frame_index = 0;
    tm.set_start_task_group_callback("cleanup-group"_rid, [&sysmgr, &tm, &db, &frame_index]()
    {
      TRACY_SCOPED_ZONE;
      db.apply_component_db_changes();
      db.optimize(tm, tm.get_group_id("cleanup-group"_rid));
      // db.optimize();
    });

    tm.set_start_task_group_callback("system-group"_rid, [&sysmgr, &tm, &tmh, &db, &frame_index]()
    {
      TRACY_SCOPED_ZONE;
      //sysmgr.push_tasks(db, tm, "system-group"_rid, true)
      sysmgr.push_tasks(db, tm, "system-group"_rid, false)
      .then([&]()
      // tm.get_task([&]
      {
        TRACY_SCOPED_ZONE;

        ++frame_index;
        if (frame_index >= frame_count)
          tmh.request_stop();

        size_t sz = 0;
        db.for_each([&sz](sample::comp_2&, sample::comp_3& ) { ++sz; });
        if (frame_index <= 2)
          neam::cr::out().debug(" matching comp2/comp3: {}", sz);
        static unsigned old_pct = 0;
        unsigned pct = (frame_index * 100 / frame_count);
        if (pct % 10 == 0 && old_pct != pct)
        {
          old_pct = pct;

          neam::cr::out().debug(" progress: {}%", pct);
        }
      });

      tm.get_task("system-group"_rid, [&tm, &db]()
      {
        // while the system runs, run some queries / for-each stuff:
  //       sz = db.query<sample::comp_1b>().filter<sample::comp_3>().result.size();
        db.for_each([](sample::comp_1b& , sample::comp_3& ) {});

        // just to test this:
        //tm.run_tasks(std::chrono::microseconds(1000));
      });
    });

    TRACY_NAME_THREAD("Worker");
    neam::cr::chrono chr;

    tmh.enroll_main_thread();
    tmh.join_all_threads();

    const double dt = chr.delta();

    neam::cr::out().log("done: Average frame duration: {:.6}ms, time per entity: {:.6}us",
                        ((dt / double(frame_count)) * 1e3),
                        ((dt / double(frame_count * entity_count)) * 1e6));

    entity_list.clear();
  }
  neam::cr::out().debug("completed run and cleanup");
  return 0;
}
