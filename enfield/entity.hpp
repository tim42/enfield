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

#ifndef __N_3928149883043231716_951828120_ENTITY_HPP__
#define __N_3928149883043231716_951828120_ENTITY_HPP__

#include <cstdint>
#include <unordered_map>

#include "enfield_types.hpp"
#include "enfield_exception.hpp"

#include "type_id.hpp"
#include "internal_base_attached_object.hpp"
#include "database_conf.hpp"

namespace neam
{
  namespace enfield
  {
    template<typename DatabaseConf> class database;

    /// \brief An entity. The entity cannot be copied, only moved. If you want to hold more than one entity,
    /// please use the memory management scheme you like the most (except reference counting, because screw ref. counts)
    /// \tparam DatabaseConf The database configuration
    /// \note An entity is NEVER dynamically allocated.
    /// \note Attached objects (components, views, ...) should never use the API of the entity: you can't hold a pointer to it as it may be moved asynchronously.
    template<typename DatabaseConf>
    class entity
    {
      public:
        using database_t = database<DatabaseConf>;
      private:
        using base_t = attached_object::base<DatabaseConf>;
        /// \brief The data of the entity itself isn't held by the instance of the entity, but lives in the DB
        struct data_t
        {
          data_t (id_t _entity_id, database_t *_db) : entity_id(_entity_id), db(_db) {}
          data_t(data_t&&) = default;
          ~data_t() = default;

          data_t() = delete;
          data_t(const data_t&) = delete;
          data_t &operator = (const data_t&) = delete;
          data_t &operator = (data_t&&) = delete;

          /// \brief Local id of the entity (local = not networked)
          /// This identifier is used to identify the entity in the DB
          const id_t entity_id = ~0u;

          database_t * const db = nullptr;

          /// \brief Allow a quick query of component this entity has
          uint64_t component_types[DatabaseConf::max_component_types / (sizeof(uint64_t) * 8)] = {0};

          /// \brief The list of attached_objects this entity have
          std::unordered_map<type_t, base_t *> attached_objects = std::unordered_map<type_t, base_t *>();

          entity *owner = nullptr;

          /// \brief Check that everything is OK
          bool validate() const
          {
            if (attached_objects.size() > DatabaseConf::max_component_types)
              return false;
            if (!owner)
              return false;
            if (this != owner->data)
              return false;

            // loop over component_types
            for (type_t i = 0; i < DatabaseConf::max_component_types; ++i)
            {
              const uint32_t index = i / (sizeof(uint64_t) * 8);
              const uint64_t mask = 1ul << (i % (sizeof(uint64_t) * 8));
              if ((!!(component_types[index] & mask)) != (!!attached_objects.count(i)))
                return false;
            }

            return true;
          }

          /// \brief Throw an exception if the entity state is invalid
          void throw_validate() const
          {
            check::on_error::n_assert(attached_objects.size() < DatabaseConf::max_component_types, "Entity is in invalid state");
            check::on_error::n_assert(owner != nullptr, "Entity is in invalid state");
            check::on_error::n_assert(this == owner->data, "Entity is in invalid state");

            // loop over component_types
            for (type_t i = 0; i < DatabaseConf::max_component_types; ++i)
            {
              const uint32_t index = i / (sizeof(uint64_t) * 8);
              const uint64_t mask = 1ul << (i % (sizeof(uint64_t) * 8));

              check::on_error::n_assert((!!(component_types[index] & mask)) == (!!attached_objects.count(i)), "Entity is in invalid state");
            }
          }
        };

      private:
        explicit entity(database_t &_db, data_t &_data) noexcept : db(&_db), data(&_data)
        {
          data->owner = this;
        }

      public:
        entity() = delete;

        entity(entity &&o) : db(o.db), data(o.data)
        {
          o.db = nullptr;
          o.data = nullptr;
          data->owner = this;
        }

        entity &operator = (entity &&o)
        {
          if (this != &o)
          {
            db->remove_entity(data);
            db = o.db;
            data = o.data;
            data->owner = this;
            o.db = nullptr;
            o.data = nullptr;
          }
          return *this;
        }

        ~entity()
        {
          if (db && data)
            db->remove_entity(data);
        }

        /// \brief Swap two entities
        void swap(entity &o)
        {
          std::swap(o.db, db);
          std::swap(o.data, data);
          o.data->owner = &o;
          data->owner = this;
        }

        /// \brief Return the ID of the entity.
        /// That ID can only be set when creating a new entity
        id_t get_id() const
        {
          return data->entity_id;
        }

        /// \brief Remove add an attached object
        /// \note if an attached object of the same type has already been user-created, an exception is thrown
        /// \note you can't add views
        /// \see has()
        template<typename AttachedObject, typename... DataProvider>
        AttachedObject &add(DataProvider *...provider)
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_creatable>();

          if (has<AttachedObject>())
          {
            AttachedObject *ret = get<AttachedObject>();

            base_t *bptr = ret;
            check::on_error::n_assert(bptr->user_added == false, "The attached object is already present and has already been user-requested");
            bptr->user_added = true;

            return *ret;
          }

          return db->template add_ao_user<AttachedObject, DataProvider...>(data, provider...);
        }

        /// \brief Remove an attached object
        /// \note you can't remove views
        template<typename AttachedObject>
        void remove()
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_removable>();

          if (!has<AttachedObject>())
            return;
          db->template remove_ao_user(data, data->attached_objects[type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id]);
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        AttachedObject *get()
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          if (!has<AttachedObject>())
            return nullptr;
          return static_cast<AttachedObject*>(data->attached_objects[type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id]);
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        const AttachedObject *get() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          if (!has<AttachedObject>())
            return nullptr;
          return static_cast<const AttachedObject*>(data->attached_objects[type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id]);
        }

        /// \brief Return true if the entity has an attached object of that type
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        bool has() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
          const uint32_t index = id / (sizeof(uint64_t) * 8);
          const uint64_t mask = 1ul << (id % (sizeof(uint64_t) * 8));
          return (data->component_types[index] & mask) != 0;
        }

        /// \brief Return the current database of the entity
        database_t &get_database() { return *db; }
        /// \brief Return the current database of the entity
        const database_t &get_database() const { return *db; }

        /// \brief Check that the entity is in a valid state
        /// \note If that's not the case, it will throw something
        void validate() const
        {
          data->throw_validate();
        }

      private:
        database_t *db = nullptr;
        data_t *data = nullptr;

        friend class database<DatabaseConf>;
        friend class attached_object::base<DatabaseConf>;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_3928149883043231716_951828120_ENTITY_HPP__

