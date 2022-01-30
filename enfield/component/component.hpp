//
// file : component.hpp
// in : file:///home/tim/projects/enfield/enfield/component/component.hpp
//
// created by : Timothée Feuillet
// date: Mon Dec 26 2016 15:04:07 GMT-0500 (EST)
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

#ifndef __N_19625634529221301_2572711997_COMPONENT_HPP__
#define __N_19625634529221301_2572711997_COMPONENT_HPP__

#include "../base_attached_object.hpp"
#include "../type_id.hpp"

namespace neam
{
  namespace enfield
  {
    /// \brief This is a base component, and should be inherited by components
    /// \tparam DatabaseConf is the configuration object for the database
    /// \tparam ComponentType is the final component type
    template<typename DatabaseConf, typename ComponentType>
    class component : public attached_object::base_tpl<DatabaseConf, typename DatabaseConf::component_class, ComponentType>
    {
      public:
        using param_t = typename attached_object::base<DatabaseConf>::param_t;
        using component_t  = component<DatabaseConf, ComponentType>;

        virtual ~component() = default;

      protected:
        component(param_t _param)
          : attached_object::base_tpl<DatabaseConf, typename DatabaseConf::component_class, ComponentType>
            (
              _param,
              type_id<ComponentType, typename DatabaseConf::attached_object_type>::id
            )
        {
        }
    };
  } // namespace enfield
} // namespace neam

#endif // __N_19625634529221301_2572711997_COMPONENT_HPP__

