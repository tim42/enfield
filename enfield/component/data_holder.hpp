//
// file : data_holder.hpp
// in : file:///home/tim/projects/enfield/enfield/component/data_holder.hpp
//
// created by : Timothée Feuillet
// date: Sat Mar 04 2017 16:10:55 GMT-0500 (EST)
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

#ifndef __N_12776153593207818043_2352923957_DATA_HOLDER_HPP__
#define __N_12776153593207818043_2352923957_DATA_HOLDER_HPP__

#include "component.hpp"
#include "../concept/serializable.hpp"

namespace neam
{
  namespace enfield
  {
    namespace components
    {
      /// \brief Component that hold some data
      /// \note You can make this component inherit from any concept provider (like serializable, ...)
      ///       as long as it does not require specific arguments in the constructor.
      /// \note I may recomend you to use an alias of that class, as you can then use that alias to perform queries
      /// \tparam Data is the type of the data struct to hold as a component
      template<typename DatabaseConf, typename Data, template<typename X> class... ConceptProviders>
      class data_holder final : public neam::enfield::component<DatabaseConf, data_holder<DatabaseConf, Data, ConceptProviders...>>,
                                private ConceptProviders<data_holder<DatabaseConf, Data, ConceptProviders...>>...
      {
        private:
          using component = neam::enfield::component<DatabaseConf, data_holder<DatabaseConf, Data, ConceptProviders...>>;
        public:
          data_holder(typename component::param_t p)
            : component(p),
              ConceptProviders<data_holder<DatabaseConf, Data, ConceptProviders...>>(this)...
          {
          }

          /// \brief If you specify a data_t* (or Data*) argument to the constructor, it will be used to initialize the data member
          data_holder(typename component::param_t p, Data *data_ptr)
            : component(p),
              ConceptProviders<data_holder<DatabaseConf, Data, ConceptProviders...>>(this)...,
              data(*data_ptr)
          {
          }

          using data_t = Data;
          data_t data;

        private: // serializable
          data_holder(typename component::param_t p, concepts::from_deserialization_t)
            : component(p),
              ConceptProviders<data_holder<DatabaseConf, Data, ConceptProviders...>>(this)...,
              data(this->get_persistent_data())
          {
            // perform the unserialization of name. The compilation won't fail as the template is lazilly evaluated
            // and this constructor, being private and thus never called should not be evaluated by the compiler.
          }

          const data_t &get_data_to_serialize() const
          {
            return data;
          }

          void refresh_from_deserialization()
          {
            data = this->get_persistent_data();
          }

          template<typename DBC, typename THING> friend class concepts::serializable;
          template<typename DBC> friend class neam::enfield::database;
      };
    } // namespace components
  } // namespace enfield
} // namespace neam

#endif // __N_12776153593207818043_2352923957_DATA_HOLDER_HPP__

