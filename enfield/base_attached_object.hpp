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
      template<typename DatabaseConf, typename AttachedObjectClass>
      class base_tpl : public base<DatabaseConf>
      {
        public:
          using entity_t = typename attached_object::base<DatabaseConf>::entity_t;
          using database_t = typename attached_object::base<DatabaseConf>::database_t;
          using base_t = attached_object::base<DatabaseConf>;
          using class_t = AttachedObjectClass;
          static constexpr type_t ao_class_id = AttachedObjectClass::id;

          virtual ~base_tpl() = default;

        protected:
          base_tpl(entity_t **_owner, type_t _object_type_id) : base<DatabaseConf>(_owner, _object_type_id, AttachedObjectClass::id) {}

                    /// \brief Require another (requireable) attached object
          /// \note circular dependencies will not be tested when calling that function but will trigger an exception when
          ///       trying to destruct the entity. Also, circular dependencies may lead to segfaults/memory corruption
          ///       as the returned object will not be fully constructed (in case of circular dependencies).
          ///
          /// Required attached objects will be destructed after the last attached object requiring them has been destructed.
          template<typename AttachedObject, typename DataProvider>
          AttachedObject &require(DataProvider *provider)
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();

            return this->owner->db->template add_ao_dep<AttachedObject>(this->owner, this, provider);
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
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();

            return this->owner->db->template add_ao_dep<AttachedObject>(this->owner, this);
          }


          /// \brief Return a required attached object. If the object to be returned
          /// either does not exists or has not been required then that function will throw.
          template<typename AttachedObject>
          AttachedObject &get_required()
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();

            AttachedObject *ret = entity_get<AttachedObject>();
            check::on_error::n_assert(ret != nullptr, "The attached object required does not exists");
            base_t *bptr = ret;
            check::on_error::n_assert(this->requires.count(bptr), "The attached object to be returned has not been required");

            return *ret;
          }

          template<typename AttachedObject>
          const AttachedObject &get_required() const
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_requireable>();

            AttachedObject *ret = entity_get<AttachedObject>();
            check::on_error::n_assert(ret != nullptr, "The attached object required does not exists");
            base_t *bptr = ret;
            check::on_error::n_assert(this->requires.count(bptr), "The attached object to be returned has not been required");

            return *ret;
          }

          /// \brief Return a pointer to a possibly non-required attached object.
          /// This is unsafe because there is no guarantee that the returned pointer will be valid:
          /// another thread may remove the returned object at any time. (except if the returned object is one of the required objects)
          /// \note some kind of attached objects (like concepts) can only be retrieved by get_unsafe() as they can't be required.
          template<typename AttachedObject>
          AttachedObject *get_unsafe()
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_unsafe_getable>();

            return entity_get<AttachedObject>();
          }

          template<typename AttachedObject>
          const AttachedObject *get_unsafe() const
          {
            static_assert_can<DatabaseConf, AttachedObject::ao_class_id, ao_class_id, attached_object_access::ao_unsafe_getable>();

            return entity_get<AttachedObject>();
          }

        private:
          template<typename AttachedObject>
          AttachedObject *entity_get()
          {
            if (!entity_has<AttachedObject>())
              return nullptr;
            AttachedObject *ret = static_cast<AttachedObject*>(this->owner->attached_objects[type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id]);
            check::on_error::n_assert(static_cast<base_t *>(ret) != (base_t *)(poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
            return ret;
          }

          template<typename AttachedObject>
          const AttachedObject *entity_get() const
          {
            if (!entity_has<AttachedObject>())
              return nullptr;
            const AttachedObject *ret = static_cast<const AttachedObject*>(this->owner->attached_objects[type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id]);
            check::on_error::n_assert(static_cast<const base_t *>(ret) != (base_t *)(poisoned_pointer), "The attached object required is being constructed (circular dependency ?)");
            return ret;
          }

          template<typename AttachedObject>
          bool entity_has() const
          {
            const type_t id = type_id<AttachedObject, typename DatabaseConf::attached_object_type>::id;
            const uint32_t index = id / (sizeof(uint64_t) * 8);
            const uint64_t mask = 1ul << (id % (sizeof(uint64_t) * 8));
            return (this->owner->component_types[index] & mask) != 0;
          }

        private:
          friend class neam::enfield::database<DatabaseConf>;
          friend class neam::enfield::entity<DatabaseConf>;
      };
    } // namespace attached_object
  } // namespace enfield
} // namespace neam

#endif // __N_2192021529137138297_367916697_BASE_ATTACHED_OBJECT_HPP__

