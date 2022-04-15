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

#include "type_registry.hpp"

#include <ntools/raw_memory_pool_ts.hpp>

namespace neam::enfield
{
  /// \brief A mak stored inline with the container class
  template<typename DatabaseConf>
  struct inline_mask
  {
    static constexpr size_t k_entry_count = (DatabaseConf::max_attached_objects_types + 63) / (64);
    static inline const size_t entry_count = (type_registry<DatabaseConf>::get_registered_type_count() + 63) / 64;

    inline_mask()
    {
      for (size_t j = 0; j < entry_count; ++j)
      {
        mask[j] = 0;
      }
    }

    // perform (*this & other) == *this
    bool match(const inline_mask& o) const
    {
      for (size_t j = 0; j < entry_count; ++j)
      {
        if ((mask[j] & o.mask[j]) != mask[j])
          return false;
      }
      return true;
    }

    bool operator == (const inline_mask& o) const
    {
      for (size_t j = 0; j < entry_count; ++j)
      {
        if (mask[j] != o.mask[j])
          return false;
      }
      return true;
    }

    void set(type_t id)
    {
      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      mask[index] |= bit_mask;
    }

    void unset(type_t id)
    {
      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      mask[index] &= ~bit_mask;
    }

    bool is_set(type_t id) const
    {
      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      return (mask[index] & bit_mask) != 0;
    }

    bool has_any_bit_set() const
    {
      for (size_t j = 0; j < entry_count; ++j)
      {
        if (mask[j] != 0)
          return true;
      }
      return false;
    }

    uint64_t mask[k_entry_count];
  };

  /// \brief A mask with a delayed allocation/initialization.
  /// Will try to defer the allocation as late as possible.
  template<typename DatabaseConf>
  struct delayed_mask
  {
    static inline const size_t entry_count = (type_registry<DatabaseConf>::get_registered_type_count() + 63) / 64;
    static inline cr::raw_memory_pool_ts mask_pool {sizeof(uint64_t) * entry_count, alignof(uint64_t), 4};

    delayed_mask() = default;
    ~delayed_mask()
    {
      mask_pool.deallocate(mask);
    }

    void init()
    {
      mask = (uint64_t*)mask_pool.allocate();
      for (size_t j = 0; j < entry_count; ++j)
      {
        mask[j] = 0;
      }
    }

    // perform (*this & other) == *this
    bool match(const delayed_mask& o) const
    {
      // we don't have any bit set: 0 & x == 0
      if (!mask) return true;

      for (size_t j = 0; j < entry_count; ++j)
      {
        if ((mask[j] & o.mask[j]) != mask[j])
          return false;
      }
      return true;
    }

    bool operator == (const delayed_mask& o) const
    {
      if (!mask) return !o.has_any_bit_set();

      for (size_t j = 0; j < entry_count; ++j)
      {
        if (mask[j] != o.mask[j])
          return false;
      }
      return true;
    }

    void set(type_t id)
    {
      if (!mask) init();

      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      mask[index] |= bit_mask;
    }

    void unset(type_t id)
    {
      if (!mask) return;

      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      mask[index] &= ~bit_mask;
    }

    bool is_set(type_t id) const
    {
      if (!mask) return false;

      const uint32_t index = id / 64;
      const uint64_t bit_mask = 1ul << (id % 64);
      return (mask[index] & bit_mask) != 0;
    }

    bool has_any_bit_set() const
    {
      if (!mask) return false;

      for (size_t j = 0; j < entry_count; ++j)
      {
        if (mask[j] != 0)
          return true;
      }
      return false;
    }

    // we use delayed initialization to allocate the mask as late as possible
    uint64_t* mask = nullptr;
  };
}
