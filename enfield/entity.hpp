//
// file : entity.hpp
// in : file:///home/tim/projects/enfield/enfield/entity.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 14:18:10 GMT-0500 (EST)
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

#include <cstdint>
#include <unordered_map>

#include <ntools/memory_pool.hpp>
#include <ntools/raw_ptr.hpp>
#include <ntools/spinlock.hpp>

#include "enfield_types.hpp"

#include "type_id.hpp"
#include "attached_object/internal_base_attached_object.hpp"
#include "database_conf.hpp"

namespace neam
{
  namespace enfield
  {
    template<typename DatabaseConf> class entity_weak_ref;

    /// \brief An entity. The entity cannot be copied, only moved.
    /// \tparam DatabaseConf The database configuration
    /// \note Attached objects (components, views, ...) should never use the API of the entity:
    ///       you can't hold a pointer to it as it may be moved around.
    /// \warning Entity (and all operations associated with them) are inherently NOT thread safe
    ///       Only a single thread should have ownership and perform write/modify operations on an entity
    ///       If you know that an entity is to be modified by multiple threads,
    ///       Handle it like any other non-thread-safe objects and use an external lock.
    ///       Operations on multiple, independent entities are thread-safe
    /// \note Why entities are not thread safe:
    ///       If one thread perform a get() (or add()) and another perform a remove(), depending on the order of those two operations
    ///       multiple outcomes can happen, including one where the pointer returned by get() is invalid
    ///       (as remove was executed right after the get, and before the returned value was used)
    ///       There are workarounds this situation, where the return value is not a pointer, but a pointer + lock-ownership
    ///       But this increase possibility of deadlocks (that object is transferred to a task), and makes everything more complex.
    ///       So instead, enfield requires to either:
    ///        - architecture around this limitation and prevent use cases where an entity has concurrent writes or read+writes
    ///        - add external locks
    template<typename DatabaseConf>
    class entity
    {
      public:
        using database_t = database<DatabaseConf>;

      private:
        using base_t = attached_object::base<DatabaseConf>;
        struct data_t;

        struct weak_ref_indirection_t
        {
          data_t* data = nullptr;
          std::atomic<uint32_t> weak_ref_counter = 0;

          [[nodiscard]] static cr::raw_ptr<weak_ref_indirection_t> create(data_t* data)
          {
            auto* ret = cr::global_object_pool<weak_ref_indirection_t>::get_pool().allocate();
            ret->data = data;
            ret->weak_ref_counter.store(1, std::memory_order_release);
            return ret;
          }

          void grab()
          {
            const uint32_t counter = weak_ref_counter.fetch_add(1, std::memory_order_acq_rel);
            check::debug::n_assert(counter > 0, "Entity weak-ref-count is 0"); // that shouldn't be possible
          }

          void drop()
          {
            const uint32_t counter = weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel);
            check::debug::n_assert(counter > 0, "Entity weak-ref-count is lower than 0");
            if (counter <= 1)
            {
              cr::global_object_pool<weak_ref_indirection_t>::get_pool().deallocate(this);
            }
          }
        };

        /// \brief The data of the entity itself isn't held by the instance of the entity, but lives in the DB
        struct data_t
        {
          data_t (database_t& _db) : db(_db) {}
          data_t(data_t&&) = default;
          ~data_t() = default;

          data_t() = delete;
          data_t(const data_t&) = delete;
          data_t& operator = (const data_t&) = delete;
          data_t& operator = (data_t&&) = delete;

          cr::raw_ptr<weak_ref_indirection_t> weak_ref_indirection;

          uint64_t index = 0;

          database_t& db;

          /// \brief Allow a quick query of the components this entity has
          inline_mask<DatabaseConf> mask;

          /// \brief The list of attached_objects this entity have
          /// (we use a linear array as we don't expect that there will be more than 100 components on most entities)
          std::mtc_vector<std::pair<type_t, base_t*>> attached_objects;

          /// \brief Strong refs for the entity
          std::atomic<uint32_t> counter = 0;
          std::atomic<bool> in_destructor = false;

          mutable shared_spinlock lock;

          /// \brief Check that everything is OK
          bool validate() const
          {
            if (attached_objects.size() > DatabaseConf::max_attached_objects_types)
              return false;

            inline_mask<DatabaseConf> actual_mask;
            for (const auto& it : attached_objects)
              actual_mask.set(it.first);

            if (!(mask == actual_mask))
              return false;

            return true;
          }

