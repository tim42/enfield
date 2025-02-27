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

#include <mutex>
#include "enfield_types.hpp"
#include "mask.hpp"

namespace neam::enfield
{
  template<typename DatabaseConf, typename... AttachedObjects>
  struct attached_object_utility
  {
    static constexpr void check()
    {
      (static_assert_check_attached_object<DatabaseConf, AttachedObjects>(), ...);
      (static_assert_can<DatabaseConf, AttachedObjects, attached_object_access::db_queryable>(), ...);
    }
    using database_t = database<DatabaseConf>;
    using entity_t = entity<DatabaseConf>;
    using entity_data_t = typename entity_t::data_t;
    template<typename AO>
    using id_t = type_id<AO, typename DatabaseConf::attached_object_type>;

    static type_t get_min_entry_count(const database_t& db)
    {
      type_t attached_object_id = ~type_t(0);
      size_t min_count = ~0ul;
      (
        ((db.attached_object_db[id_t<AttachedObjects>::id()].db.size() < min_count) ?
         (
           min_count = db.attached_object_db[id_t<AttachedObjects>::id()].db.size(),
           attached_object_id = id_t<AttachedObjects>::id(),
           0
         ) : 0), ...
      );
      return attached_object_id;
    }

    static void lock_shared(const database_t& db)
    {
      if constexpr (sizeof...(AttachedObjects) > 1)
      {
        std::lock(spinlock_shared_adapter::adapt(db.attached_object_db[id_t<AttachedObjects>::id()].lock)...);
      }
      else
      {
        // we have just a single entry:
        (db.attached_object_db[id_t<AttachedObjects>::id()].lock.lock_shared(), ...);
      }
    }

    static void unlock_shared(const database_t& db)
    {
      ((db.attached_object_db[id_t<AttachedObjects>::id()].lock.unlock_shared()), ...);
    }
    struct shared_locker
    {
      void lock()
      {
        lock_shared(db);
      }
      void unlock()
      {
        unlock_shared(db);
      }
      const database_t& db;
    };

    static inline_mask<DatabaseConf> make_mask()
    {
      inline_mask<DatabaseConf> mask;
      (mask.set(id_t<AttachedObjects>::id()), ...);
      return mask;
    }

    template<typename Func, typename... Params>
    static auto do_call_func(const Func& fnc, Params*... objs)
    {
      // If the entity is being constructed and for-each is called at that time,
      // it will return null pointers while still marking the entity as having the component
      // so we simply skip the call if that's the case
      if (((objs != nullptr) && ...))
        return fnc(*objs...);

      using ret_type = std::invoke_result_t<Func, AttachedObjects& ...>;
      if constexpr(std::is_same_v<enfield::for_each, ret_type>)
        return for_each::next;
      else
        return;
    }

    template<typename Func>
    static for_each call(const Func& fnc, database_t& db, entity_data_t& data)
    {
      using ret_type = std::invoke_result_t<Func, AttachedObjects& ...>;
      if constexpr(std::is_same_v<enfield::for_each, ret_type>)
      {
        return do_call_func(fnc, db.template entity_get<AttachedObjects>(data)...);
      }
      else
      {
        do_call_func(fnc, db.template entity_get<AttachedObjects>(data)...);
        return for_each::next;
      }
    }

    template<typename Func>
    static for_each call(const Func& fnc, const database_t& db, const entity_data_t& data)
    {
      using ret_type = std::invoke_result_t<Func, const AttachedObjects& ...>;
      if constexpr(std::is_same_v<enfield::for_each, ret_type>)
      {
        return do_call_func(fnc, db.template entity_get<AttachedObjects>(data)...);
      }
      else
      {
        do_call_func(fnc, db.template entity_get<AttachedObjects>(data)...);
        return for_each::next;
      }
    }
  };
}
