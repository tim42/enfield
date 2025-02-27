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

#include <deque>

#include "enfield_types.hpp"
#include "database_conf.hpp"

namespace neam::enfield
{
  /// \brief A query in the DB
  template<typename DatabaseConf, typename AttachedObject>
  class query_t
  {
    public:
      /// \brief The result of the query
      const std::deque<AttachedObject*> result = std::deque<AttachedObject*>();

      /// \brief Filter the result of the DB
      template<typename... FilterAttachedObjects>
      query_t filter(query_condition condition = query_condition::each) const
      {
        TRACY_SCOPED_ZONE;
        (static_assert_check_attached_object<DatabaseConf, FilterAttachedObjects>(), ...);
        (static_assert_can<DatabaseConf, FilterAttachedObjects, attached_object_access::db_queryable>(), ...);
        std::deque<AttachedObject*> next_result;

        for (auto* const it : result)
        {
          if (it->authorized_destruction)
            continue;

          bool ok = (condition == query_condition::each);
          if (condition == query_condition::any)
            ok = ((it->owner.template has<FilterAttachedObjects>()) || ... || ok);
          else if (condition == query_condition::each)
            ok = ((it->owner.template has<FilterAttachedObjects>()) && ...) && ok;

          if (ok)
            next_result.push_back(it);
        }

        return query_t {next_result};
      }

      /// \brief Return both the passing [1] and the failing [0] AttachedObjects
      template<typename... FilterAttachedObjects>
      std::array<query_t, 2> filter_both(query_condition condition = query_condition::each) const
      {
        TRACY_SCOPED_ZONE;
        (static_assert_check_attached_object<DatabaseConf, FilterAttachedObjects>(), ...);
        (static_assert_can<DatabaseConf, FilterAttachedObjects, attached_object_access::db_queryable>(), ...);

        std::deque<AttachedObject*> next_result_success;
        std::deque<AttachedObject*> next_result_fail;

        next_result_success.reserve(result.size());
        next_result_fail.reserve(result.size());

        for (auto*const it : result)
        {
          if (it->pending_destruction)
            continue;

          bool ok = (condition == query_condition::each);
          if (condition == query_condition::any)
            ok = ((it->owner->template has<FilterAttachedObjects>()) || ... || ok);
          else if (condition == query_condition::each)
            ok = ((it->owner->template has<FilterAttachedObjects>()) && ... && ok);

          if (ok)
            next_result_success.push_back(it);
          else
            next_result_fail.push_back(it);
        }

        return {query_t{next_result_fail}, query_t{next_result_success}};
      }
  };
}
