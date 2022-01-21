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

namespace neam
{
  namespace enfield
  {
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

        /// \brief Remove a system from the list
        /// \warning This operation is quite slow
        template<typename System>
        void remove_system()
        {
          const type_t id = type_id<System, typename DatabaseConf::system_type>::id;
          systems.erase(std::remove_if(systems.begin(), systems.end(), [id](base_system<DatabaseConf> *sys)
          {
            if (sys->system_id == id)
            {
              delete sys;
              return true;
            }
            return false;
          }), systems.end());
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
              {
                try
                {
                  systems[sys_index]->try_run(data);
                }
                catch (...)
                {
                  --thread_count; // die
                  throw; // forward the exception
                }
              }
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
  } // namespace enfield
} // namespace neam

#endif // __N_23127891179685730_431825828_SYSTEM_MANAGER_HPP__