          /// \brief assert if the entity state is invalid
          void assert_valid() const
          {
            check::debug::n_assert(validate(), "Entity is in invalid state");
          }

          /// \brief Return true if the entity has an attached object of that type
          bool has(const type_t id) const
          {
            return mask.is_set(id);
          }

          /// \brief Return true if the entity has an attached object of that type
          template<typename AttachedObject>
          bool has() const
          {
            const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id();
            return has(id);
          }

          const base_t* slow_get(const type_t id) const
          {
            for (const auto& it : attached_objects)
            {
              if (it.first == id)
              {
                [[likely]] if (it.second != (base_t*)(k_poisoned_pointer))
                  return it.second;
                return nullptr;
              }
            }
            return nullptr;
          }
          base_t* slow_get(const type_t id)
          {
            return const_cast<base_t*>(const_cast<const data_t*>(this)->slow_get(id));
          }

          template<typename AttachedObject>
          AttachedObject* slow_get()
          {
            return static_cast<AttachedObject*>(slow_get(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id()));
          }
          template<typename AttachedObject>
          const AttachedObject* slow_get() const
          {
            return static_cast<const AttachedObject*>(slow_get(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id()));
          }

          /// \brief Remove an attached object from the entity
          void remove_attached_object(type_t id)
          {
#if N_ENABLE_LOCK_DEBUG
            check::debug::n_assert(lock._debug_is_exclusive_lock_held_by_current_thread(), "entity_data_t::remove_attached_object: expecting exclusive lock to be held by current thread");
#endif
            if (!has(id))
              return;
            for (uint32_t i = 0; i < attached_objects.size(); ++i)
            {
              if (attached_objects[i].first == id)
              {
                std::swap(attached_objects.back(), attached_objects[i]);
                attached_objects.pop_back();
                return;
              }
            }
            return;
          }
        };

      private:
        explicit entity(data_t& _data) : entity(cr::raw_ptr<data_t>{&_data}) {}
        explicit entity(cr::raw_ptr<data_t>&& _data) : data(std::move(_data))
        {
          if constexpr(DatabaseConf::allow_ref_counting_on_entities)
            data->counter.fetch_add(1, std::memory_order_release);
        }
      public:
        entity() = default;
        entity(entity&& o) = default;

        entity& operator = (entity&& o)
        {
          if (this == &o) return *this;
          release();
          data = std::move(o.data);
          return *this;
        }

        ~entity()
        {
          release();
        }

        /// \brief Use this for externally locking the entity.
        /// \note No operation on entities are interacting with this lock, but they expect the lock to be held in different ways
        shared_spinlock& get_lock() const
        {
          check::debug::n_assert(is_valid(), "entity::get_lock: entity is not valid");
          return data->lock;
        }

        void release()
        {
          if (data != nullptr)
          {
            if constexpr(!DatabaseConf::allow_ref_counting_on_entities)
            {
              data->in_destructor.store(true, std::memory_order_release);
              data->weak_ref_indirection->data = nullptr;
              data->weak_ref_indirection.release()->drop();
              data->db.remove_entity(*data.release());
            }
            else
            {
              uint32_t counter = data->counter.fetch_sub(1, std::memory_order_acq_rel);
              check::debug::n_assert(counter > 0, "Entity ref-count is lower than 0");
              if (counter <= 1)
              {
                data->in_destructor.store(true, std::memory_order_release);
                data->weak_ref_indirection->data = nullptr;
                data->weak_ref_indirection.release()->drop();

                // check that no-one acquired a strong-ref on the entity while we were planning to destroy it:
                counter = data->counter.load(std::memory_order_acquire);
                if (counter == 0)
                  data->db.remove_entity(*data.release());
                else
                  data._drop();
              }
              else
              {
                data._drop();
              }
            }
          }
        }

        [[nodiscard]] entity duplicate_tracking_reference()
        {
          static_assert(DatabaseConf::allow_ref_counting_on_entities, "duplicate_tracking_reference can only be called when entity ref-counting is enabled");
          return { data, &data->db };
        }

        [[nodiscard]] entity_weak_ref<DatabaseConf> weak_reference()
        {
          check::debug::n_assert(is_valid(), "entity::weak_reference: entity is not valid");
          return entity_weak_ref<DatabaseConf> { data->weak_ref_indirection.get() };
        }

        /// \brief Swap two entities
        void swap(entity& o)
        {
          std::swap(o.data, data);
        }

