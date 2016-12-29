//
// file : base_entity_attached_object.hpp
// in : file:///home/tim/projects/enfield/enfield/base_entity_attached_object.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 16:15:27 GMT-0500 (EST)
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

#ifndef __N_37571836813711018_1068127766_BASE_ENTITY_ATTACHED_OBJECT_HPP__
#define __N_37571836813711018_1068127766_BASE_ENTITY_ATTACHED_OBJECT_HPP__

#include "enfield_types.hpp"
#include "enfield_exception.hpp"

#include <set>

namespace neam
{
  namespace enfield
  {
    template<typename DatabaseConf> class entity;
    template<typename DatabaseConf> class database;

    namespace attached_object
    {
      /// \brief Everything attached to an entity (views, components, ...) must inherit indirectly from this class
      /// An attached object should not have access and must not use the public interface of the entity class.
      /// Moreover the entity class may be moved in memory and its displacement is not an atomic operation.
      ///
      /// So any attached object should ONLY use the protected interface below.
      template<typename DatabaseConf>
      class base
      {
        public:
          using entity_t = entity<DatabaseConf>;
          using entity_data_t = typename entity<DatabaseConf>::data_t;
          using database_t = database<DatabaseConf>;

        protected:
          base(entity_t **_owner, type_t _object_type_id, type_t _class_id)
            : owner((*_owner)->data), object_type_id(_object_type_id), class_id(_class_id) {};
          virtual ~base()
          {
            check::on_error::n_assert(authorized_destruction, "Trying to destroy an attached object in an unauthorized fashion");
            check::on_error::n_assert(required_by.empty(), "Trying to destroy an attached object when other attached objects require it");
            check::on_error::n_assert(requires.empty(), "Trying to destroy an attached object that hasn't been properly cleaned-up (still some dependency)");
            check::on_error::n_assert(user_added, "Trying to destroy an attached object when only the user may remove it (it has been flagged as user-added)");
          }

          /// \brief Require another (requireable) attached object
          /// \note circular dependencies will not be tested when calling that function but will trigger an exception when
          ///       trying to destruct the entity. Also, circular dependencies may lead to segfaults/memory corruption
          ///       as the returned object will not be fully constructed (in case of circular dependencies).
          ///
          /// Required attached objects will be destructed after the last attached object requiring them has been destructed.
          template<typename AttachedObject, typename DataProvider>
          AttachedObject &require(DataProvider *provider)
          {
            return owner->db->add_ao_dep(owner, this, provider);
          }

          /// \brief Require another (requireable) attached object
          /// \note circular dependencies will not be tested when calling that function but will trigger an exception when
          ///       trying to destruct the entity. Also, circular dependencies may lead to segfaults/memory corruption
          ///       as the returned object will not be fully constructed (in case of circular dependencies).
          ///
          /// Required attached objects will be destructed after the last attached object requiring them has been destructed.
          template<typename AttachedObject>
          AttachedObject &require()
          {
            return owner->db->add_ao_dep(owner, this);
          }


          /// \brief Return a required attached object. If the object to be returned
          /// either does not exists or has not been required then that function will throw.
          template<typename AttachedObject>
          AttachedObject &get_required()
          {
            AttachedObject *ret = get_unsafe();
            check::on_error::n_assert(ret != nullptr, "The attached object required does not exists");
            base *bptr = ret;
            check::on_error::n_assert(requires.count(bptr), "The attached object to be returned has not been required");

            return *ret;
          }

          template<typename AttachedObject>
          const AttachedObject &get_required() const
          {
            AttachedObject *ret = get_unsafe();
            check::on_error::n_assert(ret != nullptr, "The attached object required does not exists");
            base *bptr = ret;
            check::on_error::n_assert(requires.count(bptr), "The attached object to be returned has not been required");

            return *ret;
          }

          /// \brief Return a pointer to a possibly non-required attached object.
          /// This is unsafe because there is no guarantee that the returned pointer will be valid:
          /// another thread may remove the returned object at any time. (except if the returned object is one of the required objects)
          /// \note some kind of attached objects (like concepts) can only be retrieved by get_unsafe() as they can't be required.
          template<typename AttachedObject>
          AttachedObject *get_unsafe()
          {
            return owner->owner->template get<AttachedObject>();
          }

          template<typename AttachedObject>
          const AttachedObject *get_unsafe() const
          {
            return owner->owner->template get<AttachedObject>();
          }

          /// \brief Self destruct the attached object.
          /// This may not directly calls the destructor but instead flag the attached object to be destructed
          /// when there is no other object requiring it. This effectively bypass the user_added flag.
          void commit_suicide()
          {
            owner->db->remove_ao_user(owner, this);
          }

        public:
          union
          {
            struct
            {
              /// \brief The id of the type of the attached object
              const type_t object_type_id;
              /// \brief The class id of the attached object (view, component, ...)
              const type_t class_id;
            };
            const id_t gen_type_id;
          };
          static_assert(sizeof(id_t) == 2 * sizeof(type_t), "Incompatible id_t and type_t");

        private:
          /// \brief The entity that owns that attached object
          entity_data_t *owner;

          bool user_added = false;
          std::set<base *> required_by;
          std::set<base *> requires;

          bool authorized_destruction = false;

          friend class neam::enfield::database<DatabaseConf>;
          friend class neam::enfield::entity<DatabaseConf>;
      };
    } // namespace attached_object
  } // namespace enfield
} // namespace neam

#endif // __N_37571836813711018_1068127766_BASE_ENTITY_ATTACHED_OBJECT_HPP__

