//
// file : base_attached_object.hpp
// in : file:///home/tim/projects/enfield/enfield/base_attached_object.hpp
//
// created by : Timothée Feuillet
// date: Thu Dec 29 2016 18:03:58 GMT-0500 (EST)
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

#ifndef __N_2192021529137138297_367916697_BASE_ATTACHED_OBJECT_HPP__
#define __N_2192021529137138297_367916697_BASE_ATTACHED_OBJECT_HPP__

#include "internal_base_attached_object.hpp"
#include "database_conf.hpp"

namespace neam
{
  namespace enfield
  {
    namespace attached_object
    {
      template<typename DatabaseConf, typename AttachedObjectClass, typename FinalClass, creation_flags DefaultCreationFlags = creation_flags::delayed>
      class base_tpl : public base<DatabaseConf>
      {
        private:
          using base_t = base<DatabaseConf>;

          static inline typename type_registry<DatabaseConf>::template registration<FinalClass> _registration;
          // force instantiation of the static member: (and avoid a warning)
          static_assert(&_registration == &_registration);

        public:
          using param_t = typename base_t::param_t;
          using class_t = AttachedObjectClass;
          static constexpr type_t ao_class_id = AttachedObjectClass::id;

        protected:
          // force the transient flag when non-user-gettable
          //  (there's no need to maintain anything, as simply trying to get the object will cause a compilation error)
          static constexpr creation_flags default_creation_flags = dbconf_can<DatabaseConf, AttachedObjectClass::id, attached_object_access::user_getable>() ? DefaultCreationFlags : creation_flags::transient;

          base_tpl(param_t _p)
          : base<DatabaseConf>
            (_p,
             default_creation_flags,
             type_id<FinalClass, typename DatabaseConf::attached_object_type>::id())
          {
            // Here, as when triggered the whole class has been generated
            static_assert_check_attached_object<DatabaseConf, FinalClass>();
          }

          virtual ~base_tpl() = default;

          /// \brief Return the entity id
          id_t get_entity_id() const
          {
            return this->owner.entity_id;
          }

          /// \brief Require another (requireable) attached object
          /// \note circular dependencies will not be tested when calling that function but will trigger an exception when
          ///       trying to destruct the entity. Also, circular dependencies may lead to segfaults/memory corruption
          ///       as the returned object will not be fully constructed (in case of circular dependencies).
          ///
          /// Required attached objects will be destructed after the last attached object requiring them has been destructed.
          template
          <
            typename AttachedObject,
            attached_object::creation_flags Flags = attached_object::creation_flags::none,
            typename... DataProvider
          >
          AttachedObject& require(DataProvider&&... provider)
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();

            check::debug::n_assert(!is_required<AttachedObject>(), "require: the attached object is already required");
            return this->owner.db.template add_ao_dep<AttachedObject>(this->owner, Flags, *this, std::forward<DataProvider>(provider)...);
          }

          template<typename AttachedObject>
          void unrequire()
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_removable>();

            check::debug::n_assert(is_required<AttachedObject>(), "unrequire: The attached object to be returned has not been required");

            AttachedObject* bptr = entity_get<AttachedObject>();
            if (bptr)
            {
              this->owner.db.remove_ao_dep(*bptr, this->owner, *this);
            }
          }

          /// \brief Returns whether the attached object is required (a return value of true implies that the attached object exists)
          template<typename AttachedObject>
          bool is_required() const
          {
            const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id();
            return this->requirements.is_set(id);
          }

          /// \brief Return a required attached object. If the object to be returned
          /// either does not exists or has not been required then that function will assert.
          template<typename AttachedObject>
          AttachedObject& get_required()
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();
            const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id();
            check::debug::n_assert(this->requirements.is_set(id), "get_required: The attached object to be returned has not been required");

            AttachedObject* ret = entity_get<AttachedObject>();
            check::debug::n_assert(ret != nullptr, "The attached object required does not exists");

            return *ret;
          }

