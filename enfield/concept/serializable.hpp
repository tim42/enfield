//
// created by : Timothée Feuillet
// date: 2022-1-29
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

#include <map>

#include "concept.hpp"
#include "../component/component.hpp"

#include <ntools/raw_data.hpp>
#include <ntools/struct_metadata/struct_metadata.hpp>
#include <ntools/rle/rle.hpp>


namespace neam::enfield::concepts
{
  namespace internal
  {
    struct serialized_entity
    {
      std::vector<uint64_t> externally_added_components;
      std::map<uint64_t, raw_data> serialized_components;
    };
  }

  /// \brief Define a serializable concept that uses RLE
  /// You can create an entity from a raw data by using serializable::deserialize(db, my_raw_data);
  /// You can refresh an entity (that alread have a serializable attached object) by doing : entity.get< serializable >()->refresh(entity, my_raw_data);
  ///
  /// Components can be either auto-serializable (by declaring a struct_metadata for them) or handling the deserialization themselves.
  /// Auto-serializable components will be deserialized in-place, the new data will simply be present.
  /// This also includes the refresh from a data change
  /// For auto-serializable components, a `void post_deserialize()` function can be added and it will automatically be called
  /// after a deserialization
  ///
  /// Version management is handled by neam::rle
  ///
  /// \note This concept works with conservative eccs as it does not require<>() components (this is the reason you have to pass an entity reference to some functions)
  template<typename DatabaseConf>
  class serializable : public ecs_concept<DatabaseConf, serializable<DatabaseConf>>
  {
    private:
      using ecs_concept = neam::enfield::ecs_concept<DatabaseConf, serializable<DatabaseConf>>;

      /// \brief The base logic class
      class concept_logic : public ecs_concept::base_concept_logic
      {
        protected:
          concept_logic(typename ecs_concept::base_t& _base) : ecs_concept::base_concept_logic(_base) {}

        protected:
          /// \brief Internal only, perform the serialization of the AO
          virtual raw_data _do_serialize(rle::status& st) = 0;
          virtual uint64_t _get_type_hash() const = 0;
          virtual void _do_refresh_serializable_data() = 0;
          virtual void _do_remove(entity<DatabaseConf>& entity) = 0;

        private:
          bool _is_externally_added() const { return this->get_base().is_externally_added(); }

          friend class serializable<DatabaseConf>;
      };

    public:
      /// \brief Things wanting to be serializable / deserialized should inherit from this
      /// The API contract the attached object implementing that ecs_concept must respect is:
      ///  - having a get_data_to_serialize() that does not take argument and return an object (const ref or whatever) that is de/serializable with rle.
      ///  - having a refresh_from_deserialization() method that does not take arguments (it will be called when there's new data for the attached object)
      template<typename ConceptProvider>
      class concept_provider : public concept_logic
      {
        protected:
          using serializable_t = concept_provider<ConceptProvider>;

          concept_provider(ConceptProvider& _p) : concept_logic(static_cast<typename ecs_concept::base_t&>(_p)) {}

          // API //

          /// \brief Return whether or not a persistent data is available for this attached object
          bool has_persistent_data() const
          {
            auto* ptr = this->get_concept().persistent_data;
            if (ptr)
            {
              const ConceptProvider& base = this->template get_base_as<ConceptProvider>();
              return ptr->serialized_components.end() != ptr->serialized_components.find(base.object_type_id);
            }
            return false;
          }

          /// \brief Return the deserialized data. The return type is exactly the same as get_data_to_serialize()
          /// \note If you have move assignement (either user def or default), it will be faster
          /// \note Throw an exception if called and that there is no persistent data or if that data is invalid
          /// \see assign_persistent_data()
          /// \see has_persistent_data()
          auto get_persistent_data() const requires (!metadata::concepts::StructWithMetadata<ConceptProvider>)
          {
            using data_t = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<ConceptProvider>().get_data_to_serialize())>>;
            auto* data = this->get_concept().persistent_data;
            check::debug::n_assert(data != nullptr, "get_persistent_data() called outside deserialization");

            const ConceptProvider& base = this->template get_base_as<ConceptProvider>();
            const auto& it = data->serialized_components.find(base._get_type_hash());
            check::debug::n_assert(it != data->serialized_components.end(), "get_persistent_data(): no data found for concept provider");

            rle::status rle_st = rle::status::success;
            rle::decoder dc = it->second;
            data_t ret = rle::coder<data_t>::decode(dc, rle_st);
            if (rle_st == rle::status::failure)
              check::debug::n_assert(false, "get_persistent_data(): failed to decode the data");
            return ret;
          }

        private:
          raw_data _do_serialize(rle::status& st) final override
          {
            ConceptProvider& base = this->template get_base_as<ConceptProvider>();

            if constexpr (metadata::concepts::StructWithMetadata<ConceptProvider>)
            {
              return rle::serialize(base, &st);
            }
            else
            {
              return rle::serialize(base.get_data_to_serialize(), &st);
            }
          }

          uint64_t _get_type_hash() const final override
          {
            return ct::type_hash<ConceptProvider>;
          }

          void _do_refresh_serializable_data() final override
          {
            if constexpr (metadata::concepts::StructWithMetadata<ConceptProvider>)
            {
              // We can deserialize in-place
              const internal::serialized_entity* data = this->get_concept().persistent_data;
              check::debug::n_assert(data != nullptr, "get_persistent_data() called outside deserialization");

              ConceptProvider& base = this->template get_base_as<ConceptProvider>();
              const auto& it = data->serialized_components.find(base._get_type_hash());
              check::debug::n_assert(it != data->serialized_components.end(), "get_persistent_data(): no data found for concept provider");

              rle::status rle_st = rle::in_place_deserialize(it->second, base);
              if (rle_st == rle::status::failure)
                check::debug::n_check(false, "get_persistent_data(): failed to decode the data");
            }
            else
            {
              ConceptProvider& base = this->template get_base_as<ConceptProvider>();
              base.refresh_from_deserialization();
            }
          }

