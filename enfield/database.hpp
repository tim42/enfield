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

#pragma once


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

#ifndef ENFIELD_ENABLE_DEBUG_CHECKS
# define ENFIELD_ENABLE_DEBUG_CHECKS 1
#endif

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
          // deletion is the trigger point for re-arranging the array
          std::atomic<uint32_t> deletion_count;

          // operation on entries in the db are shared operations, operations that operate on the DB object itself are exclusives
          mutable shared_spinlock lock;
          std::deque<cr::raw_ptr<base_t>> db;
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
            cr::out().debug("  {}: id: {}, size of {} bytes, aligned on {} bytes", debug_info[i].type_name, debug_info[i].id, allocator_info[i].size, allocator_info[i].alignment);
            allocator.init_for_type(allocator_info[i].id, allocator_info[i].size, allocator_info[i].alignment);
          }
        }

      public:
        ~database()
        {
          apply_component_db_changes();

          check::debug::n_assert(entity_data_pool.get_number_of_object() == 0, "There are entities that are still alive AFTER their database has been destructed. This will lead to crashes.");
        }

        /// \brief Create a new entity
        entity_t create_entity()
        {
          entity_data_t* data = entity_data_pool.allocate();
          new (data) entity_data_t(*this); // construct

          data->weak_ref_indirection = entity_t::weak_ref_indirection_t::create(data);

          entity_t ret(*data);
#if ENFIELD_ENABLE_DEBUG_CHECKS
          data->assert_valid();
#endif

          if constexpr (DatabaseConf::use_entity_db)
          {
            // for systems
            std::lock_guard _lg(spinlock_exclusive_adapter::adapt(entity_list_lock));
            data->index = entity_list.size();
            entity_list.push_back(data);
          }

          return ret;
        }

        size_t get_entity_count() const
        {
          static_assert(DatabaseConf::use_entity_db, "cannot call get_entity_count when entity-db is disabled");
          return entity_list.size();
        }

        template<typename AttachedObject>
        size_t get_attached_object_count() const
        {
          return get_attached_object_count(id_t<AttachedObject>::id());
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
          static_assert_can<DatabaseConf, AttachedObject::ao_class_id, attached_object_access::db_queryable>();

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
        void optimize(bool force = false)
        {
          TRACY_SCOPED_ZONE;
          if constexpr(DatabaseConf::use_entity_db)
          {
            if (entity_deletion_count.load(std::memory_order_acquire) > k_deletion_count_to_optimize || force)
            {
              std::lock_guard _lg(spinlock_exclusive_adapter::adapt(entity_list_lock));
              entity_deletion_count.store(0, std::memory_order_release);
              // we assume everything is already somewhat sorted, and we just need compaction
              uint32_t shift = 0;
              for (uint32_t i = 0; i < entity_list.size(); ++i)
              {
                if (entity_list[i] == nullptr)
                {
                  shift += 1;
                  continue;
                }
                if (shift != 0)
                {
                  entity_list[i - shift] = std::move(entity_list[i]);
                  entity_list[i - shift]->index = i - shift;
                }
              }
              entity_list.resize(entity_list.size() - shift);
              // cr::out().debug("db::optimize: entity size: {} (removed {} entries)", entity_list.size(), shift);

              if constexpr (0)
              {
                // old way
                std::sort(entity_list.begin(), entity_list.end());
                for (uint32_t i = 0; i < entity_list.size(); ++i)
                  entity_list[i]->index = i;
              }
            }
          }

          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db)
            {
              if (it.deletion_count.load(std::memory_order_acquire) < k_deletion_count_to_optimize || force)
                continue;
              std::lock_guard _lg(spinlock_exclusive_adapter::adapt(it.lock));
              it.deletion_count.store(0, std::memory_order_release);
              // we assume everything is already somewhat sorted, and we just need compaction
              uint32_t shift = 0;
              for (uint32_t i = 0; i < it.db.size(); ++i)
              {
                if (it.db[i] == nullptr)
                {
                  shift += 1;
                  continue;
                }
                if (shift != 0)
                {
                  it.db[i - shift] = std::move(it.db[i]);
                  it.db[i - shift]->index = i - shift;
                }
              }
              it.db.resize(it.db.size() - shift);
              // cr::out().debug("db::optimize: ao-db[{}]: size: {} (removed {} entries)", &it - attached_object_db, it.db.size(), shift);
              // std::sort(it.db.begin(), it.db.end());
              // for (uint32_t i = 0; i < it.db.size(); ++i)
              //   it.db[i]->index = i;
            }
          }
        }

        /// \brief Optimize the DB for cache coherency
        /// Calling this function every now and then will prevent the DB from slowing-down too much
        /// \warning VERY SLOW
        /// \note should be called after apply_component_db_changes
        threading::task_wrapper optimize(threading::task_manager& tm, threading::group_t group_id = threading::k_non_transient_task_group)
        {
          auto final_task = tm.get_task(group_id, []{});

          if constexpr(DatabaseConf::use_entity_db)
          {
            if (entity_deletion_count.load(std::memory_order_acquire) > k_deletion_count_to_optimize)
            {
              auto sort = tm.get_task(group_id, [this]
              {
                TRACY_SCOPED_ZONE;
                std::lock_guard _lg(spinlock_exclusive_adapter::adapt(entity_list_lock));
                entity_deletion_count.store(0, std::memory_order_release);
                // we assume everything is already somewhat sorted, and we just need compaction
                uint32_t shift = 0;
                for (uint32_t i = 0; i < entity_list.size(); ++i)
                {
                  if (entity_list[i] == nullptr)
                  {
                    shift += 1;
                    continue;
                  }
                  if (shift != 0)
                  {
                    entity_list[i - shift] = std::move(entity_list[i]);
                    entity_list[i - shift]->index = i - shift;
                  }
                }
                entity_list.resize(entity_list.size() - shift);
                // std::sort(entity_list.begin(), entity_list.end());
                // for (uint32_t i = 0; i < entity_list.size(); ++i)
                //   entity_list[i]->index = i;
              });
              final_task->add_dependency_to(*sort);
            }
          }

          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db)
            {
              if (it.db.empty())
                continue;
              if (it.deletion_count.load(std::memory_order_acquire) < k_deletion_count_to_optimize)
                continue;
              auto ao_sort = tm.get_task(group_id, [&it]
              {
                TRACY_SCOPED_ZONE;
                std::lock_guard _lg(spinlock_exclusive_adapter::adapt(it.lock));
                it.deletion_count.store(0, std::memory_order_release);
                // we assume everything is already somewhat sorted, and we just need compaction
                uint32_t shift = 0;
                for (uint32_t i = 0; i < it.db.size(); ++i)
                {
                  if (it.db[i] == nullptr)
                  {
                    shift += 1;
                    continue;
                  }
                  if (shift != 0)
                  {
                    it.db[i - shift] = std::move(it.db[i]);
                    it.db[i - shift]->index = i - shift;
                  }
                }
                it.db.resize(it.db.size() - shift);
                // std::sort(it.db.begin(), it.db.end());
                // for (uint32_t i = 0; i < it.db.size(); ++i)
                //   it.db[i]->index = i;
              });
              final_task->add_dependency_to(*ao_sort);
            }
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
              it.lock.lock_exclusive();
          }

          while (!pending_attached_object_changes.empty())
          {
            base_t* base = nullptr;
            if (!pending_attached_object_changes.try_pop_front(base))
              break;
            if constexpr (!DatabaseConf::use_attached_object_db)
            {
              auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
              allocator.deallocate(base->fully_transient_attached_object, base->object_type_id, allocator_info[base->object_type_id].size, allocator_info[base->object_type_id].alignment, base);
            }
            else
            {
              if (base->authorized_destruction)
              {
                if (!base->in_attached_object_db)
                {
                  ++skipped_count;
                  auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
                  allocator.deallocate(base->fully_transient_attached_object, base->object_type_id, allocator_info[base->object_type_id].size, allocator_info[base->object_type_id].alignment, base);
                }
                else
                {
                  ++removed_count;
                  remove_from_attached_db(*base);
                }
              }
              else
              {
                ++added_count;
                add_to_attached_db(*base);
              }
            }
          }

          // unlock all db:
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db)
              it.lock.unlock_exclusive();
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
          typename utility::shared_locker _sl{*this};
          std::lock_guard _l(_sl);

          // get the vector with the less attached objects
          const type_t attached_object_id = utility::get_min_entry_count(*this);

          // generates the mask
          const inline_mask<DatabaseConf> mask = utility::make_mask();

          // for each !
          if constexpr (DatabaseConf::use_attached_object_db)
          {
            for (auto& it : attached_object_db[attached_object_id].db)
            {
              if (it != nullptr && mask.match(it->owner.mask))
              {
                std::lock_guard _lg(spinlock_shared_adapter::adapt(it->owner.lock));
                utility::call(func, *this, it->owner);
              }
            }
          }
          else if constexpr(DatabaseConf::use_entity_db)
          {
            const uint32_t count = get_entity_count();
            for (uint32_t i = 0; i < count; ++i)
            {
              entity_data_t* data = get_entity(i);
              if (data != nullptr && mask.match(data->mask))
              {
                std::lock_guard _lg(spinlock_shared_adapter::adapt(data->lock));
                utility::call(func, *this, *data);
              }
            }
          }
        }

        template<typename AttachedObjectsList, typename Function>
        void for_each_list(const Function& func) const
        {
          using utility = typename ct::list::extract<AttachedObjectsList>::template as<attached_object_utility_t>;

          utility::check();
          typename utility::shared_locker _sl{*this};
          std::lock_guard _l(_sl);

          // get the vector with the less attached objects
          const type_t attached_object_id = utility::get_min_entry_count(*this);

          // generates the mask
          const inline_mask<DatabaseConf> mask = utility::make_mask();

          // for each !
          if constexpr(DatabaseConf::use_attached_object_db)
          {
            for (auto it : attached_object_db[attached_object_id])
            {
              if (it != nullptr && mask.match(it->owner.mask))
              {
                std::lock_guard _lg(spinlock_shared_adapter::adapt(it->owner.lock));
                utility::call(func, *this, it->owner);
              }
            }
          }
          else if constexpr (DatabaseConf::use_entity_db)
          {
            const uint32_t count = get_entity_count();
            for (uint32_t i = 0; i < count; ++i)
            {
              const entity_data_t* data = get_entity(i);
              if (data != nullptr && mask.match(data->mask))
              {
                std::lock_guard _lg(spinlock_shared_adapter::adapt(data->lock));
                utility::call(func, *this, *data);
              }
            }
          }
        }

      private:
        template<typename AttachedObject>
        bool entity_has(const entity_data_t& data) const
        {
          return data.template has<AttachedObject>();
        }

        template<typename AttachedObject>
        AttachedObject* entity_get(entity_data_t& data)
        {
          if (!data.template has<AttachedObject>())
            return nullptr;
          return data.template slow_get<AttachedObject>();
        }

        template<typename AttachedObject>
        const AttachedObject* entity_get(const entity_data_t& data) const
        {
          if (!data.template has<AttachedObject>())
            return nullptr;
          return data.template slow_get<AttachedObject>();
        }

        entity_data_t* get_entity(size_t index)
        {
          static_assert(DatabaseConf::use_entity_db, "cannot call get_entity when entity-db is disabled");

          return entity_list[index];
        }

        const entity_data_t* get_entity(size_t index) const
        {
          static_assert(DatabaseConf::use_entity_db, "cannot call get_entity when entity-db is disabled");

          return entity_list[index];
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
#if ENFIELD_ENABLE_DEBUG_CHECKS
          data.assert_valid();
#endif
          {
            if constexpr(DatabaseConf::allow_ref_counting_on_entities)
            {
              check::debug::n_assert(data.counter.load(std::memory_order_acquire) == 0, "Trying to remove an entity while there's still hard-refs on it");
            }
            std::lock_guard _lg{spinlock_exclusive_adapter::adapt(data.lock)};
            // loop over all "hard-added" attached objects in order to remove them
            // as after this pass every "soft-added" attached objects (views, requested, ...) should all be removed, unless cycles exist
            std::vector<base_t*> to_remove;
            for (auto& it : data.attached_objects)
            {
              if (it.second->externally_added)
                to_remove.push_back(it.second);
            }
            for (auto* it : to_remove)
            {
              it->externally_added = false;
              if (it->can_be_destructed())
                _delete_ao(*it, data);
            }

            // remove the entity from the list
            if constexpr(DatabaseConf::use_entity_db)
            {
              std::lock_guard _lg(spinlock_exclusive_adapter::adapt(entity_list_lock));
              check::debug::n_assert(entity_list[data.index].get() == &data, "Trying to remove and entity from a different DB");
              entity_list[data.index]._drop(); // simply assign the pointer to nullptr
            }
            entity_deletion_count.fetch_add(1, std::memory_order_release);

            // This error mostly tells you that you have dependency cycles in your attached objects.
            // You can put a breakpoint here and look at what is inside the attached_objects vector.
            //
            // This isn't toggled by ENFIELD_ENABLE_DEBUG_CHECKS because it's a breaking error. It won't produce any code in "super-release" builds
            // where n_assert will just expand to a dummy, but stil, if that error appears this means that some of your attached objects are wrongly created.
            check::debug::n_assert(data.attached_objects.empty(), "There's still attached objects on an entity while trying to destroy it (do you have dependency cycles ?)");
          }
          // free the memory
          data.~entity_data_t();
          entity_data_pool.deallocate(&data);
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject& _create_ao(entity_data_t& data,
                                   attached_object::creation_flags flags,
                                   DataProvider&& ... provider)
        {
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data.lock._debug_is_exclusive_lock_held_by_current_thread(), "database::_create_ao: expecting exclusive lock to be held by current thread");
#endif
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
                std::lock_guard _lg(spinlock_exclusive_adapter::adapt(attached_object_db[object_type_id].lock));
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

        void _delete_ao(base_t& base, entity_data_t& data)
        {
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data.lock._debug_is_exclusive_lock_held_by_current_thread(), "database::_delete_ao: expecting exclusive lock to be held by current thread");
#endif
          base.authorized_destruction = true;

          data.remove_attached_object(base.object_type_id);

          // Perform the deletion
          data.mask.unset(base.object_type_id);

          // destruct (always, to keep the nice C++ resource management pattern and avoid nasty surprises)
          // must be after the remove/unset
          base.~base_t();

          // only when supported by the configuration.
          // no compile-time checks here because that function (and some of its caller)
          // may does not know the type of the element to remove, so it cannot perform compile-time branching.
          if constexpr (DatabaseConf::use_attached_object_db)
          {
            if (!base.fully_transient_attached_object)
            {
              std::lock_guard _lg(spinlock_shared_adapter::adapt(attached_object_db[base.object_type_id].lock));
              remove_from_attached_db(base);
              //add_to_pending_change_db(base);
            }
            else
            {
              // we got a fully transient object, no need for anything else
              auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
              allocator.deallocate(true, base.object_type_id, allocator_info[base.object_type_id].size, allocator_info[base.object_type_id].alignment, &base);
            }
          }
          else
          {
              auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
            allocator.deallocate(base.fully_transient_attached_object, base.object_type_id, allocator_info[base.object_type_id].size, allocator_info[base.object_type_id].alignment, &base);
          }
        }

        void add_to_pending_change_db(base_t& base)
        {
          if constexpr(!DatabaseConf::use_attached_object_db)
          {
            static_assert(DatabaseConf::use_attached_object_db, "Cannot use the attached_object_db when use_attached_object_db is false");
            return;
          }

          if (base.authorized_destruction && !base.in_attached_object_db)
            return;

          pending_attached_object_changes.push_back(&base);
        }

        // NOTE: lock (exclusive) must be held
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

        // NOTE: lock (shared or exclusive) must be held
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
            check::debug::n_assert(attached_object_db[base.object_type_id].db[base.index].get() == &base, "Incoherent DB state");

            attached_object_db[base.object_type_id].deletion_count.fetch_add(1, std::memory_order_release);

            attached_object_db[base.object_type_id].db[base.index]._drop();
          }

          auto& allocator_info = type_registry<DatabaseConf>::allocator_info();
          allocator.deallocate(base.fully_transient_attached_object, base.object_type_id, allocator_info[base.object_type_id].size, allocator_info[base.object_type_id].alignment, &base);
        }

      private:
        /// \brief The database of components / concepts / *, sorted by type_t (attached_object_type)
        /// This is only used for queries.
        static constexpr uint32_t k_attached_object_db_size = DatabaseConf::use_attached_object_db ? DatabaseConf::max_attached_objects_types : 0;
        attached_object_db_t attached_object_db[k_attached_object_db_size];

        cr::queue_ts<cr::queue_ts_atomic_wrapper<base_t*>> pending_attached_object_changes;

        static constexpr uint32_t k_deletion_count_to_optimize = 1024;

        // deletion is the trigger point for re-arranging the array
        // entity_list usage is controlled by dbconf::use_entity_db
        std::atomic<uint32_t> entity_deletion_count;
        mutable shared_spinlock entity_list_lock;
        std::deque<cr::raw_ptr<entity_data_t>> entity_list;

        neam::cr::memory_pool<entity_data_t> entity_data_pool;

        typename DatabaseConf::attached_object_allocator allocator;

        friend class entity<DatabaseConf>;
        friend class entity_weak_ref<DatabaseConf>;
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



