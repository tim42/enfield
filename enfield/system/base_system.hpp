//
// file : system.hpp
// in : file:///home/tim/projects/enfield/enfield/system/system.hpp
//
// created by : Timothée Feuillet
// date: Sat Jan 07 2017 21:24:01 GMT-0500 (EST)
//
//
// Copyright (c) 2017 Timothée Feuillet
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

#include "../enfield_types.hpp"
#include "../type_id.hpp"
#include "../attached_object_utility.hpp"
#include <ntools/ct_list.hpp>

namespace neam
{
  namespace enfield
  {
    template<typename DatabaseConf> class system_manager;

    /// \brief Base class for a system
    /// \note A system should not inherit from base_system, but from system instead
    template<typename DatabaseConf>
    class base_system
    {
      public:
        virtual std::string get_system_name() const = 0;
        base_system(database<DatabaseConf>& _db, type_t _system_id)
          : db(_db), system_id(_system_id)
        {
        }
        virtual ~base_system() = default;

      protected:
        database<DatabaseConf>& db;

        /// \brief Called at the very beggining of the system update cycle,
        /// before any entity goes down the pipes
        virtual void begin() {}

        /// \brief Called at the very end of the system update cycle
        /// after every entity has been updated
        virtual void end() {}

        /// \brief If false, iterate over all the entities and run on those that match
        ///        If true, iterate over one of the matched attached-objects (the one with the smallest count)
        /// \note Only set this to true if you know that you only have a small proportion (< 20%) of entities matching.
        /// \warning setting this to true will yield faster execution time, but might miss recently added attached-objects (delayed ones)
        ///          and *will* skip all transient ones.
        /// \note when only matching concepts, setting this to true should (most of the time) only yield better perfs.
        ///
        /// \note some system execution modes might not respect this flag
        bool should_use_attached_object_db = false;

      private:
        using entity_data_t = typename entity<DatabaseConf>::data_t;


        /// \brief Run if the entity has the required attached objects
        void try_run(entity_data_t& data)
        {
          if (mask.match(data.mask))
            run(data);
        }

        virtual void run(entity_data_t& data) = 0;
        virtual void init_system_for_run() = 0;

        template<typename AO>
        using id_t = type_id<AO, typename DatabaseConf::attached_object_type>;
        template<typename... Types>
        using attached_object_utility_t = attached_object_utility<DatabaseConf, Types...>;

        /// \brief Set the component mask
        template<typename AttachedObjectsList>
        void set_mask()
        {
          using helper = typename ct::list::extract<AttachedObjectsList>::template as<attached_object_utility_t>;
          mask = helper::make_mask();
        }

        template<typename AttachedObjectsList>
        void compute_fewest_attached_object_id()
        {
          if constexpr (DatabaseConf::use_attached_object_db == true)
          {
            using helper = typename ct::list::extract<AttachedObjectsList>::template as<attached_object_utility_t>;
            smallest_attached_object_db = helper::get_min_entry_count(db);
          }
          else
          {
            smallest_attached_object_db = ~type_t(0);
          }
        }

      private:
        inline_mask<DatabaseConf> mask;

        const type_t system_id;
        type_t smallest_attached_object_db = ~type_t(0);

        template<typename DBC, typename SystemClass> friend class system;
        friend class system_manager<DatabaseConf>;
    };
  } // namespace enfield
} // namespace neam