        /// \brief Add an attached object
        /// \note if an attached object of the same type has already been ext-created it will assert
        /// \note you can't add views
        /// \see has()
        template
        <
          typename AttachedObject,
          attached_object::creation_flags Flags = attached_object::creation_flags::none,
          typename... DataProvider
        >
        AttachedObject& add(DataProvider&& ... provider)
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_creatable>();
          check::debug::n_assert(is_valid(), "entity::add: entity is not valid");
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data->lock._debug_is_exclusive_lock_held_by_current_thread(), "entity::add: expecting exclusive lock to be held by current thread");
#endif

          AttachedObject* ret;
          if (data->template has<AttachedObject>())
            ret = data->template slow_get<AttachedObject>();
          else
            ret = &data->db.template _create_ao<AttachedObject, DataProvider...>(*data, Flags, std::forward<DataProvider>(provider)...);
          check::debug::n_assert(ret != nullptr, "The attached object is invalid (dependency cycle?)");
          base_t* bptr = ret;
          check::debug::n_assert(bptr->externally_added == false, "The attached object is already present and has already been externally-requested");
          bptr->externally_added = true;
          return *ret;
        }

        /// \brief Remove an attached object
        /// \note you can't remove views
        template<typename AttachedObject>
        void remove()
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_removable>();
          check::debug::n_assert(is_valid(), "entity::remove: entity is not valid");
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data->lock._debug_is_exclusive_lock_held_by_current_thread(), "entity::remove: expecting exclusive lock to be held by current thread");
#endif

          if (!data->template has<AttachedObject>())
            return;
          AttachedObject& obj = *data->template slow_get<AttachedObject>();
          base_t* bptr = &obj;
          check::debug::n_assert(bptr->externally_added == true, "The attached object is has not been externally-requested");
          bptr->externally_added = false;

          if (bptr->can_be_destructed())
            data->db._delete_ao(*bptr, *data);
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        [[nodiscard]] AttachedObject* get()
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();
          check::debug::n_assert(is_valid(), "entity::get: entity is not valid");
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data->lock._get_shared_state() || data->lock._get_exclusive_state(), "entity::get: expecting shared lock to be held by current thread");
#endif

          if (!data->template has<AttachedObject>())
            return nullptr;
          return static_cast<AttachedObject*>(data->template slow_get<AttachedObject>());
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        [[nodiscard]] const AttachedObject* get() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();
          check::debug::n_assert(is_valid(), "entity::get: entity is not valid");
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data->lock._get_shared_state() || data->lock._get_exclusive_state(), "entity::get: expecting shared lock to be held by current thread");
#endif

          return static_cast<AttachedObject*>(data->template slow_get<AttachedObject>());
        }

        /// \brief Return true if the entity has an attached object of that type
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        [[nodiscard]] bool has() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();
          check::debug::n_assert(is_valid(), "entity::has: entity is not valid");
#if N_ENABLE_LOCK_DEBUG
          check::debug::n_assert(data->lock._get_shared_state() || data->lock._get_exclusive_state(), "entity::has: expecting shared lock to be held by current thread");
