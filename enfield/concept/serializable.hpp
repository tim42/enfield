//
// file : serializable.hpp
// in : file:///home/tim/projects/enfield/enfield/concept/serializable.hpp
//
// created by : Timothée Feuillet
// date: Fri Dec 30 2016 15:35:35 GMT-0500 (EST)
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

#ifndef __N_21106213692142628973_2997911515_SERIALIZABLE_HPP__
#define __N_21106213692142628973_2997911515_SERIALIZABLE_HPP__

#include <map>
#include <vector>
#include <string>

#include "concept.hpp"
#include "../enfield_reflective.hpp"
#include "../enfield_exception.hpp"

#include "../persistence/persistence/persistence.hpp" // for persistence
#include "../persistence/persistence/stl/map.hpp"     // for persistence/std::map<>
#include "../persistence/persistence/stl/vector.hpp"     // for persistence/std::vector<>
#include "../persistence/persistence/stl/string.hpp"     // for persistence/std::string<>

#include "../tools/uninitialized.hpp"

namespace neam
{
  namespace enfield
  {
    namespace concepts
    {
      struct _from_deserialization {};

      /// \brief Indicate that the constructor is called from a deserialized object
      /// Usage: from_deserialization_t().
      using from_deserialization_t = _from_deserialization *;

      /// \brief Define a serializable concept that uses persistence
      template<typename DatabaseConf, typename Backend = cr::persistence_backend::neam>
      class serializable : public concept<DatabaseConf, serializable<DatabaseConf, Backend>>
      {
        private:
          using concept = neam::enfield::concept<DatabaseConf, serializable<DatabaseConf, Backend>>;

          using vct_t = typename std::conditional<std::is_same<Backend, cr::persistence_backend::json>::value, std::string, std::vector<uint8_t>>::type;
          using data_map_t = std::pair<std::vector<type_t>, std::map<type_t, vct_t>>;

          /// \brief Represent a serialized attached object
          struct serialized_ao
          {
            type_t component_type_id;   // the id of the component to add
            vct_t data;  // the data of that component
          };

          /// \brief The base logic class
          class concept_logic : public concept::base_concept_logic
          {
            protected:
              concept_logic(typename concept::base_t *_base) : concept::base_concept_logic(_base) {}

            protected:
              /// \brief Internal only, perform the serialization of the AO
              virtual serialized_ao _do_serialize() = 0;

            private:
              bool _is_user_added() const
              {
                return this->template get_base_as<attached_object::base<DatabaseConf>>().is_user_added();
              }

              friend class serializable<DatabaseConf, Backend>;
          };

        public:
          /// \brief Things wanting to be serializable / deserialized should inherit from this
          /// The API contract the attached object implementing that concept must respect is:
          ///  - having a get_data_to_serialize() that does not take argument and return an object (const ref or whatever) that is de/serializable with persistence.
          ///  - have a constructor that takes a from_deserialization_t argument
          template<typename ConceptProvider>
          class concept_provider : public concept_logic
          {
            protected:
              concept_provider(typename concept::base_t *_base) : concept_logic(_base) {}

              // API //

              /// \brief Return whether or not a persistent data is available for this attached object
              bool has_persistent_data() const
              {
                auto *ptr = this->get_concept().persistent_data;
                if (ptr)
                {
                  const ConceptProvider &base = this->template get_base_as<ConceptProvider>();
                  return ptr->second.end() != ptr->second.find(base.object_type_id);
                }
                return false;
              }

              /// \brief Return the deserialized data. The return type is exactly the same as get_data_to_serialize()
              /// \note If you have move assignement (either user def or default), it will be faster
              /// \note Throw an exception if called and that there is no persistent data or if that data is invalid
              /// \see assign_persistent_data()
              /// \see has_persistent_data()
              auto get_persistent_data() const -> auto
              {
                auto *ptr = this->get_concept().persistent_data;
                if (ptr)
                {
                  const ConceptProvider &base = this->template get_base_as<ConceptProvider>();
                  const auto &it = ptr->second.find(base.object_type_id);
                  if (it != ptr->second.end())
                  {
                    using data_t = typename std::remove_reference<typename std::remove_cv<decltype(((ConceptProvider*)(nullptr))->get_data_to_serialize())>::type>::type;
                    cr::uninitialized<data_t> data;
                    if (cr::persistence::deserialize<Backend, data_t>(cr::raw_data(it->second.size(), (int8_t *)it->second.data()), &data))
                    {
                      data.call_destructor(true);
                      // return a temporary
                      return data_t(std::move(static_cast<data_t &>(data)));
                    }
                    throw exception_tpl<concept_provider>("get_persistent_data() invalid data", __FILE__, __LINE__);
                  }
                  throw exception_tpl<concept_provider>("get_persistent_data() no data found for that concept provider", __FILE__, __LINE__);
                }
                throw exception_tpl<concept_provider>("get_persistent_data() called while not deserializing", __FILE__, __LINE__);
              }