          template<typename AttachedObject>
          const AttachedObject& get_required() const
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();
            const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
            check::debug::n_assert(this->requirements.is_set(id), "get_required: The attached object to be returned has not been required");

            const AttachedObject* ret = entity_get<AttachedObject>();
            check::debug::n_assert(ret != nullptr, "The attached object required does not exists");

            return *ret;
          }

          /// \brief Return a pointer to a possibly non-required attached object.
          /// This is unsafe because there is no guarantee that the returned pointer will be valid:
          /// another thread may remove the returned object at any time. (except if the returned object is one of the required objects)
          /// \note some kind of attached objects (like concepts) can only be retrieved by get_unsafe() as they can't be required.
          template<typename AttachedObject>
          AttachedObject* get_unsafe()
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_unsafe_getable>();

            return entity_get<AttachedObject>();
          }

          template<typename AttachedObject>
          const AttachedObject* get_unsafe() const
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_unsafe_getable>();

            return entity_get<AttachedObject>();
          }

          /// \brief returns true if the attached object is present on the entity, false otherwise
          template<typename AttachedObject>
          bool has() const
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_unsafe_getable>();
            return entity_has<AttachedObject>();
          }

          /// \brief Create an automanaged instance.
          /// \note The only way to destroy such a object is with self_destruct()
          /// \warning Failing to call self_destruct() will result in an assert being triggered when destructing the entity
          /// \param base An attached object that will be used to retrieve the entity on which to create the new attached object.
          template<attached_object::creation_flags Flags = attached_object::creation_flags::none, typename... DataProvider>
          static FinalClass& create_self(base_t& base, DataProvider&& ... provider)
          {
            static_assert_can<DatabaseConf, ao_class_id, attached_object_access::automanaged>();

            {
              // check for existance:
              const type_t id = type_id<FinalClass, typename DatabaseConf::attached_object_type>::id();
              if (base.owner.has(id))
              {
                FinalClass* ptr = static_cast<FinalClass*>(base.owner.get(id));
                if (ptr)
                {
                  // already existing: check to see if it's an automanaged object
                  FinalClass& ret = *ptr;
                  base_t* bptr = &ret;
                  check::debug::n_assert(bptr != (base_t*)(k_poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
                  check::debug::n_assert(bptr->automanaged == true, "create_self() called on an entity which already have an attached object of that type but that isn't automanaged");
                  return ret;
                }
              }
            }

            // create it
            FinalClass& ret = base.owner.db.template _create_ao<FinalClass>(base.owner, Flags, std::forward<DataProvider>(provider)...);
            base_t* bptr = &ret;
            check::debug::n_assert(bptr != (base_t*)(k_poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
            bptr->automanaged = true;

            return ret;
          }

          /// \brief Self destruct the attached object.
          /// This may not directly calls the destructor but instead flag the attached object to be destructed
          /// when there is no other object requiring it. This effectively bypass the user_added flag.
          /// \note This is the only way to destroy an object that has been created with create_self()
          void self_destruct()
          {
            static_assert_can<DatabaseConf, ao_class_id, attached_object_access::automanaged>();
            check::debug::n_assert(!this->authorized_destruction, "self_destruct() called while the destruction of the attached object is in progress");

            this->automanaged = false;
            this->user_added = true;
            this->owner.db.remove_ao_user(this->owner, *this);
          }

        private:
          template<typename AttachedObject>
          AttachedObject* entity_get()
          {
            return this->owner.template get<AttachedObject>();
          }

          template<typename AttachedObject>
          const AttachedObject* entity_get() const
          {
            return this->owner.template get<AttachedObject>();
          }

          template<typename AttachedObject>
          bool entity_has() const
          {
            return this->owner.template has<AttachedObject>();
          }

        private:
          friend class neam::enfield::database<DatabaseConf>;
          friend class neam::enfield::entity<DatabaseConf>;
      };
    } // namespace attached_object
  } // namespace enfield
} // namespace neam

#endif // __N_2192021529137138297_367916697_BASE_ATTACHED_OBJECT_HPP__

