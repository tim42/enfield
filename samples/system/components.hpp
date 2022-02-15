//
// file : components.hpp
// in : file:///home/tim/projects/enfield/samples/system/components.hpp
//
// created by : Timothée Feuillet
// date: Sun Jan 08 2017 17:27:23 GMT-0500 (EST)
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

#ifndef __N_7721205361892716324_793230972_COMPONENTS_HPP__
#define __N_7721205361892716324_793230972_COMPONENTS_HPP__

#include "enfield/enfield.hpp"

#include "conf.hpp"
#include "auto_updatable.hpp"

namespace sample
{
  /// \brief Deadly stupid and simple component that does nothing
  class comp_1 : public neam::enfield::component<sample::db_conf, comp_1>
  {
    public:
      comp_1(param_t p) : component_t(p)
      {
      }

      uint64_t data = reinterpret_cast<uint64_t>(this);
  };

  class comp_1b : public neam::enfield::component<sample::db_conf, comp_1b>
  {
    public:
      comp_1b(param_t p) : component_t(p)
      {
      }
  };

  class comp_3 : public neam::enfield::component<sample::db_conf, comp_3>,
    private auto_updatable::concept_provider<comp_3>
  {
    public:
      comp_3(param_t p)
        : component_t(p),
          auto_updatable_t(*this)
      {
      }

    private:
      void update()
      {
      }

      friend auto_updatable_t;
  };

  /// \brief Deadly stupid and simple component that is auto-updatable
  class comp_2 : public neam::enfield::component<sample::db_conf, comp_2>,
    private auto_updatable::concept_provider<comp_2>
  {
    public:
      comp_2(param_t p)
        : component_t(p),
          auto_updatable_t(*this)
      {
      }

    private:
      void update()
      {
        // some bit of math:
        comp.data += comp.data * comp.data | 5;

        if ((comp.data & (1 << 23)))
        {
          if (is_required<comp_1b>())
            unrequire<comp_1b>();
          else if (!is_required<comp_1b>())
            require<comp_1b>();
        }

        if (is_required<comp_3>())
          unrequire<comp_3>();
        else
          require<comp_3>();
      }

      comp_1 &comp = require<comp_1>();
      friend auto_updatable_t;
  };
} // namespace sample

#endif // __N_7721205361892716324_793230972_COMPONENTS_HPP__

