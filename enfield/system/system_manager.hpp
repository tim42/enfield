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
        System &add_system(Args &&... args)
        {
          systems.emplace_back(new System(std::forward<Args>(args)...));
          return static_cast<System &>(*systems.back());
        }
        /// \brief Remove a system from the list
        /// \warning This operation is quite slow
        template<typename System>
        void remove_system()
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          systems.erase(std::remove_if(systems.begin(), systems.end(), [id](auto& sys)
          {
            return (sys->system_id == id);
          }), systems.end());
        }

        /// \brief Retrieve a system from the list
        template<typename System>
        System &get_system()
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          for (auto& sys : systems)
          {
            if (sys->system_id == id)
              return *static_cast<System *>(sys);
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
          const threading::group_t group = tm.get_group_id(group_name);

          threading::task_wrapper final_task_wr = tm.get_task(group, [](){});

          // we only require the heavy/slow option when:
          //  - we have more than one system
          //  - we are required to have sync points
          if (sync_exec && systems.size() > 1)
          {
            sync_point(true, db, tm, *final_task_wr);
          }
          else // not sync_exec
          {
            index.store(0, std::memory_order_relaxed);

            // compute the number of task to dispatch, but limit that to a max number
            // to avoid saturating the task system
            const uint32_t entity_count = db.entity_list.size();
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
          if (initial)
            system_index = 0;
          else
          {
            ++system_index;
            check::debug::n_assert(index >= db.entity_list.size(), "invalid stuff ?");
//             return;
          }

          index.store(0, std::memory_order_release);

          // add the task for the next system
          if (system_index < systems.size())
          {
            // create the final sync task:
            threading::task_wrapper next_sync_wr = tm.get_task(final_task.get_task_group(), [this, &db, &tm, &final_task]()
            {
              sync_point(false, db, tm, final_task);
            });

            final_task.add_dependency_to(*next_sync_wr);

            const uint32_t entity_count = db.entity_list.size();
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
          check::debug::n_assert(system_index < systems.size(), "Invalid system index");

          const uint32_t base_index = index.fetch_add(entity_per_task);

          // for each entities, run all systems:
          for (uint32_t i = 0; i < entity_per_task && base_index + i < db.entity_list.size(); ++i)
          {
            entity_data_t* data = db.entity_list[base_index + i];

            systems[system_index]->try_run(data);
          }

          // not completed yet: we need more tasks:
          if (index < db.entity_list.size())
          {
            threading::task_wrapper task = tm.get_task(next_sync.get_task_group(), [this, &db, &tm, &next_sync]() { run_sync_exec(db, tm, next_sync); });
            next_sync.add_dependency_to(*task);
          }
        }

        void run_all_systems(database_t& db, threading::task_manager& tm, threading::task& final_task)
        {
          const uint32_t base_index = index.fetch_add(entity_per_task);

          // for each entities, run all systems:
          for (uint32_t i = 0; i < entity_per_task && base_index + i < db.entity_list.size(); ++i)
          {
            entity_data_t* data = db.entity_list[base_index + i];

            for (auto& sys : systems)
              sys->try_run(data);
          }

          // not completed yet: we need more tasks:
          if (index < db.entity_list.size())
          {
            threading::task_wrapper task = tm.get_task(final_task.get_task_group(), [this, &db, &tm, &final_task]() { run_all_systems(db, tm, final_task); });
            final_task.add_dependency_to(*task);
          }
        }

      private:
        const unsigned max_task_count = (std::thread::hardware_concurrency() + 2) * 2;
        unsigned entity_per_task = 256;

        unsigned system_index = 0;

        std::vector<std::unique_ptr<base_system<DatabaseConf>>> systems;

        alignas(64) std::atomic<uint32_t> index;
    };
#if 0
    /// \brief Manage a group of system and handle their parallel execution
    template<typename DatabaseConf>
    class system_manager
    {
      private:
        using entity_t = entity<DatabaseConf>;
        using entity_data_t = typename entity_t::data_t;

      public:
        system_manager() = default;
        system_manager(const system_manager&) = delete;
        system_manager& operator = (const system_manager&) = delete;

        ~system_manager()
        {
          clear_systems();
        }

        /// \brief Add a new system to the system list
        template<typename System, typename... Args>
        System &add_system(Args &&... args)
        {
          systems.push_back(new System(std::forward<Args>(args)...));
          return static_cast<System &>(*systems.back());
        }

        /// \brief Retrieve a system from the list
        /// \warning This operation is slow
        template<typename System>
        System &get_system()
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          for (base_system<DatabaseConf> *sys : systems)
          {
            if (sys->system_id == id)
              return *static_cast<System *>(sys);
          }
          throw exception_tpl<system_manager>("Could not find the corresponding system", __FILE__, __LINE__);
        }

        /// \brief Retrieve a system from the list
        /// \warning This operation is slow
        template<typename System>
        System &get_system() const
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          for (const base_system<DatabaseConf> *sys : systems)
          {
            if (sys->system_id == id)
              return *static_cast<const System *>(sys);
          }
          throw exception_tpl<system_manager>("Could not find the corresponding system", __FILE__, __LINE__);
        }

        /// \brief Check if a system is in the list
        /// \warning This operation is slow
        template<typename System>
        bool has_system() const
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          for (base_system<DatabaseConf> *sys : systems)
          {
            if (sys->system_id == id)
              return true;
          }
          throw false;
        }

        /// \brief Start a new frame / update cycle
        /// Must be called before any run_systems, and it must be called on a single and unique thread (usually the main thread).
        /// Forgetting to call this method will result in a deadlock
        void start_new_cycle()
        {
          // wait for the last run to finish
          while (thread_count.load(std::memory_order_acquire) != 0);

          // synchronously call begin on the systems
          // if an exception occurs there
          for (base_system<DatabaseConf> *sys : systems)
            sys->begin();

          // reset the index
          index = 0;

          // reset the base system index
          system_base_index = 0;

          barrier_thread_count = 0;

          // set the running flag to true
          is_running = true;
        }

        /// \brief Run the systems
        /// \note You can call this function on multiple threads at the same time
        /// \warning Do not do any \b **modify** operation on the DB/on any entities while this function is running.
        ///          Components/Concepts must not require anything nor unrequire things.
        /// \note It is the user duty to call start_new_cycle() before this
        /// \see start_new_cycle
        void run_systems(database<DatabaseConf>& db)
        {
          // wait for the proper conditions
          while (is_running.load(std::memory_order_acquire) == false);

          // increment the thread count
          ++thread_count;
          unsigned int base_index;

          {
            if (barrier_thread_count)
            {
              // do the system barrier, because it is active
              ++barrier_thread_count;

              // wait for the barrier to be lift
              while (barrier_thread_count.load(std::memory_order_acquire) != 0);

              // acquire the base system
              base_index = system_base_index.load(std::memory_order_acquire);
            }
            else
            {
              // the system barrier isn't active, simply load the value
              // if thread have entered the barrier at this point, this isn't harmful:
              // the current thread will go through the inner (entity) loop, see that there isn't any entities
              // and go through the barrier (threads will have be waiting for it).
              // Its sys_index will be equals to base_index (no entity have been grabbed), so it will simply
              // wait for the correct system_base_index to be computed
              //
              // If the thread actually do some work, it will be OK as its sys_index will have the correct value
              base_index = system_base_index.load(std::memory_order_acquire);
            }
          }

          // Run
          do
          {
            unsigned int sys_index = base_index;
            do
            {
              const unsigned int i = index++;

              if (size_t(i) >= db.entity_list.size())
                break;

              entity_data_t *data = db.entity_list[i];

              // run the entity on every system, until the next barrier
              for (sys_index = base_index; sys_index < systems.size() && (sys_index == base_index || !systems[sys_index]->has_barrier_before); ++sys_index)

                systems[sys_index]->try_run(data);
            } while (true);

            // We have encountered a barrier
            if (sys_index != systems.size())
            {
              ++barrier_thread_count;

              // set the next base index
              base_index = sys_index;

              // the actual barrier:
              while (barrier_thread_count.load(std::memory_order_acquire) != thread_count.load(std::memory_order_acquire));

              if (sys_index > base_index)
              {
                // set the new value, once the every thread reaches the barrier
                // we only care about storing
                system_base_index.store(sys_index, std::memory_order_release);

                // restore the index
                index = 0;

                // end the barrier
                barrier_thread_count.store(0, std::memory_order_release);
              }
              else
              {
                // we are a new thread, 
                while (barrier_thread_count.load(std::memory_order_acquire) != 0);
                base_index = barrier_thread_count.load(std::memory_order_acquire);
              }

              // loop again
              continue;
            }
          } while (false);

          // synchronously call the end() on the very last thread
          if (thread_count == 1)
          {
            // de-init the systems
            for (base_system<DatabaseConf> *sys : systems)
              sys->end();
          }
          --thread_count;

          // set the is_running flag to false: we are done with this update cycle
          // NOTE: This is after the is_terminating thing, because that way start_new_cycle() will wait for the total completion
          // before toggling the is_running flag to true and start a new cycle.
          is_running = false;
        }

        /// \brief Remove every system
        void clear_systems()
        {
          for (base_system<DatabaseConf> *sys : systems)
            delete sys;
          systems.clear();
        }

      private:
        std::vector<base_system<DatabaseConf> *> systems;

        std::atomic<int> thread_count = ATOMIC_VAR_INIT(0);
        std::atomic<int> barrier_thread_count = ATOMIC_VAR_INIT(0);
        std::atomic<unsigned int> index = ATOMIC_VAR_INIT(0);
        std::atomic<unsigned int> system_base_index = ATOMIC_VAR_INIT(0);
        std::atomic<bool> is_running = ATOMIC_VAR_INIT(false);
    };
#endif
} // namespace neam::enfield

#endif // __N_23127891179685730_431825828_SYSTEM_MANAGER_HPP__