          void _do_remove(entity<DatabaseConf>& entity) final override
          {
            entity.template remove<ConceptProvider>();
          }

          /// \brief Perform the require in the name of serializable<>
          static void require_concept_provider(serializable&, entity<DatabaseConf>& entity)
          {
            concept_provider<ConceptProvider>& base = entity.template add<ConceptProvider>();
            if constexpr(metadata::concepts::StructWithMetadata<ConceptProvider>)
            {
              // Handle the deserialization this way:
              base._do_refresh_serializable_data();
            }
          };

          /// \brief Only here for the automatic registration of the type
          static int _dummy_;

          // force instantiation of the static member: (and avoid a warning)
          static_assert(&_dummy_ == &_dummy_);
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
          deserialization_marker(typename ecs_concept::param_t p, entity<DatabaseConf>* _entity, const raw_data& _data)
            : component<DatabaseConf, deserialization_marker>(p), concept_logic(static_cast<typename ecs_concept::base_t&>(*this))
          {
            rle::status st = rle::status::success;

            this->get_concept().refresh(*_entity, _data, st);
          }

        private:
          raw_data _do_serialize(rle::status& /*st*/) final override
          {
            return {};
          };

          uint64_t _get_type_hash() const final override
          {
            return ct::type_hash<deserialization_marker>;
          }

          void _do_remove(entity<DatabaseConf>& ) final override
          {
            // Cannot remove as we very probably are still in the constructor
//             entity.template remove<deserialization_marker>();
          }

          void _do_refresh_serializable_data() final override {}
      };

    public:
      /// \brief standard enfield constructor
      /// \note the constructor actually performs the deserialization
      serializable(typename ecs_concept::param_t p) : ecs_concept(p)
      {
      }

      /// \brief Return the serialized data for the serializable attached objects of that entity
      raw_data serialize(rle::status& st)
      {
        internal::serialized_entity serialized_data;
        for (size_t i = 0; i < this->get_concept_providers_count(); ++i)
        {
          if (this->get_concept_provider(i)._is_externally_added())
          {
            serialized_data.externally_added_components.push_back(this->get_concept_provider(i)._get_type_hash());
          }
        }

        if (serialized_data.externally_added_components.empty())
        {
          st = rle::status::failure;
          return {};
        }

        for (size_t i = 0; i < this->get_concept_providers_count(); ++i)
        {
          serialized_data.serialized_components.emplace(this->get_concept_provider(i)._get_type_hash(), this->get_concept_provider(i)._do_serialize(st));
        }

        return rle::serialize(serialized_data, &st);
      }

      /// \brief Create a new entity and deserialize the attached objects from the raw data
      static entity<DatabaseConf> deserialize(database<DatabaseConf>& db, const raw_data& _data)
      {
        entity<DatabaseConf> entity = db.create_entity();
        entity.template add<deserialization_marker>(&entity, _data);
        entity.template remove<deserialization_marker>();
        return entity;
      }

      static void deserialize(entity<DatabaseConf>& entity, const raw_data& _data)
      {
        entity.template add<deserialization_marker>(&entity, _data);
        entity.template remove<deserialization_marker>();
      }

      /// \brief Update an entity from raw data
      /// \note attached objects that aren't present in the data_map will be removed (unless there's dependencies)
      ///       attached objects that are present in the data_map but not in the entity will be created
      void refresh(entity<DatabaseConf>& entity, const raw_data& data, rle::status& st)
      {
        deserialize(entity, rle::deserialize<internal::serialized_entity>(data, &st));
      }

    private:
      void deserialize(entity<DatabaseConf>& entity, internal::serialized_entity&& serialized_entity)
      {
        // generate the list of present attached objects
        std::map<uint64_t, concept_logic&> present_attached_objects;
        this->for_each_concept_provider([&present_attached_objects](concept_logic & prov)
        {
          present_attached_objects.emplace(prov._get_type_hash(), prov);
        });

        persistent_data = &serialized_entity;
        for (uint64_t it : serialized_entity.externally_added_components)
        {
          auto pao = present_attached_objects.find(it);
          if (pao != present_attached_objects.end())
          {
            // refresh the attached object
            pao->second._do_refresh_serializable_data();
          }
          else
          {
            // create the attached object
            auto fncit = require_map.find(it);
            if (fncit != require_map.end())
            {
              // call the require function pointer
              fncit->second(*this, entity);
            }
            else
            {
              check::debug::n_assert(false, "Unable to find the corresponding attached object");
            }
          }
        }
        persistent_data = nullptr;

        // remove extra components
        for (auto& it : present_attached_objects)
        {
          if (!serialized_entity.serialized_components.count(it.first))
          {
            it.second._do_remove(entity);
          }
        }
      }

    private:
      static inline std::map<type_t, void (*)(serializable&, entity<DatabaseConf> &)> require_map;
      // force instantiation of the static member: (and avoid a warning)
      static_assert(&require_map == &require_map);

      const internal::serialized_entity* persistent_data = nullptr;

      friend ecs_concept;
      friend class deserialization_marker;
  };

  template<typename DatabaseConf>
  template<typename ConceptProvider>
  int serializable<DatabaseConf>::concept_provider<ConceptProvider>::_dummy_ = []()
  {
    serializable::require_map.emplace(ct::type_hash<ConceptProvider>, &require_concept_provider);
    return 0;
  }();
}

N_METADATA_STRUCT(neam::enfield::concepts::internal::serialized_entity)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(externally_added_components),
    N_MEMBER_DEF(serialized_components)
  >;
};

