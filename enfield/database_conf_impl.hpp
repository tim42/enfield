//
// file : database_conf_impl.hpp
// in : file:///home/tim/projects/enfield/enfield/database_conf_impl.hpp
//
// created by : Timothée Feuillet
// date: Fri Dec 30 2016 12:56:50 GMT-0500 (EST)
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


#include "database_conf.hpp"
#include "enfield_types.hpp"

#include <ntools/ct_list.hpp>

namespace neam
{
  namespace enfield
  {
    /// A DB configuration allow a fine grained configuration over what is permitted:
    ///   - what kind of "attached objects" classes an entity can have (components, concepts, ...)
    ///   - what are the specifc access rights of a given attached object class
    namespace db_conf
    {
      // HELPERS //

      /// \brief Does not perform any checks
      template<bool Value>
      struct no_check
      {
        static constexpr bool value = Value;
      };

      // CONFS //

      /// \brief ECCS (entity component concept system) configuration
      /// This is the default DB configuration.
      struct eccs
      {
          // type markers (mandatory)
          struct attached_object_class;
          struct attached_object_type;
          struct system_type;

          // allowed attached object classes (the constexpr type_t id is mandatory):
          struct component_class { static constexpr type_t id = 0; };
          struct concept_class { static constexpr type_t id = 1; };

          // the list of classes
          using classes = ct::type_list<component_class, concept_class>;

          // "rights" configuration:
          template<type_t ClassId>
          struct class_rights
          {
            // default configuration: (must be static constexpr)

            /// \brief Define general access rights
            static constexpr attached_object_access access = attached_object_access::all_no_automanaged;
          };

          /// \brief Specify the rights of OtherClassId over ClassId.
          /// In this mode, only attached_object_access::ao_* are accounted
          /// the default is to use the class_rights.
          template<type_t ClassId, type_t OtherClassId>
          struct specific_class_rights : public class_rights<ClassId> {};

          /// \brief In that mode, the default enfield checks are OK
          template<type_t ClassId, typename AttachedObject>
          struct check_attached_object : no_check<true> {};


          /// \brief The maximum number of components
          static constexpr uint64_t max_attached_objects_types = 4 * 64;

          /// \brief The attached object allocator
          using attached_object_allocator = default_attached_object_allocator<eccs>;

          /// \brief Completly disallow the use of attached_object_db.
          /// Remove usage of queries.
          /// for-each is a bit slower (overhead) when the matching entity count is low ( < 20% of the database)
          /// Attached Object creation and suppression and optimize() is much much faster
          /// If your data is very dynamic, should be false.
          static constexpr bool use_attached_object_db = true;
          static constexpr bool use_entity_db = true;

          static constexpr bool allow_ref_counting_on_entities = true;
      };
      template<>
      struct eccs::class_rights<eccs::concept_class::id>
      {
        // specific configuration: (must be static constexpr)

        /// \brief Define general access rights
        static constexpr attached_object_access access = attached_object_access::automanaged | attached_object_access::ao_unsafe_getable | attached_object_access::ext_getable | attached_object_access::db_queryable;
      };

      using enfield_default = eccs;

      /// \brief A conservative ECCS (entity component concept system) configuration
      /// In that configuration, concepts cannot require components
      struct conservative_eccs
      {
          // type markers (mandatory)
          struct attached_object_class;
          struct attached_object_type;
          struct system_type;

          // allowed attached object classes (the constexpr type_t id is mandatory):
          struct component_class { static constexpr type_t id = 0; };
          struct concept_class { static constexpr type_t id = 1; };

          // the list of classes
          using classes = ct::type_list<component_class, concept_class>;

          // "rights" configuration:
          template<type_t ClassId>
          struct class_rights
          {
            // default configuration: (must be static constexpr)

            /// \brief Define general access rights
            static constexpr attached_object_access access = attached_object_access::all_no_automanaged;
          };

          /// \brief Specify the rights of OtherClassId over ClassId.
          /// In this mode, only attached_object_access::ao_* are accounted
          /// the default is to use the class_rights.
          template<type_t ClassId, type_t OtherClassId>
          struct specific_class_rights : public class_rights<ClassId> {};

          /// \brief In that mode, the default enfield checks are OK
          template<type_t ClassId, typename AttachedObject>
          struct check_attached_object : no_check<true> {};

          /// \brief The maximum number of components
          static constexpr uint64_t max_attached_objects_types = 4 * 64;

          /// \brief The attached object allocator
          using attached_object_allocator = default_attached_object_allocator<conservative_eccs>;

          /// \brief Completly disallow the use of attached_object_db.
          /// Remove usage of queries.
          /// for-each is a bit slower (overhead) when the matching entity count is low ( < 20% of the database)
          /// Attached Object creation and suppression and optimize() is much much faster
          /// If your data is very dynamic, should be false.
          static constexpr bool use_attached_object_db = true;
          static constexpr bool use_entity_db = true;

          static constexpr bool allow_ref_counting_on_entities = true;
      };
      template<>
      struct conservative_eccs::class_rights<conservative_eccs::concept_class::id>
      {
        // specific configuration: (must be static constexpr)

        /// \brief Define general access rights
        static constexpr attached_object_access access = attached_object_access::automanaged | attached_object_access::ao_unsafe_getable | attached_object_access::ext_getable | attached_object_access::db_queryable;
      };
      template<>
      struct conservative_eccs::specific_class_rights<conservative_eccs::component_class::id, conservative_eccs::concept_class::id>
      {
        /// \brief Define specific access rights (concepts only have those rights when dealing with components)
        static constexpr attached_object_access access = attached_object_access::ao_unsafe_getable;
      };

      /// \brief A plain dull ECS configuration
      struct ecs
      {
          // type markers (mandatory)
          struct attached_object_class;
          struct attached_object_type;
          struct system_type;

          // allowed attached object classes (the constexpr type_t id is mandatory):
          struct component_class { static constexpr type_t id = 0; };

          // the list of classes
          using classes = ct::type_list<component_class>;

          // "rights" configuration:
          template<type_t ClassId>
          struct class_rights
          {
            // default configuration: (must be static constexpr)

            /// \brief Define general access rights
            static constexpr attached_object_access access = attached_object_access::all_no_automanaged;
          };

          /// \brief Specify the rights of OtherClassId over ClassId.
          /// In this mode, only attached_object_access::ao_* are accounted
          /// the default is to use the class_rights.
          template<type_t ClassId, type_t OtherClassId>
          struct specific_class_rights : public class_rights<ClassId> {};

          /// \brief In that mode, the default enfield checks are OK
          template<type_t ClassId, typename AttachedObject>
          struct check_attached_object : no_check<true> {};

          /// \brief The maximum number of components
          static constexpr uint64_t max_attached_objects_types = 4 * 64;

          /// \brief The attached object allocator
          using attached_object_allocator = default_attached_object_allocator<ecs>;

          /// \brief Completly disallow the use of attached_object_db.
          /// Remove usage of queries.
          /// for-each is a bit slower (overhead) when the matching entity count is low ( < 20% of the database)
          /// Attached Object creation and suppression and optimize() is much much faster
          /// If your data is very dynamic, should be false.
          static constexpr bool use_attached_object_db = true;
          static constexpr bool use_entity_db = true;

          static constexpr bool allow_ref_counting_on_entities = true;
      };
    } // namespace db_conf
  } // namespace enfield
} // namespace neam



