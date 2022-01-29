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

#include <set>

#include <ntools/debug/assert.hpp>
#include "enfield_types.hpp"


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
        private:
          using entity_t = entity<DatabaseConf>;
          using entity_data_t = typename entity<DatabaseConf>::data_t;
          using database_t = database<DatabaseConf>;

        public:
          using param_t = entity_t **;

        private:
          base(entity_t **_owner, type_t _object_type_id, type_t _class_id)
            : object_type_id(_object_type_id), class_id(_class_id), owner((*_owner)->data)
          {
            check::debug::n_assert(object_type_id < DatabaseConf::max_attached_objects_types, "Too many attached object types for the current configuration");
          };

          virtual ~base()
          {
            check::debug::n_assert(authorized_destruction, "Trying to destroy an attached object in an unauthorized fashion");
            check::debug::n_assert(required_by.empty(), "Trying to destroy an attached object when other attached objects require it");
            check::debug::n_assert(requirements.empty(), "Trying to destroy an attached object that hasn't been properly cleaned-up (still some dependency)");
            check::debug::n_assert(!user_added, "Trying to destroy an attached object when only the user may remove it (it has been flagged as user-added)");
            check::debug::n_assert(!automanaged, "Trying to destroy an attached object when only itself may remove it (via self_destruct())");
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

          /// \brief Return true if the user required this attached object
          bool is_user_added() const
          {
            return user_added;
          }
          /// \brief Return true if the attached object is automanaged (self destruction / self creation)
          bool is_automanaged() const
          {
            return automanaged;
          }

        private:
          /// \brief The entity that owns that attached object
          entity_data_t *owner;
          uint64_t index = 0;

          bool user_added = false;
          bool automanaged = false;
          std::set<base *> required_by;
          std::set<base *> requirements;

          bool authorized_destruction = false;

          friend class neam::enfield::database<DatabaseConf>;
          friend class neam::enfield::entity<DatabaseConf>;

          // allow the deallocator to call the destructor
          template<typename DBC>
          friend void DatabaseConf::attached_object_allocator::deallocate(type_t, base<DBC> &) noexcept;

          template<typename DBC, typename AttachedObjectClass, typename FC>
          friend class base_tpl;
      };
    } // namespace attached_object
  } // namespace enfield
} // namespace neam

#endif // __N_37571836813711018_1068127766_BASE_ENTITY_ATTACHED_OBJECT_HPP__

