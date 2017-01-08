//
// file : database_conf.hpp
// in : file:///home/tim/projects/enfield/enfield/database_conf.hpp
//
// created by : Timothée Feuillet
// date: Thu Dec 29 2016 17:52:03 GMT-0500 (EST)
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

#ifndef __N_24947127962899119538_2330222174_DATABASE_CONF_HPP__
#define __N_24947127962899119538_2330222174_DATABASE_CONF_HPP__

#include "enfield_exception.hpp"
#include "enfield_types.hpp"

#include "tools/ct_list.hpp"

namespace neam
{
  namespace enfield
  {
    namespace attached_object
    {
      template<typename DatabaseConf, typename AttachedObjectClass, typename FinalClass> class base_tpl;
      template<typename DatabaseConf> class base;
    } // namespace attached_object

    enum class attached_object_access : int
    {
      /// \brief Create an automanaged attached object (not creatable, except by itself, not removable, except by itself)
      none = 0,

      /// \brief Can another attached object require that an attached object of that class ? (this allows implicit creation via a require call)
      ao_requireable =      1 << 2,
      /// \brief Can another attached object destruct an attached object of that class ?
      /// \note If not in the rights, the only way to destroy an attached object of that class is either:
      ///         - by an entity destruction
      ///         - when the attached object calls commit_suicide() on itself
      /// \todo [tim] implementation of that right
      ao_removable =        1 << 3,

      /// \brief Allow that attached object to be retrieved via get_unsafe
      ao_unsafe_getable =   1 << 4,

      /// \brief Allow automanagement of the attached object class (self creation / self destruction)
      automanaged =         1 << 5,

      /// \brief grant all rights to other attached objects
      ao_all = ao_requireable | ao_removable | ao_unsafe_getable | automanaged,
      /// \brief grant all "safe" rights to other attached objects
      ao_all_safe = ao_requireable | ao_removable,

      /// \brief Can the user (user = public API of the entity) create an attached object of that class ?
      user_creatable =      1 << 8,
      /// \brief Can the user (user = public API of the entity) get an attached object of that class ?
      /// \note This allows the user to perform queries.
      user_getable =        1 << 9,
      /// \brief Can the user (user = public API of the entity) remove an attached object of that class ?
      user_removable =      1 << 10,

      /// \brief grant all rights to the user
      user_all = user_creatable | user_getable | user_removable,

      /// \brief Grant all rights to everybody
      all = ao_all | user_all,
    };
    inline constexpr attached_object_access operator | (attached_object_access a, attached_object_access b) { return static_cast<attached_object_access>((int)a | (int)b); }
    inline constexpr attached_object_access operator & (attached_object_access a, attached_object_access b) { return static_cast<attached_object_access>((int)a & (int)b); }
    inline constexpr attached_object_access operator ~ (attached_object_access a) { return static_cast<attached_object_access>(~(int)a); }
    inline /*constexpr*/ attached_object_access &operator |= (attached_object_access& a, attached_object_access b) { a = static_cast<attached_object_access>((int)a | (int)b); return a; }
    inline /*constexpr*/ attached_object_access &operator &= (attached_object_access& a, attached_object_access b) { a = static_cast<attached_object_access>((int)a & (int)b); return a; }

    namespace internal
    {
      template<typename List, type_t ClassId>
      struct class_id
      {
        template<typename X>
        struct _find
        {
          static constexpr bool value = (X::id == ClassId);
        };

        using type = typename List::template find_if<_find>::type;
      };
    }

    /// \brief Check that the corresponding type satisfy the database requirements
    template<typename DatabaseConf, typename AttachedObject>
    constexpr inline bool static_assert_check_attached_object()
    {
      // Enfield default checks
      static_assert(std::is_base_of<neam::enfield::attached_object::base<DatabaseConf>, AttachedObject>::value, "invalid type: is not an attached object");

      using class_t = typename internal::class_id<typename DatabaseConf::classes, AttachedObject::ao_class_id>::type;
      static_assert(!std::is_same<ct::type_not_found, class_t>::value, "invalid attached object Class ID");
      static_assert(std::is_same<class_t, typename AttachedObject::class_t>::value, "invalid attached object class");
      static_assert(std::is_base_of<neam::enfield::attached_object::base_tpl<DatabaseConf, class_t, AttachedObject>, AttachedObject>::value, "invalid attached object: does not inherit from the correct class");

      // Type specific checks (note: you should static_assert in that class)
      static_assert(DatabaseConf::template check_attached_object<AttachedObject::ao_class_id, AttachedObject>::value, "invalid attached object: database conf checks have failed");
      return true;
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, attached_object_access Operation>
    constexpr inline bool dbconf_can()
    {
      return (DatabaseConf::template class_rights<AttachedObjectClass>::access & Operation) != attached_object_access::none;
    }
    template<typename DatabaseConf, typename AttachedObjectClass, attached_object_access Operation>
    constexpr inline bool dbconf_can()
    {
      return dbconf_can<DatabaseConf, AttachedObjectClass::ao_class_id, Operation>();
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, type_t OtherAttachedObjectClass, attached_object_access Operation>
    constexpr inline bool dbconf_can()
    {
      return (DatabaseConf::template specific_class_rights<AttachedObjectClass, OtherAttachedObjectClass>::access & Operation) != attached_object_access::none;
    }
    template<typename DatabaseConf, typename AttachedObjectClass, typename OtherAttachedObjectClass, attached_object_access Operation>
    constexpr inline bool dbconf_can()
    {
      return dbconf_can<DatabaseConf, AttachedObjectClass::ao_class_id, OtherAttachedObjectClass::ao_class_id, Operation>();
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert(dbconf_can<DatabaseConf, AttachedObjectClass, Operation>(), "Operation not permitted");
      return true;
    }
    template<typename DatabaseConf, typename AttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert(dbconf_can<DatabaseConf, AttachedObjectClass, Operation>(), "Operation not permitted");
      return true;
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, type_t OtherAttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert(dbconf_can<DatabaseConf, AttachedObjectClass, OtherAttachedObjectClass, Operation>(), "Operation on not permitted in the current context");
      return true;
    }
    template<typename DatabaseConf, typename AttachedObjectClass, typename OtherAttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert(dbconf_can<DatabaseConf, AttachedObjectClass, OtherAttachedObjectClass, Operation>(), "Operation on not permitted in the current context");
      return true;
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if (!dbconf_can<DatabaseConf, AttachedObjectClass, Operation>())
        throw exception_tpl<DatabaseConf>("Operation not permitted", __FILE__, __LINE__);
    }
    template<typename DatabaseConf, typename AttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if (!dbconf_can<DatabaseConf, AttachedObjectClass, Operation>())
        throw exception_tpl<DatabaseConf>("Operation not permitted", __FILE__, __LINE__);
    }

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, type_t OtherAttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if (!dbconf_can<DatabaseConf, AttachedObjectClass, OtherAttachedObjectClass, Operation>())
        throw exception_tpl<DatabaseConf>("Operation not permitted in the current context", __FILE__, __LINE__);
    }
    template<typename DatabaseConf, typename AttachedObjectClass, typename OtherAttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if (!dbconf_can<DatabaseConf, AttachedObjectClass, OtherAttachedObjectClass, Operation>())
        throw exception_tpl<DatabaseConf>("Operation not permitted in the current context", __FILE__, __LINE__);
    }
  } // namespace enfield
} // namespace neam

#endif // __N_24947127962899119538_2330222174_DATABASE_CONF_HPP__

