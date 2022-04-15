//
// file : database.hpp
// in : file:///home/tim/projects/enfield/enfield/database.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 14:19:34 GMT-0500 (EST)
//
//
// Copyright (c) 2016 Timothée Feuillet
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

#ifndef __N_3158130790807728943_2877410136_DATABASE_HPP__
#define __N_3158130790807728943_2877410136_DATABASE_HPP__

#include <unordered_set>
#include <algorithm>
#include <array>
#include <deque>
#include <vector>
#include <type_traits>

#include "enfield_types.hpp"
#include "database_conf.hpp"
#include "type_registry.hpp"
#include "attached_object_utility.hpp"
#include "query.hpp"

#include <ntools/memory_pool.hpp>
#include <ntools/function.hpp>
#include <ntools/debug/assert.hpp>
#include <ntools/tracy.hpp>
#include <ntools/threading/threading.hpp>

namespace neam
{
  namespace enfield
  {
    /// \brief Where components are stored
    /// \warning The database isn't thread safe yet, except the run_systems() call
    /// \note The way the database perform allocations is really bad and should be improved
    /// \tparam DatabaseConf The database configuration (default_database_conf should be more than correct for most usages)
    template<typename DatabaseConf>
    class database
    {
      public:
        using conf_t = DatabaseConf;
        using entity_t = entity<DatabaseConf>;
//        using attached_object_mask_t = inline_mask<DatabaseConf>;
        using base_t = attached_object::base<DatabaseConf>;
        template<typename... Types>
        using attached_object_utility_t = attached_object_utility<DatabaseConf, Types...>;

      private: // check the validity of the compile-time conf
        static_assert(DatabaseConf::max_attached_objects_types % (sizeof(uint64_t) * 8) == 0, "database's Conf::max_attached_objects_types property must be a multiple of uint64_t");
        template<typename Type>
        using rm_rcv = typename std::remove_reference<typename std::remove_cv<Type>::type>::type;

        using entity_data_t = typename entity_t::data_t;

        struct attached_object_db_t
        {
          // deletion is the trigger point for re-arrangin the array
          std::atomic<uint32_t> deletion_count;

          spinlock lock;
          std::deque<base_t*> db;
        };

        database(const database&) = delete;
        database& operator = (const database&) = delete;

      public:
        database()
        {
          cr::out().debug("number of registered types: {}", type_registry<DatabaseConf>::allocator_info().size());
          // go over the registered types to setup the allocator:
          auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
          auto& debug_info = type_registry<DatabaseConf>::debug_info();
          for (size_t i = 0; i < allocator_info.size(); ++i)
          {
            cr::out().debug("  {}: size of {} bytes, aligned on {} bytes", debug_info[i].type_name, allocator_info[i].size, allocator_info[i].alignment);
            allocator.init_for_type(allocator_info[i].id, allocator_info[i].size, allocator_info[i].alignment);
          }
        }

      public:
        ~database()
        {
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            apply_component_db_changes();
          }

          check::debug::n_assert(entity_data_pool.get_number_of_object() == 0, "There are entities that are still alive AFTER their database has been destructed. This will lead to crashes.");
        }

        /// \brief Create a new entity
        entity_t create_entity()
        {
          entity_data_t* data = entity_data_pool.allocate();
          new (data) entity_data_t(*this); // construct

          entity_t ret(*data);
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data->assert_valid();
#endif

          // for systems
          std::lock_guard<spinlock> _lg(entity_list_lock);
          data->index = entity_list.size();
          entity_list.push_back(data);

          return ret;
        }

        size_t get_entity_count() const
        {
          return entity_list.size();
        }