#endif

          return data->template has<AttachedObject>();
        }

        /// \brief Return the current database of the entity
        database_t& get_database()
        {
          check::debug::n_assert(is_valid(), "entity::get_database: entity is not valid");
          return data->db;
        }
        /// \brief Return the current database of the entity
        const database_t& get_database() const
        {
          check::debug::n_assert(is_valid(), "entity::get_database: entity is not valid");
          return data->db;
        }

        /// \brief Check that the entity is in a valid state
        void validate() const
        {
          check::debug::n_assert(is_valid(), "entity::validate: entity is not valid");
          data->assert_valid();
        }

        /// \brief Return whether this entity contains a valid data
        /// \note if false, most operations will assert
        [[nodiscard]] bool is_valid() const { return data != nullptr && !data->in_destructor.load(std::memory_order_acquire); }

        bool is_tracking_same_entity(const entity& o) const
        {
          return data.get() == o.data.get();
        }

        bool is_tracking_same_entity(const entity_weak_ref<DatabaseConf>& o) const
        {
          if (!data && (!o.indirection || o.indirection->data == nullptr))
            return true;
          if (!o.indirection || !data)
            return false;
          return data.get() == o.indirection->data;
        }

      private:
        cr::raw_ptr<data_t> data;

        friend class database<DatabaseConf>;
        friend class attached_object::base<DatabaseConf>;
        template<typename DBC, typename SystemClass> friend class system;
        friend class base_system<DatabaseConf>;
        friend class system_manager<DatabaseConf>;
        template<typename DBC, typename... AttachedObjects> friend struct attached_object_utility;
        friend entity_weak_ref<DatabaseConf>;
    };

    /// \brief Weak ref for entities
    /// \warning As entities are not thread safe, this class isn't either
    template<typename DatabaseConf>
    class entity_weak_ref
    {
      private:
        using entity_t = entity<DatabaseConf>;
        using weak_ref_indirection_t = typename entity_t::weak_ref_indirection_t;
        using database_t = database<DatabaseConf>;

      public:
        entity_weak_ref() = default;
        entity_weak_ref(entity_weak_ref&& o) = default;
        entity_weak_ref& operator = (entity_weak_ref&& o)
        {
          if (&o == this) return *this;
          release();
          indirection = std::move(o.indirection);
          return *this;
        }

        ~entity_weak_ref()
        {
          release();
        }

        [[nodiscard]] bool is_valid() const
        {
          if (!indirection) return false;
          if (!indirection->data)
          {
            indirection.release()->drop();
            return false;
          }
          return true;
        }

        void release()
        {
          if (is_valid())
            indirection.release()->drop();
        }

        /// \brief atomically generate a strong reference for the entity
        /// \warning always check if the resulting object is valid
        [[nodiscard]] entity_t generate_strong_reference()
        {
          static_assert(DatabaseConf::allow_ref_counting_on_entities, "generate_strong_reference can only be called when entity ref-counting is enabled");
          if (!is_valid())
            return {};
          // Might be invalid, but always check entity_t::is_valid()
          return entity_t{ indirection->data };
        }

        entity_weak_ref duplicate_tracking_reference() const
        {
          if (!is_valid())
            return {};
          return entity_weak_ref{ indirection.get() };
        }

        bool is_tracking_same_entity(const entity_t& o) const
        {
          return o.is_tracking_same_entity(*this);
        }

        bool is_tracking_same_entity(const entity_weak_ref& o) const
        {
          return indirection.get() == o.indirection.get();
        }

      public: // unsafe entity API

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        /// \warning NOT SAFE. Only use this function if you have the guarantee that the entity will not be destroyed during the call
        template<typename AttachedObject>
        [[nodiscard]] AttachedObject* get()
        {
          check::debug::n_assert(is_valid(), "entity-weak-ref::get: weak-ref is not valid");
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();

          if (!indirection->data->template has<AttachedObject>())
            return nullptr;

          AttachedObject* ret = static_cast<AttachedObject*>(indirection->data->template slow_get<AttachedObject>());
          check::debug::n_assert(is_valid(), "entity-weak-ref::get: weak-ref has become invalid during operation (TOCTOU)");
          return ret;
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        /// \warning NOT SAFE. Only use this function if you have the guarantee that the entity will not be destroyed during the call
        template<typename AttachedObject>
        [[nodiscard]] const AttachedObject* get() const
        {
          check::debug::n_assert(is_valid(), "entity-weak-ref::get: weak-ref is not valid");
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();

          if (!indirection->data->template has<AttachedObject>())
            return nullptr;

          const AttachedObject* ret = static_cast<AttachedObject*>(indirection->data->template slow_get<AttachedObject>());
          check::debug::n_assert(is_valid(), "entity-weak-ref::get: weak-ref has become invalid during operation (TOCTOU)");
          return ret;
        }

        /// \brief Return true if the entity has an attached object of that type
        /// \note has() is way faster than a get() when the component/attached object is present
        /// \warning NOT SAFE. Only use this function if you have the guarantee that the entity will not be destroyed during the call
        template<typename AttachedObject>
        [[nodiscard]] bool has() const
        {
          check::debug::n_assert(is_valid(), "entity-weak-ref::has: weak-ref is not valid");
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::ext_getable>();

          const bool ret = indirection->data->template has<AttachedObject>();
          check::debug::n_assert(is_valid(), "entity-weak-ref::has: weak-ref has become invalid during operation (TOCTOU)");
          return ret;
        }

      private:
        explicit entity_weak_ref(cr::raw_ptr<weak_ref_indirection_t>&& _data)
          : indirection(std::move(_data))
        {
          indirection->grab();
        }
        mutable cr::raw_ptr<weak_ref_indirection_t> indirection;

        friend entity<DatabaseConf>;
        friend attached_object::base<DatabaseConf>;
    };
  } // namespace enfield
} // namespace neam