              /// \brief Assign the persistent data to an already constructed object
              /// \note If you have move assignement (either user def or default), it will be faster
              /// \note Throw an exception if called and that there is no persistent data or if that data is invalid
              /// \see get_persistent_data()
              /// \see has_persistent_data()
              template<typename Type>
              void assign_persistent_data(Type &&type) const
              {
                type = std::move(get_persistent_data());
              }

            private:
              virtual serialized_ao _do_serialize() final
              {
                ConceptProvider &base = this->template get_base_as<ConceptProvider>();
                cr::raw_data dt = cr::persistence::serialize<Backend>(base.get_data_to_serialize());
                return
                {
                  base.object_type_id,
                  vct_t(dt.data, dt.data + dt.size + _dummy_)
                };
              }

              /// \brief Perform the require in the name of serializable<>
              static void require_concept_provider(serializable &, entity<DatabaseConf> &entity)
              {
                entity.template add<ConceptProvider>(from_deserialization_t());
              };

              /// \brief Only here for the automatic registration of the type
              static int _dummy_;
          };

          /// \brief Mark the entity to be deserialized
          /// Usage:
          /// \code
          /// entity.add<serializable::deserialization_marker>([neam::cr::raw_data *]);
          /// entity.remove<serializable::deserialization_marker>();
          /// \endcode
          class deserialization_marker : public component<DatabaseConf, deserialization_marker>, public concept_logic
          {
            public:
              deserialization_marker(typename concept::param_t p, entity<DatabaseConf> *_entity, const cr::raw_data *_data)
                : component<DatabaseConf, deserialization_marker>(p), concept_logic(this)
              {
                // Let the deserialization happen:
                cr::uninitialized<data_map_t> data;
                if (cr::persistence::deserialize<Backend, data_map_t>(*_data, &data))
                {
                  data.call_destructor(true);
                  this->get_concept().deserialize(*_entity, data);
                }
                else
                {
                  throw exception_tpl<deserialization_marker>("deserialization_marker() Invalid data", __FILE__, __LINE__);
                }
              }

            private:
              virtual serialized_ao _do_serialize() final
              {
                return
                {
                  ~0u,
                  vct_t()
                };
              };
          };

        public:
          /// \brief standard enfield constructor
          /// \note the constructor actually performs the deserialization
          serializable(typename concept::param_t p) : concept(p)
          {
          }

          /// \brief Return the serialized data for the serializable attached objects of that entity
          cr::raw_data serialize()
          {
            data_map_t data_map;

            for (size_t i = 0; i < this->get_concept_providers_count(); ++i)
            {
              serialized_ao data = this->get_concept_provider(i)._do_serialize();

              if (data.data.size() > 0)
              {
                data_map.second.emplace(data.component_type_id, std::move(data.data));

                if (this->get_concept_provider(i)._is_user_added())
                  data_map.first.push_back(data.component_type_id);
              }
            }

            check::on_error::n_assert(!data_map.first.empty(), "No user-added attached object have been found: deserialization won't work.");

            return cr::persistence::serialize<Backend>(data_map);
          }

        private:
          void deserialize(entity<DatabaseConf> &entity, const data_map_t &data_map)
          {
            // We somehow got our data_map, all we need is require components. There constructor will take care of everything else.
            persistent_data = &data_map;
            try
            {
              for (type_t it : data_map.first)
              {
                auto fncit = require_map.find(it);
                if (fncit != require_map.end())
                {
                  // call the require function pointer
                  fncit->second(*this, entity);
                }
                else
                {
                  throw exception_tpl<serializable>("deserialize(): Unable to find the corresponding attached object", __FILE__, __LINE__);
                }
              }
            }
            catch (...)
            {
              persistent_data = nullptr;
              throw;
            }
            persistent_data = nullptr;
          }

          static std::map<type_t, void (*)(serializable &, entity<DatabaseConf> &)> require_map;
          const data_map_t *persistent_data = nullptr;

          friend concept;
          friend class deserialization_marker;
      };

      template<typename DatabaseConf, typename Backend>
      std::map<type_t, void (*)(serializable<DatabaseConf, Backend> &, entity<DatabaseConf> &)> serializable<DatabaseConf, Backend>::require_map =
        std::map<type_t, void (*)(serializable<DatabaseConf, Backend> &, entity<DatabaseConf> &)>();

      template<typename DatabaseConf, typename Backend>
      template<typename ConceptProvider>
      int serializable<DatabaseConf, Backend>::concept_provider<ConceptProvider>::_dummy_ = []()
      {
        serializable::require_map.emplace(type_id<ConceptProvider, typename DatabaseConf::attached_object_type>::id, &require_concept_provider);
        return 0;
      }();
    } // namespace concepts
  } // namespace enfield
} // namespace neam

#endif // __N_21106213692142628973_2997911515_SERIALIZABLE_HPP__

