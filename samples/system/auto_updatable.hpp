//
// file : autoupdate.hpp
// in : file:///home/tim/projects/enfield/samples/system/autoupdate.hpp
//
// created by : Timothée Feuillet
// date: Sun Jan 08 2017 17:12:01 GMT-0500 (EST)
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

#ifndef __N_14159230731374730825_173045086_AUTOUPDATE_HPP__
#define __N_14159230731374730825_173045086_AUTOUPDATE_HPP__

#include "enfield/enfield.hpp"

#include "conf.hpp"


namespace sample
{
  /// \brief Defines the "auto-updatable" concept + system
  class auto_updatable : public neam::enfield::ecs_concept<db_conf, auto_updatable>
  {
    private:
      using ecs_concept = neam::enfield::ecs_concept<db_conf, auto_updatable>;
      class concept_logic : public ecs_concept::base_concept_logic
      {
        protected:
          concept_logic(base_t& _base) : ecs_concept::base_concept_logic(_base) {}

          virtual void _do_update() = 0;
          friend class auto_updatable;
      };

    public:
      /// \brief The concept provider class: attached objects that want to be auto-updated inherit from this
      template<typename ConceptProvider>
      class concept_provider : public concept_logic
      {
        public:
          using auto_updatable_t = concept_provider<ConceptProvider>;

          concept_provider(ConceptProvider& _base) : concept_logic(static_cast<base_t&>(_base)) {}

        private:
          void _do_update() final { get_base_as<ConceptProvider>().update(); }
      };

      /// \brief The system the auto_updatable concept needs. Please register this in your DB
      class system : public neam::enfield::system<db_conf, system>
      {
        public:
          system(neam::enfield::database<db_conf> &_db) : neam::enfield::system<db_conf, system>(_db) {}

        private:
          virtual void begin() final {}

          /// \brief Called for every entities
          void on_entity(auto_updatable &au)
          {
            au.update_all();
          }

          virtual void end() final {}

          friend class neam::enfield::system<db_conf, system>;
      };

    public:
      auto_updatable(param_t p) : ecs_concept(p) {}

    private:
      /// \brief Called by the system to update every auto_updatable attached objects
      void update_all()
      {
        for (size_t i = 0; i < get_concept_providers_count(); ++i)
          get_concept_provider(i)._do_update();
      }

      // Mandatory
      friend ecs_concept;
  };
} // namespace sample

#endif // __N_14159230731374730825_173045086_AUTOUPDATE_HPP__

