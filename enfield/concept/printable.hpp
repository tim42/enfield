//
// created by : Timothée Feuillet
// date: 2022-1-30
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

#include "concept.hpp"
#include "../component/component.hpp"

#include <ntools/struct_metadata/struct_metadata.hpp>
#include <ntools/struct_metadata/fmt_support.hpp>
#include <ntools/ct_list.hpp>
#include <ntools/type_id.hpp>
#include <ntools/log_type.hpp>

namespace neam::enfield::concepts
{
  /// \brief Simple printable concept. Compatible with auto-serializable components
  /// \note use neam::cr::log_type for its purpose.
  /// Extending with specific type support is done via inheriting neam::cr::log_type::helpers::auto_register and implementing the api.
  ///
  /// \note the actual logging is done via serialization + metadata. (it is quite slow, as it's a three-steps process).
  template<typename DatabaseConf>
  class printable : public ecs_concept<DatabaseConf, printable<DatabaseConf>>
  {
    private:
      using ecs_concept = neam::enfield::ecs_concept<DatabaseConf, printable<DatabaseConf>>;

      /// \brief The base logic class
      class concept_logic : public ecs_concept::base_concept_logic
      {
        protected:
          concept_logic(typename ecs_concept::base_t& _base) : ecs_concept::base_concept_logic(_base) {}

        protected:
          virtual void do_print() const = 0;

        private:
          friend class printable<DatabaseConf>;
      };

    public:
      template<typename ConceptProvider>
      class concept_provider : public concept_logic
      {
        protected:
          using printable_t = concept_provider<ConceptProvider>;

          concept_provider(ConceptProvider& _p) : concept_logic(static_cast<typename ecs_concept::base_t&>(_p)) {}

        protected:
          // Can be overriden in the child class to have custom handling of debug information
          void do_print() const override
          {
            const ConceptProvider& base = this->template get_base_as<ConceptProvider>();
            print_type(base);
          }

          static void print_type(const ConceptProvider& v)
          {
            cr::log_type::log_type<ConceptProvider>(v);
          }

        private:
          template<typename MT, size_t Offset>
          static const MT& member_at(const ConceptProvider& v)
          {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
            return *reinterpret_cast<const MT*>(ptr + Offset);
          }

          template<typename T>
          static std::string format_type()
          {
            std::string type_name = ct::type_name<T>.str;
            size_t off = type_name.find('<');
            if (off != std::string::npos)
            {
              type_name.resize(off);
            }
            return type_name;
          }
      };

    public:
      printable(typename ecs_concept::param_t p) : ecs_concept(p)
      {
      }

      void print() const
      {
        neam::cr::out().log(" ------ entity ------");
        for (size_t i = 0; i < this->get_concept_providers_count(); ++i)
          this->get_concept_provider(i).do_print();
        neam::cr::out().log(" ------ ------ ------");
      }

      friend ecs_concept;
  };
}

