//
// created by : Timothée Feuillet
// date: 2022-4-15
//
//
// Copyright (c) 2022 Timothée Feuillet
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

#include <string>

#include "type_id.hpp"

#include <ntools/type_id.hpp>
#include <ntools/mt_check/vector.hpp>

namespace neam::enfield
{
  /// \brief Hold information about types
  template<typename DatabaseConf>
  struct type_registry
  {
    template<typename Type>
    struct registration
    {
      registration() { type_registry::add_type<Type>(); }
    };

    struct allocator_info_t
    {
      type_t id = ~(type_t)0;
      size_t size = 0;
      size_t alignment = 0;
    };

    struct debug_info_t
    {
      type_t id;
      std::string type_name;
    };

    template<typename Type>
    static void add_type()
    {
      const type_t object_type_id = type_id<Type, typename DatabaseConf::attached_object_type>::id();
      check::debug::n_assert(object_type_id < DatabaseConf::max_attached_objects_types, "neam::enfield::type_registry::add_type: type-id is too big", sizeof(type_t) * 8);
      if ((type_t)allocator_info().size() < object_type_id + 1)
      {
        allocator_info().resize(object_type_id + 1);
        debug_info().resize(object_type_id + 1);
      }
      allocator_info()[object_type_id] = {object_type_id, sizeof(Type), alignof(Type)};
      debug_info()[object_type_id] = {object_type_id, ct::type_name<Type>.str};
    }

    static auto& allocator_info()
    {
      static std::mtc_vector<allocator_info_t> _info;
      return _info;
    }
    static auto& debug_info()
    {
      static std::mtc_vector<debug_info_t> _info;
      return _info;
    }

    static size_t get_registered_type_count()
    {
      return allocator_info().size();
    }
  };
}
