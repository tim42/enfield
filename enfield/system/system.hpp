//
// file : system.hpp
// in : file:///home/tim/projects/enfield/enfield/system/system.hpp
//
// created by : Timothée Feuillet
// date: Sun Jan 08 2017 13:08:20 GMT-0500 (EST)
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

#include <type_traits>

#include "base_system.hpp"

#include "../entity.hpp"
#include <ntools/logger/logger.hpp>
#include <ntools/demangle.hpp>
#include <ntools/function.hpp>

namespace neam
{
  namespace enfield
  {
    /// \brief Defines a system (this is a CRTP class)
    /// A system class should have:
    ///  void begin();
    ///  void on_entity(... /* put here the attached objects the entity should have */ ...);
    ///  void end();
    ///
    /// Depending on the threading model, the on_entity function may be called at the same time on different entities
    /// on multiple threads, but an entity can't have more than one system doing stuff with it at a time
    ///
    /// \tparam SystemClass The class inheriting from this class
    template<typename DatabaseConf, typename SystemClass>
    class system : public base_system<DatabaseConf>
    {
      private:
        template<typename Type>
        using rm_rcv = typename std::remove_reference<typename std::remove_cv<Type>::type>::type;

      public:
        virtual std::string get_system_name() const override
        {
          return neam::demangle<SystemClass>();
        }

      protected:
        /// \brief constructor
        system(database<DatabaseConf>& _db)
          : base_system<DatabaseConf>(_db, type_id<SystemClass, typename DatabaseConf::system_type>::id())
        {
          // setup the mask
          using list = ct::list::for_each<typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list, rm_rcv>;
          this->template set_mask<list>();
        }

        virtual ~system() = default;

        /// \brief Tell the database to remove the given attached object
        /// The remove operation is queued and may effectively be done at a later stage,
        /// but further systems that depends on that attached object wont be run for that entity
        /// Queries performed on the database won't show that this attached object is removed
        /// \note Only usable in SystemClass::on_entity();
        template<typename AttachedObject>
        void remove(AttachedObject& ao);

        /// \brief Add an attached object to the entity
        /// further systems that depends on that attached object will be run for that entity,
        /// but queries performed on the database won't show that this attached object is added
        /// \note Only usable in SystemClass::on_entity();
        template<typename AttachedObject, typename... DataProvider>
        void add(DataProvider* ...providers);

      private:
        using entity_data_t = typename entity<DatabaseConf>::data_t;
        template<typename AO>
        using id_t = type_id<AO, typename DatabaseConf::attached_object_type>;

        template<typename... AttachedObjects>
        struct run_helper_t
        {
          static auto run(SystemClass& self, entity_data_t& data)
          {
            self.on_entity(*self.db.template entity_get<AttachedObjects>(data)...);
          }
        };

        void run(entity_data_t& data) final override
        {
          using list = ct::list::for_each<typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list, rm_rcv>;
          using helper = typename ct::list::extract<list>::template as<run_helper_t>;
          helper::run(*static_cast<SystemClass*>(this), data);
        }

        void init_system_for_run() final override
        {
          using list = ct::list::for_each<typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list, rm_rcv>;
          this->template compute_fewest_attached_object_id<list>();
        }
    };
  } // namespace enfield
} // namespace neam