        size_t get_attached_object_count(type_t id) const
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            return 0;
          }
          else
          {
            check::debug::n_assert(id < DatabaseConf::max_attached_objects_types, "get_attached_object_count: type-id is too big (id: {}, max: {})", id, DatabaseConf::max_attached_objects_types);
            return attached_object_db[id].db.size();
          }
        }

        /// \brief Iterate over each attached object of a given type
        /// \tparam Function a function or function-like object that takes as argument (const) references to the attached object to query
        /// \note If your function performs entity removal / ... then you may not iterate over each entity and you shoud use a query instead
        ///       as query() perform a copy of the vector
        /// \note Might miss attached objects added before apply_component_db_changes
        /// \see query
        template<typename Function>
        void for_each(Function&& func)
        {
          TRACY_SCOPED_ZONE;
          using list = ct::list::for_each<typename ct::function_traits<Function>::arg_list, rm_rcv>;

          for_each_list<list>(func);
        }

        template<typename Function>
        void for_each(Function&& func) const
        {
          TRACY_SCOPED_ZONE;
          using list = ct::list::for_each<typename ct::function_traits<Function>::arg_list, rm_rcv>;

          for_each_list<list>(func);
        }

        /// \brief Perform a query in the DB.
        /// \see for_each
        /// \note Calling apply_component_db_changes invalidates existing queries
        /// \note Calling apply_component_db_changes is necessary at least once a frame
        /// \note Might miss attached objects added before apply_component_db_changes
        template<typename AttachedObject>
        query_t<DatabaseConf, AttachedObject> query() const
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot perform queries when use_attached_object_db is false");
            return {};
          }

          TRACY_SCOPED_ZONE;

          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject::ao_class_id, attached_object_access::user_getable>();

          const type_t attached_object_id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
          if (attached_object_id > DatabaseConf::max_attached_objects_types)
            return {};

          std::deque<AttachedObject*> ret;
          for (uint32_t i = 0; i < attached_object_db[attached_object_id].db.size(); ++i)
          {
            base_t* ptr = attached_object_db[attached_object_id].db[i];
            if (ptr->authorized_destruction == false)
              ret.push_back(static_cast<AttachedObject*>(ptr));
          }

          return {ret};
        }

        /// \brief Optimize the DB for cache coherency
        /// Calling this function every now and then will prevent the DB from slowing-down too much
        /// \warning VERY SLOW
        /// \note should be called after apply_component_db_changes
        void optimize()
        {
          TRACY_SCOPED_ZONE;
          if (entity_deletion_count.load(std::memory_order_acquire) > k_deletion_count_to_optimize)
          {
            std::lock_guard<spinlock> _lg(entity_list_lock);
            entity_deletion_count.store(0, std::memory_order_release);
            std::sort(entity_list.begin(), entity_list.end());
            for (uint32_t i = 0; i < entity_list.size(); ++i)
              entity_list[i]->index = i;
          }

          if constexpr(!DatabaseConf::use_attached_object_db) return;

          for (auto& it : attached_object_db)
          {
            if (it.deletion_count.load(std::memory_order_acquire) < k_deletion_count_to_optimize)
              continue;
            std::lock_guard<spinlock> _lg(it.lock);
            it.deletion_count.store(0, std::memory_order_release);
            std::sort(it.db.begin(), it.db.end());
            for (uint32_t i = 0; i < it.db.size(); ++i)
              it.db[i]->index = i;
          }
        }

        /// \brief Optimize the DB for cache coherency
        /// Calling this function every now and then will prevent the DB from slowing-down too much
        /// \warning VERY SLOW
        /// \note should be called after apply_component_db_changes
        threading::task_wrapper optimize(threading::task_manager& tm, threading::group_t group_id = threading::k_non_transient_task_group)
        {
          auto final_task = tm.get_task(group_id, []{});

          if (entity_deletion_count.load(std::memory_order_acquire) > k_deletion_count_to_optimize)
          {
            auto sort = tm.get_task(group_id, [this]
            {
              TRACY_SCOPED_ZONE;
              std::lock_guard<spinlock> _lg(entity_list_lock);
              entity_deletion_count.store(0, std::memory_order_release);
              std::sort(entity_list.begin(), entity_list.end());
              for (uint32_t i = 0; i < entity_list.size(); ++i)
                entity_list[i]->index = i;
            });
            final_task->add_dependency_to(*sort);
          }

          if constexpr(!DatabaseConf::use_attached_object_db) return final_task;

          for (auto& it : attached_object_db)
          {
            if (it.deletion_count.load(std::memory_order_acquire) < k_deletion_count_to_optimize)
              continue;
            if (it.db.empty())
              continue;
            auto ao_sort = tm.get_task(group_id, [&it]
            {
              TRACY_SCOPED_ZONE;
              std::lock_guard<spinlock> _lg(it.lock);
              it.deletion_count.store(0, std::memory_order_release);
              std::sort(it.db.begin(), it.db.end());
              for (uint32_t i = 0; i < it.db.size(); ++i)
                it.db[i]->index = i;
            });
            final_task->add_dependency_to(*ao_sort);
          }

          return final_task;
        }

        /// \brief Apply attached-object destruction, maintain the query caches
        /// \warning It must be called often (something like at the beginning of frames)
        /// \warning Calling this invalidates existing queries
        /// \note inherently single-threaded
        void apply_component_db_changes()
        {
          TRACY_SCOPED_ZONE;

          [[maybe_unused]] int64_t skipped_count = 0;
          [[maybe_unused]] int64_t added_count = 0;
          [[maybe_unused]] int64_t removed_count = 0;

          // lock all db:
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db)
              it.lock.lock();
          }

          for (auto& it : pending_change_db)
          {
            std::lock_guard<spinlock>(it.lock);
            for (auto* base : it.db)
            {
              if constexpr (!DatabaseConf::use_attached_object_db)
              {
                allocator.deallocate(base->fully_transient_attached_object, base->object_type_id, base);
              }
              else
              {
                if (base == nullptr) // transient object being removed
                {
                  ++skipped_count;
                  continue;
                }

                if (base->authorized_destruction)
                {
                  ++removed_count;
                  remove_from_attached_db(*base);
                }
                else
                {
                  ++added_count;
                  add_to_attached_db(*base);
                }
              }
            }
            // clear the pending changes:
            it.db.clear();
          }

          // unlock all db:
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db)
              it.lock.unlock();
          }

          if constexpr(DatabaseConf::use_attached_object_db)
          {
            TRACY_PLOT("db::apply_changes::skipped", skipped_count);
            TRACY_PLOT("db::apply_changes::added", added_count);
            TRACY_PLOT("db::apply_changes::removed", removed_count);
          }
        }

      private: // for each impl
        template<typename AO>
        using id_t = type_id<AO, typename DatabaseConf::attached_object_type>;

        template<typename AttachedObjectsList, typename Function>
        void for_each_list(const Function& func)
        {
          using utility = typename ct::list::extract<AttachedObjectsList>::template as<attached_object_utility_t>;

          utility::check();

          // get the vector with the less attached objects
          const type_t attached_object_id = utility::get_min_entry_count(*this);

          // generates the mask
          const inline_mask<DatabaseConf> mask = utility::make_mask();

          // for each !
          if constexpr (DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db[attached_object_id].db)
            {
              if (mask.match(it->owner.mask))
              {
                utility::call(func, *this, it->owner);
              }
            }
          }
          else
          {
            for (uint32_t i = 0; i < get_entity_count(); ++i)
            {
              entity_data_t& data = get_entity(i);
              if (mask.match(data.mask))
              {
                utility::call(func, *this, data);
              }
            }
          }
        }

        template<typename AttachedObjectsList, typename Function>
        void for_each_list(const Function& func) const
        {
          using utility = typename ct::list::extract<AttachedObjectsList>::template as<attached_object_utility_t>;

          utility::check();

          // get the vector with the less attached objects
          const type_t attached_object_id = utility::get_min_entry_count(*this);

          // generates the mask
          const inline_mask<DatabaseConf> mask = utility::make_mask();

          // for each !
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db[attached_object_id])
            {
              if (mask.match(it->owner.mask))
              {
                utility::call(func, *this, it->owner);
              }
            }
          }
          else
          {
            for (uint32_t i = 0; i < get_entity_count(); ++i)
            {
              entity_data_t& data = get_entity(i);
              if (mask.match(data.mask))
              {
                utility::call(func, *this, data);
              }
            }
          }

        }

      private:
        template<typename AttachedObject>
        bool entity_has(const entity_data_t& data) const
        {
          const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
          return data.mask.is_set(id);
        }

        template<typename AttachedObject>
        AttachedObject* entity_get(entity_data_t& data)
        {
          return data.template get<AttachedObject>();
        }

        template<typename AttachedObject>
        const AttachedObject* entity_get(const entity_data_t& data) const
        {
          return data.template get<AttachedObject>();
        }

        entity_data_t& get_entity(size_t index)
        {
          return *entity_list[index];
        }

        const entity_data_t& get_entity(size_t index) const
        {
          return *entity_list[index];
        }

        base_t* get_attached_object(size_t index, type_t id)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use attached-object-db when use_attached_object_db is false");
            return nullptr;
          }

          base_t* ret = attached_object_db[id].db[index];
          if (ret->authorized_destruction)
            return nullptr;
          return ret;
        }

        const base_t* get_attached_object(size_t index, type_t id) const
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use attached-object-db when use_attached_object_db is false");
            return nullptr;
          }

          const base_t* ret = attached_object_db[id].db[index];
          if (ret->authorized_destruction)
            return nullptr;
          return ret;
        }

        entity_data_t* get_attached_object_owner(size_t index, type_t id)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use attached-object-db when use_attached_object_db is false");
            return nullptr;
          }

          base_t* ret = attached_object_db[id].db[index];
          if (ret->authorized_destruction)
            return nullptr;
          return &ret->owner;
        }

        const entity_data_t* get_attached_object_owner(size_t index, type_t id) const
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use attached-object-db when use_attached_object_db is false");
            return nullptr;
          }

          const base_t* ret = attached_object_db[id].db[index];
          if (ret->authorized_destruction)
            return nullptr;
          return &ret->owner;
        }

        void remove_entity(entity_data_t& data)
        {
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data.assert_valid();
#endif
          // loop over all "hard-added" attached objects in order to remove them
          // as after this pass every "soft-added" attached objects (views, requested, ...) should all be removed, unless cycles exist
          std::vector<base_t*> to_remove;
          for (auto& it : data.attached_objects)
          {
            if (it.second->user_added)
              to_remove.push_back(it.second);
          }
          for (auto& it : to_remove)
            remove_ao_user(data, *it);


          // remove the entity from the list
          {
            std::lock_guard<spinlock> _lg(entity_list_lock);
            check::debug::n_assert(entity_list[data.index] == &data, "Trying to remove and entity from a different DB");
            entity_list.back()->index = data.index;
            entity_list[data.index] = entity_list.back();
            entity_list.pop_back();
          }
          entity_deletion_count.fetch_add(1, std::memory_order_release);

          // This error mostly tells you that you have dependency cycles in your attached objects.
          // You can put a breakpoint here and look at what is inside the attached_objects vector.
          //
          // This isn't toggled by ENFIELD_ENABLE_DEBUG_CHECKS because it's a breaking error. It won't produce any code in "super-release" builds
          // where n_assert will just expand to a dummy, but stil, if that error appears this means that some of your attached objects are wrongly created.
          check::debug::n_assert(data.attached_objects.empty(), "There's still attached objects on an entity while trying to destroy it (do you have dependency cycles ?)");

          // free the memory
          data.~entity_data_t();
          entity_data_pool.deallocate(&data);
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject& add_ao_user(entity_data_t& data, attached_object::creation_flags flags, DataProvider&& ...provider)
        {
          AttachedObject& ret = _create_ao<AttachedObject>(data, flags, std::forward<DataProvider>(provider)...);
          base_t* bptr = &ret;
          check::debug::n_assert(bptr != (base_t*)(k_poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
          bptr->user_added = true;
          return ret;
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject& add_ao_dep(entity_data_t& data, attached_object::creation_flags flags, base_t& requester, DataProvider&& ... provider)
        {
          const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id();

          base_t* bptr;
          if (data.has(id))
          {
            // it already exists
            bptr = data.get(id);
            check::debug::n_assert(bptr != (base_t*)(k_poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
          }
          else
          {
            // create it
            bptr = &_create_ao<AttachedObject>(data, flags, std::forward<DataProvider>(provider)...);
          }

          bptr->required_count += 1;
          requester.requirements.set(id);
          AttachedObject& ret = *static_cast<AttachedObject*>(bptr);
          return ret;
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject& _create_ao(entity_data_t& data,
                                   attached_object::creation_flags flags,
                                   DataProvider&& ... provider)
        {
          if (flags == attached_object::creation_flags::none)
            flags = AttachedObject::default_creation_flags;

          const type_t object_type_id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id();
          data.mask.set(object_type_id);

          // make the get/add<AttachedObject>() segfault
          // (this helps avoiding incorrect usage of partially constructed attached objects)
          uint32_t index = data.attached_objects.size();
          data.attached_objects.emplace_back(object_type_id, (base_t*)(k_poisoned_pointer));

          const bool is_transient = flags == attached_object::creation_flags::transient;
          void* raw_ptr = allocator.allocate(is_transient, object_type_id, sizeof(AttachedObject), alignof(AttachedObject));

          AttachedObject* ptr = new (raw_ptr) AttachedObject(data, std::forward<DataProvider>(provider)...);

          ptr->set_creation_flags(flags);

          check::debug::n_assert(raw_ptr == (void*)static_cast<base_t*>(ptr), "attached-object base must be the first in the inheritence tree");

          // replace poisoned pointer with the actual one (as the object has now been fully constructed)
          data.attached_objects[index].second = ptr;

          check::debug::n_assert(is_transient == ptr->fully_transient_attached_object, "invalid mix between a transient creation flag and a class not flagged as transient");

          if constexpr (DatabaseConf::use_attached_object_db)
          {
            if (!ptr->fully_transient_attached_object)
            {
              if (ptr->force_immediate_db_change)
              {
                // force immediate changes. slow.
                std::lock_guard<spinlock> _lg(attached_object_db[object_type_id].lock);
                add_to_attached_db(*ptr);
              }
              else
              {
                add_to_pending_change_db(*ptr);
              }
            }
          }

          return *ptr;
        }

        /// \note This remove path is the user-requested path
        void remove_ao_user(entity_data_t& data, base_t& base)
        {
          check::debug::n_assert(base.user_added, "Invalid usage of remove_ao_user(): attached object is not user-added");

          base.user_added = false;
          if (base.required_count > 0)
            return;

          _delete_ao(base, data);
        }

        /// \note This remove path is the dependency path
        void remove_ao_dep(base_t& base, entity_data_t& data, base_t& requester)
        {
          check::debug::n_assert(&base != (base_t*)(k_poisoned_pointer), "The attached object being removed is also being constructed");
          {
            check::debug::n_assert(requester.requirements.is_set(base.object_type_id), "remove_ao_dep() is wrongly used (requester did not require<> the ao)");
            check::debug::n_assert(!base.requirements.is_set(requester.object_type_id), "remove_ao_dep(): circular dependency found");
            requester.requirements.unset(base.object_type_id);
          }

          --base.required_count;

          if (base.required_count > 0)
            return;
          if (base.user_added)
            return;

          _delete_ao(base, data);
        }

        void cleanup_ao_dependencies(base_t& base, entity_data_t& data)
        {
          for (uint32_t i = 0; i <  data.attached_objects.size();)
          {
            auto& it = data.attached_objects[i];
            if (base.requirements.is_set(it.first))
            {
              check::debug::n_assert(!it.second->authorized_destruction, "Dependency cycle detected when trying to remove an attached object");
              if (!it.second->authorized_destruction)
                remove_ao_dep(*it.second, data, base);

              continue;
            }
            ++i;
          }
        }

        void _delete_ao(base_t& base, entity_data_t& data)
        {
          base.authorized_destruction = true;

          data.remove_attached_object(base.object_type_id);

          // Perform the deletion
          data.mask.unset(base.object_type_id);

          // destruct (always, to keep the nice C++ resource management pattern and avoid nasty surprises)
          base.~base_t();

          // only when supported by the configuration.
          // no compile-time checks here because that function (and some of its caller)
          // may does not know the type of the element to remove, so it cannot perform compile-time branching.
          if constexpr (DatabaseConf::use_attached_object_db)
          {
            if (!base.fully_transient_attached_object)
            {
              add_to_pending_change_db(base);
            }
            else
            {
              // we got a fully transient object, no need for anything else
              allocator.deallocate(base.object_type_id, &base);
            }
          }
          else
          {
            allocator.deallocate(base.fully_transient_attached_object, base.object_type_id, &base);
          }
        }

        void add_to_pending_change_db(base_t& base)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use the attached_object_db when use_attached_object_db is false");
            return;
          }

          thread_local uint32_t pending_change_db_index = (uint32_t)(reinterpret_cast<uint64_t>(&base) >> 3); // all pointers are aligned to 8

          if (base.authorized_destruction && !base.in_attached_object_db)
          {
            // doing this here is a tiny tiny bit faster:
            // (also, the attached object should have been marked as transient)
            uint32_t db_index = base.index & (~0u);
            uint32_t index = base.index >> 32;

            {
              std::lock_guard<spinlock> _lg(pending_change_db[db_index].lock);
              check::debug::n_assert(pending_change_db[db_index].db[index] == &base, "Invalid db state");
              pending_change_db[db_index].db[index] = nullptr;

              // we can destroy the object as it isn't present in the db, so queries cannot reference it
              DatabaseConf::attached_object_allocator::deallocate(base.object_type_id, &base);
            }
            return;
          }

          while (true)
          {
            pending_change_db_index = (pending_change_db_index + 1) % k_change_db_size;

            if (pending_change_db[pending_change_db_index].lock.try_lock())
            {
              std::lock_guard<spinlock> _lg(pending_change_db[pending_change_db_index].lock, std::adopt_lock);
              // we are already in the db
              if (!base.authorized_destruction)
              {
                base.in_attached_object_db = false;
                base.index = pending_change_db[pending_change_db_index].db.size() << 32 | pending_change_db_index;
              }
              pending_change_db[pending_change_db_index].db.push_back(&base);
              return;
            }
          }
        }

        // NOTE: lock must be held
        void add_to_attached_db(base_t& base)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use the attached_object_db when use_attached_object_db is false");
            return;
          }

          check::debug::n_assert(base.object_type_id < DatabaseConf::max_attached_objects_types, "Invalid attached object to add");

          if (base.in_attached_object_db)
            return;
          base.in_attached_object_db = true;

          base.index = attached_object_db[base.object_type_id].db.size();
          attached_object_db[base.object_type_id].db.push_back(&base);
        }

        // NOTE: lock must be held
        void remove_from_attached_db(base_t& base)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use the attached_object_db when use_attached_object_db is false");
            return;
          }

          check::debug::n_assert(base.object_type_id < DatabaseConf::max_attached_objects_types, "Invalid attached object to remove");

          if (base.in_attached_object_db)
          {
            check::debug::n_assert(attached_object_db[base.object_type_id].db[base.index] == &base, "Incoherent DB state");

            attached_object_db[base.object_type_id].deletion_count.fetch_add(1, std::memory_order_release);

            attached_object_db[base.object_type_id].db.back()->index = base.index;
            attached_object_db[base.object_type_id].db[base.index] = attached_object_db[base.object_type_id].db.back();
            attached_object_db[base.object_type_id].db.pop_back();
          }

          DatabaseConf::attached_object_allocator::deallocate(base.object_type_id, &base);
        }

      private:
        /// \brief The database of components / concepts / *, sorted by type_t (attached_object_type)
        /// This is only used for queries.
        static constexpr uint32_t k_attached_object_db_size = DatabaseConf::use_attached_object_db ? DatabaseConf::max_attached_objects_types : 0;
        attached_object_db_t attached_object_db[k_attached_object_db_size];

        static constexpr uint32_t k_change_db_size = DatabaseConf::use_attached_object_db ? 8 : 0;
        // contains both pending addition and deletion, used to avoid as much as possible lock contention
        // increasing the entry count might help with higher thread count CPUs in case contention is still to important
        // removing the for-each/queries support completly remove the need 
        attached_object_db_t pending_change_db[k_change_db_size];

        static constexpr uint32_t k_deletion_count_to_optimize = 1024;

        // deletion is the trigger point for re-arranging the array
        std::atomic<uint32_t> entity_deletion_count;
        spinlock entity_list_lock;
        std::deque<entity_data_t*> entity_list;

        neam::cr::memory_pool<entity_data_t> entity_data_pool;

        typename DatabaseConf::attached_object_allocator allocator;

        friend class entity<DatabaseConf>;
        friend class attached_object::base<DatabaseConf>;
        template<typename DBC, typename AttachedObjectClass, typename FC, attached_object::creation_flags>
        friend class attached_object::base_tpl;
        template<typename DBCFG, typename SystemClass> friend class system;
        friend class base_system<DatabaseConf>;
        friend class system_manager<DatabaseConf>;
        template<typename DBC, typename... AttachedObjects> friend struct attached_object_utility;

        friend class system_manager<DatabaseConf>;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_3158130790807728943_2877410136_DATABASE_HPP__

