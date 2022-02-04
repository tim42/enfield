//
// file : system_manager.hpp
// in : file:///home/tim/projects/enfield/enfield/system/system_manager.hpp
//
// created by : Timothée Feuillet
// date: Mon Mar 27 2017 00:30:15 GMT+0200 (CEST)
//
//
// Copyright (c) 2017 Timothée Feuillet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef __N_23127891179685730_431825828_SYSTEM_MANAGER_HPP__
#define __N_23127891179685730_431825828_SYSTEM_MANAGER_HPP__

#include <vector>
#include <atomic>

#include "base_system.hpp"
#include "../entity.hpp"
#include "../database.hpp"

#include <ntools/spinlock.hpp>
#include <ntools/threading/threading.hpp>
#include <ntools/tracy.hpp>

namespace neam::enfield
{
  template<typename DatabaseConf>
  class system_manager
  {
    private:
      using database_t = database<DatabaseConf>;
      using entity_t = entity<DatabaseConf>;
      using entity_data_t = typename entity_t::data_t;

    public:
      system_manager() : systems() {}
      system_manager(const system_manager&) = delete;
      system_manager& operator = (const system_manager&) = delete;

      ~system_manager() {}

      /// \brief Add a new system to the system list
      template<typename System, typename... Args>
      System& add_system(Args&& ... args)
      {
        systems.emplace_back(new System(std::forward<Args>(args)...));
        return static_cast<System&>(*systems.back());
      }
      /// \brief Remove a system from the list
      /// \warning This operation is quite slow
      template<typename System>
      void remove_system()
      {
        const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
        systems.erase(std::remove_if(systems.begin(), systems.end(), [id](auto & sys)
        {
          return (sys->system_id == id);
        }), systems.end());
      }

      /// \brief Retrieve a system from the list
      template<typename System>
      System& get_system()
      {
        const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
        for (auto& sys : systems)
        {
          if (sys->system_id == id)
            return *static_cast<System*>(sys);
        }
        check::debug::n_assert(false, "Could not find system with type id {}", id);
      }

      /// \brief Check if a system is in the list
      template<typename System>
      bool has_system() const
      {
        const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
        for (const auto& sys : systems)
        {
          if (sys->system_id == id)
            return true;
        }
        return false;
      }

      /// \brief Push all task from all systems
      /// \param entity_per_task The number of entities that each task will grab
      /// \param sync_exec If false, entities in a chunk will go through all systems without any sync point
      ///                  If true, all entities will go through one system, then a sync point, then the next system, ...
      /// \return The final task (for synchronisation purpose)
      /// \note With sync_exec to false is the lightest option.
      /// \note All systems will belong to the same task group.
      ///       If you want to have parallel execution of systems, create multiple system managers
      ///
      /// \warning creating or destroying entities during the execution of a system is a very very bad idea
      threading::task& push_tasks(database_t& db, threading::task_manager& tm, neam::id_t group_name,
                                  bool sync_exec = false)
      {
        TRACY_SCOPED_ZONE;
        const threading::group_t group = tm.get_group_id(group_name);

        threading::task_wrapper final_task_wr;

        // we only require the heavy/slow option when:
        //  - we have more than one system
        //  - we are required to have sync points
        if (sync_exec && systems.size() > 1)
        {
          final_task_wr = tm.get_task(group, []() {});

          sync_point(true, db, tm, *final_task_wr);
        }
        else // not sync_exec
        {
          final_task_wr = tm.get_task(group, [this]()
          {
            // call end() on all the systems:
            for (auto& it : systems)
              it->end();
          });
          index.store(0, std::memory_order_relaxed);


          // call begin() on all the systems:
          for (auto& it : systems)
            it->begin();

          // compute the number of task to dispatch, but limit that to a max number
          // to avoid saturating the task system
          const uint32_t entity_count = db.get_entity_count();
          uint32_t dispatch_count = entity_count / entity_per_task;
          if (dispatch_count > max_task_count)
            dispatch_count = max_task_count;
          for (uint32_t i = 0; i < dispatch_count; ++i)
          {
            threading::task_wrapper task = tm.get_task(group, [this, &db, &tm, &final_task = *final_task_wr]() { run_all_systems(db, tm, final_task); });
            final_task_wr->add_dependency_to(*task);
          }
        }

        return *final_task_wr;
      }

    private:
      void sync_point(bool initial, database_t& db, threading::task_manager& tm, threading::task& final_task)
      {
        TRACY_SCOPED_ZONE;
        if (initial)
        {
          system_index = 0;
        }
        else
        {
          systems[system_index]->end();
          ++system_index;
        }

        index.store(0, std::memory_order_release);

        // add the task for the next system
        if (system_index < systems.size())
        {
          systems[system_index]->begin();

          // create the final sync task:
          threading::task_wrapper next_sync_wr = tm.get_task(final_task.get_task_group(), [this, &db, &tm, &final_task]()
          {
            sync_point(false, db, tm, final_task);
          });

          final_task.add_dependency_to(*next_sync_wr);

          const uint32_t entity_count = db.get_entity_count();
          // create the worker tasks:
          uint32_t dispatch_count = entity_count / entity_per_task;
          if (dispatch_count > max_task_count)
            dispatch_count = max_task_count;
          for (uint32_t i = 0; i < dispatch_count; ++i)
          {
            threading::task_wrapper task = tm.get_task(final_task.get_task_group(), [this, &db, &tm, &next_sync = *next_sync_wr]() { run_sync_exec(db, tm, next_sync); });
            next_sync_wr->add_dependency_to(*task);
          }
        }
      }

      void run_sync_exec(database_t& db, threading::task_manager& tm, threading::task& next_sync)
      {
        TRACY_SCOPED_ZONE;
        check::debug::n_assert(system_index < systems.size(), "Invalid system index");

        const uint32_t base_index = index.fetch_add(entity_per_task);

        // for each entities, run all systems:
        for (uint32_t i = 0; i < entity_per_task && base_index + i < db.get_entity_count(); ++i)
        {
          entity_data_t& data = db.get_entity(base_index + i);

          systems[system_index]->try_run(data);
        }

        // not completed yet: we need more tasks:
        if (index < db.get_entity_count())
        {
          threading::task_wrapper task = tm.get_task(next_sync.get_task_group(), [this, &db, &tm, &next_sync]() { run_sync_exec(db, tm, next_sync); });
          next_sync.add_dependency_to(*task);
        }
      }

      void run_all_systems(database_t& db, threading::task_manager& tm, threading::task& final_task)
      {
        TRACY_SCOPED_ZONE;
        const uint32_t base_index = index.fetch_add(entity_per_task);

        // for each entities, run all systems:
        for (uint32_t i = 0; i < entity_per_task && base_index + i < db.get_entity_count(); ++i)
        {
          entity_data_t& data = db.get_entity(base_index + i);

          for (auto& sys : systems)
            sys->try_run(data);
        }

        // not completed yet: we need more tasks:
        if (index < db.get_entity_count())
        {
          threading::task_wrapper task = tm.get_task(final_task.get_task_group(), [this, &db, &tm, &final_task]() { run_all_systems(db, tm, final_task); });
          final_task.add_dependency_to(*task);
        }
      }

    private:
      const unsigned max_task_count = (std::thread::hardware_concurrency() + 2) * 2;
      unsigned entity_per_task = 1024;

      unsigned system_index = 0;

      std::vector<std::unique_ptr<base_system<DatabaseConf>>> systems;

      alignas(64) std::atomic<uint32_t> index;
  };
} // namespace neam::enfield

#endif // __N_23127891179685730_431825828_SYSTEM_MANAGER_HPP__

