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

#include "enfield_types.hpp"
#include "database_conf.hpp"
#include "tools/memory_pool.hpp"
#include "tools/execute_pack.hpp"

namespace neam
{
  namespace enfield
  {
    namespace attached_object
    {
      template<typename DBC, typename AttachedObjectClass> class base_tpl;
    } // namespace attached_object

    static constexpr long poisoned_pointer = (~static_cast<long>(0xDEADBEEF));

    /// \brief The default DB configuration.
    /// A DB configuration allow a fine grained configuration over what is permitted:
    ///   - what kind of "attached objects" classes an entity can have (components, concepts, ...)
    ///   - what are the specifc access rights of a given attached object class
    struct default_database_conf
    {
      // type markers (mandatory)
      struct attached_object_class;
      struct attached_object_type;

      // allowed attached object classes (the constexpr type_t id is mandatory):
      struct component_class { static constexpr type_t id = 0; };
      struct concept_class { static constexpr type_t id = 1; };

      // "rights" configuration:
      template<type_t ClassId>
      struct class_rights
      {
        // default configuration: (must be static constexpr)

        /// \brief Define general access rights
        static constexpr attached_object_access access = attached_object_access::all;
      };

      /// \brief Specify the rights of OtherClassId over ClassId.
      /// In this mode, only attached_object_access::ao_* are accounted
      /// he default is to use the class_rights.
      template<type_t ClassId, type_t OtherClassId>
      struct specific_class_rights : public class_rights<ClassId> {};

      /// \brief The maximum number of components
      static constexpr uint64_t max_component_types = 2 * 64;
    };
    template<>
    struct default_database_conf::class_rights<default_database_conf::concept_class::id>
    {
      // specific configuration: (must be static constexpr)

      /// \brief Define general access rights
      static constexpr attached_object_access access = attached_object_access::ao_requireable | attached_object_access::ao_unsafe_getable | attached_object_access::user_getable;
    };

    /// \brief Where components are stored
    /// \tparam DatabaseConf The database configuration (default_database_conf should be more than correct for most usages)
    template<typename DatabaseConf>
    class database
    {
      private: // check the validity of the compile-time conf
        static_assert(DatabaseConf::max_component_types % (sizeof(uint64_t) * 8) == 0, "database's Conf::max_component_types property must be a multiple of uint64_t");

      public:
        using conf_t = DatabaseConf;
        using entity_t = entity<DatabaseConf>;
        using base_t = attached_object::base<DatabaseConf>;

        /// \brief A query in the DB
        class query_t
        {
          public:
            /// \brief The result of the query
            std::vector<entity_t *> result;

            /// \brief Filter the result of the DB
            template<typename... AttachedObject>
            query_t &filter(bool test_is_and = true)
            {
              NEAM_EXECUTE_PACK(static_assert_can<DatabaseConf, AttachedObject>(attached_object_access::user_getable));
              return filter({type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id...}, test_is_and);
            }

          private:
            /// \brief Filter the result of the DB
            query_t &filter(const std::vector<type_t> &attached_object_id, bool test_is_and = true)
            {
              std::vector<entity_t *> next_result;
              next_result.reserve(result.size());

              for (const auto &entity_it : result)
              {
                bool ok = test_is_and;
                for (const auto &vct_it : attached_object_id)
                {
                  const bool res = entity_it->data->attached_objects.count(vct_it) > 0;
                  ok = test_is_and ? (ok && res) : (ok || res);

                  if ((test_is_and && !ok) || (!test_is_and && ok))
                    break;
                }

                if (ok)
                  next_result.push_back(entity_it);
              }

              result.swap(next_result);
              return *this;
            }
        };

      public:
        /// \brief Create a new entity
        entity_t create_entity(id_t id = ~0u)
        {
          entity_data_t *data = entity_data_pool.allocate();
          new (data) entity_data_t(id, this); // call the constructor

          entity_t ret(*this, *data);
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data->throw_validate();
#endif
          return ret;
        }

        /// \brief Iterate over each attached object of a given type
        /// \tparam UnaryFunction a function-like object that takes one argument that is the entity
        template<typename AttachedObject, typename UnaryFunction>
        void for_each(const UnaryFunction &func)
        {
          static_assert_can<DatabaseConf, AttachedObject>(attached_object_access::user_getable);

          for_each(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id, func);
        }

      private:
        /// \brief Iterate over each attached object of a given type
        /// \tparam UnaryFunction a function-like object that takes one argument that is a reference to the entity
        /// \note If your function performs components removal / ... then you may not iterate over each entity and you shoud use a query instead
        template<typename UnaryFunction>
        void for_each(type_t attached_object, const UnaryFunction &func)
        {
          if (attached_object > DatabaseConf::max_component_types)
            return;

          for (uint32_t i = 0; i < attached_object_db[attached_object].size(); ++i)
            func(attached_object_db[attached_object][i]);
        }

      public:
        /// \brief Perform a query in the DB
        template<typename AttachedObject>
        query_t query()
        {
          static_assert_can<DatabaseConf, AttachedObject>(attached_object_access::user_getable);

          return query(type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id);
        }

      private:
        /// \brief Perform a query in the DB
        query_t query(type_t attached_object)
        {
          if (attached_object > DatabaseConf::max_component_types)
            return query_t();
          return query_t {attached_object_db[attached_object]};
        }

      private:
        using entity_data_t = typename entity_t::data_t;

