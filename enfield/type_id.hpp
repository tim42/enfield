//
// file : component_type.hpp
// in : file:///home/tim/projects/enfield/enfield/component_type.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 15:18:08 GMT-0500 (EST)
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

#ifndef __N_3139214210286304272_324738490_COMPONENT_TYPE_HPP__
#define __N_3139214210286304272_324738490_COMPONENT_TYPE_HPP__

#include <cstdint>

#include "enfield_types.hpp"
#include "enfield_exception.hpp"

namespace neam
{
  namespace enfield
  {
    namespace internal
    {
      template<typename Class>
      class type_id_base
      {
        protected:
          /// \brief Return the next id
          static type_t get_next_id()
          {
            check::on_error::n_assert(counter != ~0u, "neam::enfield::type_id<>: more than 2^32 identifiers have been generated");
            return ++counter;
          }

        private:
          static type_t counter;
      };
      template<typename Class>
      type_t type_id_base<Class>::counter = 0;
    } // namespace internal

    /// \brief Generate a unique identifier (at runtime) for a given type.
    /// \warning Identifiers may vary from run to runs
    template<typename Type, typename Class>
    class type_id : private internal::type_id_base<Class>
    {
      public:
        /// \brief The type identifier
        static const type_t id;
    };
    template<typename Type, typename Class>
    const type_t type_id<Type, Class>::id = type_id<Type, Class>::get_next_id();

    /// \brief A generic type id (you should not use this one)
    template<typename Type>
    using generic_type_id = type_id<Type, void>;
  } // namespace enfield
} // namespace neam

#endif // __N_3139214210286304272_324738490_COMPONENT_TYPE_HPP__

