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

#ifndef __N_2469794491472017192_3040717291_SYSTEM_HPP__
#define __N_2469794491472017192_3040717291_SYSTEM_HPP__

#include <type_traits>

#include "base_system.hpp"

#include "../entity.hpp"
#include "../enfield_logger.hpp"
#include "../tools/demangle.hpp"
#include "../tools/function.hpp"

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
        struct _place_barrier_t;
        using place_barrier_t = _place_barrier_t *;

      public:
        virtual std::string get_system_name() const override
        {
          return neam::demangle<SystemClass>();
        }

      protected:
        static constexpr place_barrier_t place_barrier_before = nullptr;

        /// \brief constructor
        system(database<DatabaseConf> &_db)
          : base_system<DatabaseConf>(_db, type_id<SystemClass, typename DatabaseConf::system_type>::id, false)
        {
          // setup the mask
          using list = typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list::template direct_for_each<rm_rcv>;
          this->set_mask(list());
        }

        /// \brief constructor, specify that a barrier is present before this system
        /// A system barrier does not cost anything but guaranties that every entity in the DB will have
        /// completed the previous systems before going through this system
        system(database<DatabaseConf> &_db, place_barrier_t)
          : base_system<DatabaseConf>(_db, type_id<SystemClass, typename DatabaseConf::system_type>::id, true)
        {
          // setup the mask
          using list = typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list::template direct_for_each<rm_rcv>;
          this->set_mask(list());
        }

        virtual ~system() = default;

        /// \brief Tell the database to remove the given attached object
        /// The remove operation is queued and may effectively be done at a later stage,
        /// but further systems that depends on that attached object wont be run for that entity
        /// Queries performed on the database won't show that this attached object is removed
        /// \note Only usable in SystemClass::on_entity();
        template<typename AttachedObject>
        void remove(AttachedObject &ao);

        /// \brief Add an attached object to the entity
        /// further systems that depends on that attached object will be run for that entity,
        /// but queries performed on the database won't show that this attached object is added
        /// \note Only usable in SystemClass::on_entity();
        template<typename AttachedObject, typename... DataProvider>
        void add(DataProvider *...providers);

      private:
        using entity_data_t = typename entity<DatabaseConf>::data_t;
        template<typename AO>
        using id_t = type_id<AO, typename DatabaseConf::attached_object_type>;

        void run(entity_data_t *data) final override
        {
          using list = typename ct::function_traits<decltype(&SystemClass::on_entity)>::arg_list::template direct_for_each<rm_rcv>;

          run_list(data, list());
        }

        template<typename... AttachedObjects>
        void run_list(entity_data_t *data, ct::type_list<AttachedObjects...>)
        {
          entity_data = data;

          try
          {
            static_cast<SystemClass *>(this)->on_entity(*this->db.template entity_get<AttachedObjects>(data)...);
          }
#ifndef ENFIELD_NO_MESSAGES
          catch (std::exception &e)
          {
            entity_data = nullptr;
            ENFIELD_LOG(error, "system " << get_system_name() << ": on_entity(): exception caught: " << e.what() << std::endl);
          }
#endif
          catch (...)
          {
            entity_data = nullptr;
            ENFIELD_LOG(error, "system " << get_system_name() << ": on_entity(): unknown exception caught." << std::endl);
          }

          entity_data = nullptr;
        }

      private:
        entity_data_t *entity_data;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_2469794491472017192_3040717291_SYSTEM_HPP__