        void remove_entity(entity_data_t *data)
        {
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data->throw_validate();
#endif
          // loop over all "hard-added" attached objects in order to remove them
          // as after this pass every "soft-added" attached objects (views, requested, ...) will be removed automatically
          std::vector<base_t *> to_remove;
          for (auto &it : data->attached_objects)
          {
            if (it.second->required_by.empty())
              to_remove.push_back(it.second);
          }
          for (auto &it : to_remove)
            remove_ao_user(data, it);

          // This error mostly tells you that you have dependency cycles in your attached objects.
          // You can put a breakpoint here and look at what is inside the attached_objects vector.
          //
          // This isn't toggled by ENFIELD_ENABLE_DEBUG_CHECKS because it's a breaking error. It won't produce any code in "super-release" builds
          // where n_assert will just expand to a dummy, but stil, if that error appears this means that some of your attached objects are wrongly created.
          check::on_error::n_assert(data->attached_objects.empty(), "There's still attached objects on an entity while trying to destroy it (do you have dependency cycles ?)");

          data->owner = nullptr; // unset the owner
          data->attached_objects.clear(); // just in case
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject &add_ao_user(entity_data_t *data, DataProvider *...provider)
        {
          AttachedObject &ret = _create_ao<AttachedObject>(data, provider...);
          base_t *bptr = &ret;
          check::on_error::n_assert(bptr != (AttachedObject *)(poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
          bptr->user_added = true;
          return ret;
        }

        template<typename AttachedObject>
        AttachedObject &add_ao_dep(entity_data_t *data, base_t *requester)
        {
          return _add_ao_dep<AttachedObject>(data, requester);
        }
        template<typename AttachedObject, typename DataProvider>
        AttachedObject &add_ao_dep(entity_data_t *data, base_t *requester, DataProvider *provider)
        {
          return _add_ao_dep<AttachedObject>(data, requester, provider);
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject &_add_ao_dep(entity_data_t *data, base_t *requester, DataProvider *...provider)
        {
          const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
          const uint32_t index = id / (sizeof(uint64_t) * 8);
          const uint64_t mask = 1ul << (id % (sizeof(uint64_t) * 8));

          // it already exists !
          if ((data->component_types[index] & mask) != 0)
          {
            base_t *bptr = data->attached_objects[id];
            check::on_error::n_assert(bptr != (base_t *)(poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
            bptr->required_by.insert(requester);
            requester->requires.insert(bptr);
            AttachedObject &ret = *static_cast<AttachedObject*>(bptr);
            return ret;
          }

          // create it
          AttachedObject &ret = _create_ao<AttachedObject>(data, provider...);
          base_t *bptr = &ret;
          bptr->required_by.insert(requester);
          requester->requires.insert(bptr);
          return ret;
        }

        template<typename AttachedObject, typename... DataProvider>
        AttachedObject &_create_ao(entity_data_t *data, DataProvider *...provider)
        {
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data->throw_validate();
#endif
          const type_t object_type_id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
          const uint32_t index = object_type_id / (sizeof(uint64_t) * 8);
          const uint64_t mask = 1ul << (object_type_id % (sizeof(uint64_t) * 8));
          data->component_types[index] |= mask;

          // make the get/add<AttachedObject>() segfault
          // (this helps avoiding incorrect usage of partially constructed attached objects)
          data->attached_objects[object_type_id] = (base_t *)(poisoned_pointer);

          // TODO(tim): replace this with a memory pool
          AttachedObject *ptr = new AttachedObject(&data->owner, provider...);

          // undo the segfault thing (the object has been fully constructed)
          data->attached_objects[object_type_id] = ptr;
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          data->throw_validate();
#endif
          return *ptr;
        }

        /// \note This remove path is the user-requested path
        void remove_ao_user(entity_data_t *data, base_t *base)
        {
#ifdef ENFIELD_ENABLE_DEBUG_CHECKS
          check::on_error::n_assert(base->user_added, "Invalid usage of remove_ao_user(): attached object is not user-added");
#endif
          base->user_added = false;
          if (base->required_by.size())
            return;

          _delete_ao(base, data);
        }

        /// \note This remove path is the dependency path
        void remove_ao_dep(base_t *base, entity_data_t *data, base_t *requester)
        {
          const size_t count = base->required_by.erase(requester);
          (void)count; // avoid a warning in super-release
          check::on_error::n_assert(count != 0, "remove_ao_dep() is wrongly used (requester not in the required_by list)");

          if (base->required_by.size())
            return;
          if (base->user_added)
            return;

          _delete_ao(base, data);
        }

        void _delete_ao(base_t *base, entity_data_t *data)
        {
          base->authorized_destruction = true;

          for (auto &it : base->requires)
          {
            check::on_error::n_assert(!it->authorized_destruction, "Dependency cycle detected when trying to remove an attached object");
            if (!it->authorized_destruction)
              remove_ao_dep(it, data, base);
          }
          base->requires.clear();

          // Perform the deletion
          const uint32_t index = base->object_type_id / (sizeof(uint64_t) * 8);
          const uint64_t mask = 1ul << (base->object_type_id % (sizeof(uint64_t) * 8));
          data->component_types[index] &= ~mask;
          data->attached_objects.erase(base->object_type_id);

          // TODO(tim): replace this with a memory pool
          delete base;
        }

      private:
        /// \brief The database of components / views / *, sorted by type_t (attached_object_type)
        std::vector<base_t *> attached_object_db[DatabaseConf::max_component_types];

        neam::cr::memory_pool<entity_data_t> entity_data_pool;

        friend class entity<DatabaseConf>;
        friend class attached_object::base<DatabaseConf>;
        template<typename DBC, typename AttachedObjectClass>
        friend class attached_object::base_tpl;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_3158130790807728943_2877410136_DATABASE_HPP__

