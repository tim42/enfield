//
// file : enfield_types.hpp
// in : file:///home/tim/projects/enfield/enfield/enfield_types.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 16:07:41 GMT-0500 (EST)
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

#include <cstdint>
#include <cstddef>

namespace neam
{
  namespace enfield
  {
    /// \brief Alias for a type identifier
    using type_t = uint16_t;

    /// \brief Different possibilities for filtering queries
    enum class query_condition
    {
      each,
      any,
    };

    enum class for_each
    {
      next, // conitnue (default if the function returns void)
      stop, // break the for-each loop
    };


    template<typename DatabaseConf> class database;
    template<typename DatabaseConf> class base_system;
    template<typename DatabaseConf> class system_manager;
    template<typename DatabaseConf> class entity;

    template<typename DatabaseConf, typename... AttachedObjects> struct attached_object_utility;

    namespace attached_object
    {
      enum class creation_flags;

      template<typename DatabaseConf> class base;
      template<typename DatabaseConf, typename AttachedObjectClass, typename FinalClass, creation_flags DefaultCreationFlags>
      class base_tpl;
    }
  } // namespace enfield
} // namespace neam


