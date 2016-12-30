//
// file : concept.hpp
// in : file:///home/tim/projects/enfield/enfield/concept/concept.hpp
//
// created by : Timothée Feuillet
// date: Thu Dec 29 2016 21:12:28 GMT-0500 (EST)
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

#ifndef __N_473723811246623694_2897013755_CONCEPT_HPP__
#define __N_473723811246623694_2897013755_CONCEPT_HPP__

#include <vector>
#include <algorithm>
#include <type_traits>

#include "../base_attached_object.hpp"
#include "../type_id.hpp"
#include "../enfield_exception.hpp"

namespace neam
{
  namespace enfield
  {
    /// \brief This is a base concept, and should be inherited by concepts
    /// \tparam DatabaseConf is the configuration object for the database
    /// \tparam ConceptType is the final concept type
    ///
    /// Concepts are a powerfull yet not-trivial thing to understand. There are concepts in the samples,
    /// but here is a small explanation of the concept pattern (and what enfield asks):
    /// your concept class (say my_concept) inherit from base_concept,
    /// my_concept have to declare/define:
    ///   - a class concept_logic that inherit from base_concept<...>::base_concept_logic
    ///   - a templated class concept_provider (CRTP) which inherit from your concept_logic class
    /// The concept_logic class defines the communication interface from the concept provider (the class that inherit from your concept_provider class) and my_concept,
    /// it should not be used to constrain the concept provider to have a specific API as your concept_provider knows the exact type of the concept provider. (use get_base_as < ConceptProvider > ())
    template<typename DatabaseConf, typename ConceptType>
    class base_concept : public attached_object::base_tpl<DatabaseConf, typename DatabaseConf::concept_class, ConceptType>
    {
      public:
        using param_t = typename attached_object::base<DatabaseConf>::param_t;

        virtual ~base_concept() = default;

      protected:
        using base_t = attached_object::base<DatabaseConf>;

        /// \brief Implement the register / unregister thing
        class base_concept_logic
        {
          public:
            virtual ~base_concept_logic()
            {
              auto it = std::remove(concept.concept_providers.begin(), concept.concept_providers.end(), this);
              concept.concept_providers.erase(it, concept.concept_providers.end());
              if (concept.concept_providers.empty())
                concept.commit_suicide();
            }

          protected:
            base_concept_logic(base_t *_base)
             : concept(base_concept::create_self(*_base)), base(_base)
            {
              concept.concept_providers.push_back(this);
            }

          protected:
            /// \brief Return the base as a specific type (via static_cast)
            template<typename FinalClass>
            FinalClass &get_base_as() { return *static_cast<FinalClass *>(base); }
            /// \brief Return the base as a specific type (via static_cast)
            template<typename FinalClass>
            const FinalClass &get_base_as() const { return *static_cast<const FinalClass *>(base); }

            /// \brief Return the concept class
            ConceptType &get_concept() { return concept; }
            /// \brief Return the concept class
            const ConceptType &get_concept() const { return concept; }

          private:
            ConceptType &concept;
            base_t *base;
        };

      protected:
        base_concept(param_t _param)
          : attached_object::base_tpl<DatabaseConf, typename DatabaseConf::concept_class, ConceptType>
          (
            _param,
            type_id<ConceptType, typename DatabaseConf::attached_object_type>::id
          )
        {
          // checks: (can't be in the class body, as the whole base_concept & ConceptType types have to be defined.

          // If there's a compilation error here, your concept class does not define a "concept_logic" class
          static_assert(sizeof(typename ConceptType::concept_logic), "Missing / bad concept_logic class in ConceptType");

          // If there's a compilation error here, your concept_logic class does not inherit from base_concept_logic
          static_assert(std::is_base_of<base_concept_logic, typename ConceptType::concept_logic>::value, "ConceptType::concept_logic does not inherit from base_concept_logic");
        }

        template<typename UnaryFunction>
        void for_each_concept_provider(const UnaryFunction &func)
        {
          for (base_concept_logic *it : concept_providers)
            func(static_cast<typename ConceptType::concept_logic *>(it));
        }
        template<typename UnaryFunction>
        void for_each_concept_provider(const UnaryFunction &func) const
        {
          for (const base_concept_logic *it : concept_providers)
            func(static_cast<const typename ConceptType::concept_logic *>(it));
        }

        size_t get_concept_providers_count() const
        {
          return concept_providers.size();
        }

        auto get_concept_provider(size_t i) -> auto
        {
          return static_cast<typename ConceptType::concept_logic *>(concept_providers[i]);
        }
        auto get_concept_provider(size_t i) const -> auto
        {
          return static_cast<const typename ConceptType::concept_logic *>(concept_providers[i]);
        }

        auto _get_concept_providers() -> auto
        {
          return reinterpret_cast<typename ConceptType::concept_logic **>(concept_providers.data());
        }
        auto _get_concept_providers() const -> auto
        {
          return reinterpret_cast<const typename ConceptType::concept_logic *const*>(concept_providers.data());
        }

      private:
        std::vector<base_concept_logic *> concept_providers;
    };
  } // namespace enfield
} // namespace neam

#endif // __N_473723811246623694_2897013755_CONCEPT_HPP__

