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

#pragma once


#include <set>

#include <ntools/debug/assert.hpp>
#include <ntools/raw_memory_pool_ts.hpp>
#include "../enfield_types.hpp"
#include "../mask.hpp"


namespace neam
{
  namespace enfield
  {
    static constexpr uint64_t k_poisoned_pointer = uint64_t(0xA5A5A5A00A5A5A5A);
    template<typename DatabaseConf> class entity_weak_ref;

    namespace attached_object
    {
      enum class creation_flags
      {
        none, // equivalent to delayed / no change
        delayed, // default behavior
        transient, // fast creation and deletion, no for-each and queries
        force_immediate_changes // slow creation (delayed deletion), immediate availlability to for-each and queries
      };

      /// \brief Everything attached to an entity (views, components, ...) must inherit indirectly from this class
      /// An attached object should not have access and must not use the public interface of the entity class.
      /// Moreover the entity class may be moved in memory and its displacement is not an atomic operation.
      ///
      /// So any attached object should ONLY use the protected interface below.
      template<typename DatabaseConf>
      class base
      {
        public:
          using entity_data_t = typename entity<DatabaseConf>::data_t;
          using database_t = database<DatabaseConf>;
          using base_t = base<DatabaseConf>;

        public:
          using param_t = entity_data_t&;

        private:
          base(param_t& _owner, creation_flags flags, type_t _object_type_id)
            : owner(_owner), object_type_id(_object_type_id)
          {
            set_creation_flags(flags);

            check::debug::n_assert(object_type_id < DatabaseConf::max_attached_objects_types, "Too many attached object types for the current configuration");
          };

          virtual ~base()
          {
            check::debug::n_assert(authorized_destruction, "Trying to destroy an attached object in an unauthorized fashion");
            check::debug::n_assert(required_count == 0, "Trying to destroy an attached object when other attached objects require it");
            check::debug::n_assert(!externally_added, "Trying to destroy an attached object when only an external source may remove it (it has been flagged as externally-added)");
            check::debug::n_assert(!automanaged, "Trying to destroy an attached object when only itself may remove it (via self_destruct())");

            if (requirements.has_mask())
            {
              for (uint32_t i = 0; i < owner.attached_objects.size();)
              {
                auto& it = owner.attached_objects[i];
                if (requirements.is_set(it.first))
                {
                  requirements.unset(it.first);
                  check::debug::n_assert(!it.second->authorized_destruction, "Dependency cycle detected when trying to remove an attached object");
                  check::debug::n_assert(!it.second->requirements.is_set(object_type_id), "Dependency cycle detected when trying to remove an attached object");
                  check::debug::n_assert(it.second->required_count > 0, "attached-object cleanup: The attached object to be unrequired has an invalid dep counter (ao id: {})", it.first);

                  it.second->required_count -= 1;
                  if (it.second->can_be_destructed())
                  {
                    owner.db._delete_ao(*it.second, owner);
                    continue;
                  }
                }
                ++i;
              }
            }

            check::debug::n_assert(!requirements.has_any_bit_set(), "Missing entries during attached object cleanup. Invalid state detected.");
          }

        public:
          /// \brief Return true if this attached object was externally required (using the entity API)
          bool is_externally_added() const
          {
            return externally_added;
          }

          /// \brief Return true if the attached object is automanaged (self destruction / self creation)
          bool is_automanaged() const
          {
            return automanaged;
          }

          /// \brief Indicate that the attached-object is in the process of being destroyed
          /// \note Only true during the destruction process
          ///       (when the required attached-objects are unrequired, and during the destructor call)
          bool is_pending_destruction() const
          {
            return authorized_destruction;
          }

          /// \brief If true, the attached-object can be safely destroyed as no one hold a reference to it
          /// \note Mostly used internally
          bool can_be_destructed() const
          {
            return !automanaged && !externally_added && required_count == 0;
          }

          /// \brief Return whether a call to unrequire will result in the destruction of the object
          /// \warning Relying on this can be dangerous (in the case where multiple attached-object are waiting to be the last one)
          bool can_be_destructed_if_unrequired() const
          {
            return !automanaged && !externally_added && required_count <= 1;
          }

          entity_weak_ref<DatabaseConf> create_entity_weak_reference_tracking()
          {
            return entity_weak_ref<DatabaseConf>{owner.weak_ref_indirection.get()};
          }

          database_t& get_database() { return owner.db; }
          const database_t& get_database() const { return owner.db; }

        private:
          /// \brief set the creation flags. Must be called during the construction process
          void set_creation_flags(creation_flags flags)
          {
            fully_transient_attached_object = (flags == creation_flags::transient);
            force_immediate_db_change = (flags == creation_flags::force_immediate_changes);
            check::debug::n_assert(fully_transient_attached_object != force_immediate_db_change || !fully_transient_attached_object,
                                   "Cannot have a fully transient object that should also apply changes in the DB");
          }



        private:
          delayed_mask<DatabaseConf> requirements;

          /// \brief The entity that owns that attached object
          entity_data_t& owner;

          uint32_t index = 0;

        public:
          /// \brief The id of the type of the attached object
          const type_t object_type_id;

        private:
          uint8_t required_count = 0;

          // can add up-to 8 flags
          bool externally_added : 1 = false;
          bool automanaged : 1 = false;
          bool authorized_destruction : 1 = false;
          bool in_attached_object_db : 1 = false;

          // If set to true, the attached-object will be fully transient.
          // A transient attached-object is not added to the attached_object_db,
          // meaning that:
          //  - creation and deletion are much much faster
          //  - querying it (for-each/query or some systems) will not return the attached-object
          //  - concepts will still be updated (meaning queries on concepts will catch transient AO implementing such concepts)
          // NOTE: all non-externally-gettable attached-objects are fully-transient by default. (as for-each and such are not permitted)
          bool fully_transient_attached_object : 1 = false;
          // Cannot be set to true if fully_transient_attached_object is true.
          // An attached-object created with this flag to true will immediately be inserted in the attached-object db.
          //  - creation will be slower, increasing lock contention
          //  - queries, for-each, and some systems will be immediately aware of that attached-object
          //  - removal is still deferred, as this would impact for-each, systems and queries
          bool force_immediate_db_change : 1 = false;

          friend class neam::enfield::database<DatabaseConf>;
          friend class neam::enfield::entity<DatabaseConf>;


          template<typename DBC, typename AttachedObjectClass, typename FC, creation_flags DCF>
          friend class base_tpl;
      };
    } // namespace attached_object
  } // namespace enfield
} // namespace neam



