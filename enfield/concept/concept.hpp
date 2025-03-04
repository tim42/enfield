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

#pragma once


#include <vector>
#include <algorithm>
#include <type_traits>

#include "../attached_object/base_attached_object.hpp"
#include "../type_id.hpp"

namespace neam
{
  namespace enfield
  {
    /// \brief This is a base concept, and should be inherited by concepts
    /// \tparam DatabaseConf is the configuration object for the database
    /// \tparam ConceptType is the final concept type
    ///
    /// Concepts are a core... concept... in enfield. They provide a way to abstract components and perform one-to-many broadcast operations on compatible components in an entity.
    /// The core utility of a concept is the same core utility an abstract class may have in C++
    /// Please refer to the concepts provided in the samples on the proper way to implement / use them.
    /// \note The proper way to provide concepts is via CRTP so as to encapsulate the boilerplate in the parent class
    template<typename DatabaseConf, typename ConceptType>
    class ecs_concept : public attached_object::base_tpl<DatabaseConf, typename DatabaseConf::concept_class, ConceptType, attached_object::creation_flags::force_immediate_changes>
    {
        using parent_t = attached_object::base_tpl<DatabaseConf, typename DatabaseConf::concept_class, ConceptType, attached_object::creation_flags::force_immediate_changes>;

      public:
        using param_t = typename attached_object::base<DatabaseConf>::param_t;

        virtual ~ecs_concept() = default;

      protected:
        using base_t = attached_object::base<DatabaseConf>;

        /// \brief Implement the register / unregister thing
        class base_concept_logic
        {
          public:
            virtual ~base_concept_logic()
            {
              auto it = std::remove(ecs_concept.concept_providers.begin(), ecs_concept.concept_providers.end(), this);
              ecs_concept.concept_providers.erase(it, ecs_concept.concept_providers.end());
              if (ecs_concept.concept_providers.empty())
                ecs_concept.self_destruct();
            }

          protected:
            template<typename... Types>
            base_concept_logic(base_t& _base, Types&&... types)
              : ecs_concept(ecs_concept::create_self(_base, std::forward<Types>(types)...)), base(_base)
            {
              ecs_concept.concept_providers.push_back(this);
            }

          protected:
            /// \brief Return the base as a specific type (via static_cast)
            template<typename FinalClass>
            FinalClass& get_base_as() { return static_cast<FinalClass&>(base); }
            /// \brief Return the base as a specific type (via static_cast)
            template<typename FinalClass>
            const FinalClass& get_base_as() const { return static_cast<const FinalClass&>(base); }

            /// \brief Return the base
            base_t& get_base() { return base; }
            /// \brief Return the base
            const base_t& get_base() const { return base; }

            /// \brief Return the concept class
            ConceptType& get_concept() { return ecs_concept; }
            /// \brief Return the concept class
            const ConceptType& get_concept() const { return ecs_concept; }

          private:
            ConceptType& ecs_concept;
            base_t& base;
            friend ConceptType;
        };

      protected:
        // the default for concepts is to be fully synchrone with the db changes
        // it's a bit slow, but should allow much more transient attached_objects
        ecs_concept(param_t _param)
          : parent_t(_param)
        {
          // checks: (can't be in the class body, as the whole base_concept & ConceptType types have to be defined.

          // If there's a compilation error here, your concept class does not define a "concept_logic" class
          static_assert(sizeof(typename ConceptType::concept_logic), "Missing / bad concept_logic class in ConceptType");

          // If there's a compilation error here, your concept_logic class does not inherit from base_concept_logic
          static_assert(std::is_base_of<base_concept_logic, typename ConceptType::concept_logic>::value, "ConceptType::concept_logic does not inherit from base_concept_logic");
        }

        template<typename UnaryFunction>
        void for_each_concept_provider(UnaryFunction&& func)
        {
          for (uint32_t i = 0; i < concept_providers.size(); ++i)
            func(*static_cast<typename ConceptType::concept_logic*>(concept_providers[i]));
        }
        template<typename UnaryFunction>
        void for_each_concept_provider(UnaryFunction&& func) const
        {
          for (uint32_t i = 0; i < concept_providers.size(); ++i)
            func(*static_cast<const typename ConceptType::concept_logic*>(concept_providers[i]));
        }

        size_t get_concept_providers_count() const
        {
          return concept_providers.size();
        }

        auto get_concept_provider(size_t i) -> auto&
        {
          check::debug::n_assert(i < concept_providers.size(), "get_concept_provider: out of bound access");

          return *static_cast<typename ConceptType::concept_logic*>(concept_providers[i]);
        }
        auto get_concept_provider(size_t i) const -> const auto&
        {
          check::debug::n_assert(i < concept_providers.size(), "get_concept_provider: out of bound access");

          return *static_cast<const typename ConceptType::concept_logic*>(concept_providers[i]);
        }

      private:
        std::mtc_vector<base_concept_logic*> concept_providers;
    };
  } // namespace enfield
} // namespace neam



