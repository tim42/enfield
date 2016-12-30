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

namespace neam
{
  namespace enfield
  {
    enum class attached_object_access : int
    {
      /// \brief Create an automanaged attached object (not creatable, except by itself, not removable, except by itself)
      none = 0,

      /// \brief Can another attached object via a require operation create an attached object of that class ?
      /// \note without ao_requireable it does not have any effect.
      /// \note specifying ao_requireable but not ao_creatable won't generate compilation errors but will trigger exceptions.
      ao_creatable =        1 << 1,
      /// \brief Can another attached object require that an attached object of that class ?
      /// \note if ao_creatable isn't specified, the attached object class can only be retrieved, not created.
      ///       This won't add compile-time checks as this operation can only be performed at runtime.
      ao_requireable =      1 << 2,
      /// \brief Can another attached object destruct an attached object of that class ?
      /// \note If not in the rights, the only way to destroy an attached object of that class is either:
      ///         - by an entity destruction
      ///         - when the attached object calls commit_suicide() on itself
      ao_removable =        1 << 3,

      /// \brief Allow that attached object to be retrieved via get_unsafe
      ao_unsafe_getable =   1 << 4,

      /// \brief Allow automanagement of the attached object class (self creation / self destruction)
      automanaged =         1 << 5,

      /// \brief grant all rights to other attached objects
      ao_all = ao_creatable | ao_requireable | ao_removable | ao_unsafe_getable,
      /// \brief grant all "safe" rights to other attached objects
      ao_all_safe = ao_creatable | ao_requireable | ao_removable,

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

    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert((DatabaseConf::template class_rights<AttachedObjectClass>::access & Operation) != attached_object_access::none, "Operation on AttachedObject not permitted");
      return true;
    }
    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, type_t OtherAttachedObjectClass, attached_object_access Operation>
    constexpr inline bool static_assert_can()
    {
      static_assert((DatabaseConf::template specific_class_rights<AttachedObjectClass, OtherAttachedObjectClass>::access & Operation) != attached_object_access::none,
                    "Operation on AttachedObject not permitted for OtherAttachedObject");
      return true;
    }
    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if ((DatabaseConf::template class_rights<AttachedObjectClass>::access & Operation) == attached_object_access::none)
        throw exception_tpl<DatabaseConf>("Operation on AttachedObject not permitted", __FILE__, __LINE__);
    }
    /// \brief Check that an operation is possible on a given AttachedObject
    template<typename DatabaseConf, type_t AttachedObjectClass, type_t OtherAttachedObjectClass, attached_object_access Operation>
    inline void throw_can()
    {
      if ((DatabaseConf::template specific_class_rights<AttachedObjectClass, OtherAttachedObjectClass>::access & Operation) == attached_object_access::none)
        throw exception_tpl<DatabaseConf>("Operation on AttachedObject not permitted for OtherAttachedObject");
    }
  } // namespace enfield
} // namespace neam

#endif // __N_24947127962899119538_2330222174_DATABASE_CONF_HPP__

