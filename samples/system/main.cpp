
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <list>
#include <ntools/logger/logger.hpp>
#include <ntools/id/string_id.hpp>
#include <ntools/chrono.hpp>

#include <enfield/enfield.hpp>
// #include <enfield/concept/serializable.hpp>
#include <enfield/system/system_manager.hpp>

#include "conf.hpp"
#include "auto_updatable.hpp"
#include "components.hpp"

constexpr size_t frame_count = 1000;
constexpr size_t thread_count = 7;
constexpr size_t entity_count = 16384 * 2 * (thread_count + 1);

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
  neam::cr::out.min_severity = neam::cr::logger::severity::debug;
  neam::cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

  {
    neam::threading::task_manager tm;
    {
      neam::threading::task_group_dependency_tree tgd;
      tgd.add_task_group("cleanup-group"_rid, "cleanup-group");
      tgd.add_task_group("system-group"_rid, "system-group");

      // system depends on cleanup
      tgd.add_dependency("system-group"_rid, "cleanup-group"_rid);

      auto tree = tgd.compile_tree();
  //     tree.print_debug();

      tm.add_compiled_frame_operations(std::move(tree));
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
    db.optimize(tm);

    neam::cr::out().log("running a bit the systems [{} frames]...", frame_count);
    neam::cr::out().log("Using {} threads...", thread_count + 1);

    std::atomic<unsigned> frame_index = 0;
    tm.set_start_task_group_callback("cleanup-group"_rid, [&sysmgr, &tm, &db, &frame_index]()
    {
      TRACY_SCOPED_ZONE;
      db.apply_component_db_changes();
      db.optimize(tm, tm.get_group_id("cleanup-group"_rid));
    });

    tm.set_start_task_group_callback("system-group"_rid, [&sysmgr, &tm, &db, &frame_index]()
    {
      TRACY_SCOPED_ZONE;
      sysmgr.push_tasks(db, tm, "system-group"_rid, true)
  //     sysmgr.push_tasks(db, tm, "system-group"_rid, false)
      .then([&]()
      {
        TRACY_SCOPED_ZONE;

        ++frame_index;

        size_t sz = 0;
        db.for_each([&sz](sample::comp_1b&, sample::comp_3& ) { ++sz; });
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
        tm.run_tasks(std::chrono::microseconds(1000));
      });
    });

    TRACY_NAME_THREAD("Worker");
    neam::cr::chrono chr;
    std::deque<std::thread> thr;

    for (unsigned i = 0; i < thread_count; ++i)
    {
      thr.emplace_back([&frame_index, &tm]()
      {
        TRACY_NAME_THREAD("Worker");
        while (frame_index < frame_count)
        {
          tm.wait_for_a_task();
          tm.run_a_task();
        }
      });
    }

    // run the systems for quite a bit
    for (size_t i = 0; frame_index < frame_count; ++i)
    {
      tm.wait_for_a_task();
      tm.run_a_task();
    }
    for (auto& it : thr)
    {
      if (it.joinable())
        it.join();
    }

    const double dt = chr.delta();

    neam::cr::out().log("done: Average frame duration: {:.6}ms, time per entity: {:.6}us",
                        ((dt / double(frame_count)) * 1e3),
                        ((dt / double(frame_count * entity_count)) * 1e6));

    entity_list.clear();
  }
  neam::cr::out().debug("completed run and cleanup");
  return 0;
}
