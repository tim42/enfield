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

#include "type_id.hpp"
#include "attached_object/internal_base_attached_object.hpp"
#include "database_conf.hpp"

namespace neam
{
  namespace enfield
  {
    /// \brief An entity. The entity cannot be copied, only moved. If you want to hold more than one entity,
    /// please use the memory management scheme you like the most (except reference counting, because screw ref. counts)
    /// \tparam DatabaseConf The database configuration
    /// \note Attached objects (components, views, ...) should never use the API of the entity:
    ///       you can't hold a pointer to it as it may be moved asynchronously.
    /// \warning Entity (and all operations associated with them) are inherently NOT thread safe
    ///          Only a single thread should have ownership and perform write/modify operations on an entity
    ///          If you know that an entity is to be modified by multiple threads,
    ///          Handle it like any other non-thread-safe objects and use an external lock.
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
          data_t (database_t& _db) : db(_db) {}
          data_t(data_t&&) = default;
          ~data_t() = default;

          data_t() = delete;
          data_t(const data_t&) = delete;
          data_t& operator = (const data_t&) = delete;
          data_t& operator = (data_t&&) = delete;

          uint64_t index = 0;

          database_t& db;

          /// \brief Allow a quick query of component this entity has
          inline_mask<DatabaseConf> mask;

          /// \brief The list of attached_objects this entity have
          /// (we use a linear array as we don't expect that there will be more than 100 components on most entities)
          std::vector<std::pair<type_t, base_t*>> attached_objects;

          /// \brief Check that everything is OK
          bool validate() const
          {
            if (attached_objects.size() > DatabaseConf::max_component_types)
              return false;

            inline_mask<DatabaseConf> actual_mask;
            for (const auto& it : attached_objects)
              actual_mask.set(it.first);

            if (!(mask == actual_mask))
              return false;

            return true;
          }

          /// \brief Throw an exception if the entity state is invalid
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

          const base_t* get(const type_t id) const
          {
            if (!has(id))
              return nullptr;
            for (const auto& it : attached_objects)
            {
              if (it.first == id)
              {
                if (it.second != (base_t*)(k_poisoned_pointer))
                  return it.second;
                return nullptr;
              }
            }
            return nullptr;
          }
          base_t* get(const type_t id)
          {
            return const_cast<base_t*>(const_cast<const data_t*>(this)->get(id));
          }

          template<typename AttachedObject>
          AttachedObject* get()
          {
            return static_cast<AttachedObject*>(get(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id()));
          }
          template<typename AttachedObject>
          const AttachedObject* get() const
          {
            return static_cast<const AttachedObject*>(get(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id()));
          }

          /// \brief Remove an attached object from the entity
          void remove_attached_object(type_t id)
          {
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
        explicit entity(data_t& _data) : data(&_data)
        {
        }

      public:
        entity() = delete;

        entity(entity&& o) : data(o.data)
        {
          o.data = nullptr;
        }

        entity& operator = (entity&& o)
        {
          if (this != &o)
          {
            data = o.data;
            o.data = nullptr;
          }
          return *this;
        }

        ~entity()
        {
          if (data != nullptr)
            data->db.remove_entity(*data);
        }

        /// \brief Swap two entities
        void swap(entity& o)
        {
          std::swap(o.data, data);
          o.data->owner = &o;
          data->owner = this;
        }

        /// \brief Add an attached object
        /// \note if an attached object of the same type has already been user-created it will assert
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
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_creatable>();

          if (has<AttachedObject>())
          {
            AttachedObject* ret = get<AttachedObject>();

            base_t* bptr = ret;
            check::debug::n_assert(bptr->user_added == false, "The attached object is already present and has already been user-requested");
            bptr->user_added = true;

            return *ret;
          }

          return data->db.template add_ao_user<AttachedObject, DataProvider...>(*data, Flags, std::forward<DataProvider>(provider)...);
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
          data->db.remove_ao_user(*data, *data->template get<AttachedObject>());
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        AttachedObject* get()
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          return static_cast<AttachedObject*>(data->template get<AttachedObject>());
        }

        /// \brief Return an attached object.
        /// If that attached object is not present, it returns nullptr
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        const AttachedObject* get() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          return static_cast<AttachedObject*>(data->template get<AttachedObject>());
        }

        /// \brief Return true if the entity has an attached object of that type
        /// \note has() is way faster than a get() when the component/attached object is present
        template<typename AttachedObject>
        bool has() const
        {
          static_assert_check_attached_object<DatabaseConf, AttachedObject>();
          static_assert_can<DatabaseConf, AttachedObject, attached_object_access::user_getable>();

          return data->template has<AttachedObject>();
        }

        /// \brief Return the current database of the entity
        database_t& get_database() { return data->db; }
        /// \brief Return the current database of the entity
        const database_t& get_database() const { return data->db; }

        /// \brief Check that the entity is in a valid state
        void validate() const
        {
          data->assert_valid();
        }

      private:
        data_t* data = nullptr;

        friend class database<DatabaseConf>;
        friend class attached_object::base<DatabaseConf>;
        template<typename DBC, typename SystemClass> friend class system;
        friend class base_system<DatabaseConf>;
        friend class system_manager<DatabaseConf>;
        template<typename DBC, typename... AttachedObjects> friend struct attached_object_utility;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_3928149883043231716_951828120_ENTITY_HPP__

