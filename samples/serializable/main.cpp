
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <enfield/tools/logger/logger.hpp>

#include <enfield/enfield.hpp>
#include <enfield/concept/serializable.hpp> // we need the serializable builtin concept

using db_conf = neam::enfield::db_conf::conservative_eccs;


// Easy alias for the serializable concept. We want to serialize to JSON and use the current database conf
// NOTE: The neam backend (the default one) is WAY faster than the JSON backend and is recommended (except when you need to debug)
using serializable = neam::enfield::concepts::serializable<db_conf, neam::cr::persistence_backend::json>;

/// \brief A component + a concept provider
/// \note For most concepts, you can privately inherit from the concept::concept_provider<...> class.
class truc2 : public neam::enfield::component<db_conf, truc2>, private serializable::concept_provider<truc2>
{
  private:
    using component = neam::enfield::component<db_conf, truc2>;
  public:
    truc2(param_t p)
      : component(p),
        serializable::concept_provider<truc2>(this)
    {
    }

    truc2(param_t p, neam::enfield::concepts::from_deserialization_t)
      : component(p),
        serializable::concept_provider<truc2>(this)
    {
      // unserialize the data member. This effectively performs an assignation.
      if (has_persistent_data())
      {
        data = get_persistent_data();
        //assign_persistent_data(data);
      }

      print("deserialized !");
    }

    /// \brief Print a message
    void print(const std::string &hello_message = "howdy") const
    {
      neam::cr::out.log() << LOGGER_INFO << hello_message << ": truc2" << std::endl;
      for (const auto &it : data)
      {
        neam::cr::out.log() << LOGGER_INFO << "  { " << it.first << ", " << it.second << " }" << std::endl;
      }
    }

    std::map<int, int> data;

  private: // keep the public interface clean
    /// \brief Return the data. Can be anything that can be serialized by persistence.
    const std::map<int, int> &get_data_to_serialize() const
    {
      return data;
    }

    friend serializable::concept_provider<truc2>;
};

/// \brief Another component + a concept provider
class truc : public neam::enfield::component<db_conf, truc>, private serializable::concept_provider<truc>
{
  private:
    using component = neam::enfield::component<db_conf, truc>;
  public:
    truc(param_t p)
      : component(p),
        serializable::concept_provider<truc>(this)
    {
      // We require truc2
      require<truc2>();
    }

    truc(param_t p, neam::enfield::concepts::from_deserialization_t t)
      : component(p),
        serializable::concept_provider<truc>(this)
    {
      // unserialize data
      if (has_persistent_data())
        assign_persistent_data(dummy);

      // We require truc2, but by convention we have to forward the same arguments
      // (else truc won't know that it can be deserialized)
      require<truc2>(t);
    }

    std::string print() const
    {
      get_required<truc2>().print("greetings from truc::print");

      neam::cr::out.log() << LOGGER_INFO << "hello: truc: data: " << dummy << std::endl;

      return "truc";
    }

  private:
    /// \brief Return the data. Can be anything that can be serialized by persistence.
    int get_data_to_serialize() const { return dummy; }

    // our data
    int dummy = -1;

    friend serializable::concept_provider<truc>;
};

int main(int, char **)
{
  neam::cr::out.log_level = neam::cr::stream_logger::verbosity_level::debug;

  neam::enfield::database<db_conf> db;
  auto entity = db.create_entity();

  entity.add<truc>();

  truc2 *t2 = entity.get<truc2>();
  t2->data =
  {
    {42, 43},
    {43, 44},
    {44, 45},
    {45, 46},
  };
  t2->print("before serialization");

  neam::cr::raw_data serialized_data;
  db.for_each([&serialized_data, &db](serializable &s)
  {
    serialized_data = s.serialize();
    neam::cr::out.log() << LOGGER_INFO << "serialized data:" << std::endl;
    neam::cr::out.log() << LOGGER_INFO << (char *)serialized_data.data << std::endl;
    db.break_for_each();
  });

  // This will destroy both truc and truc2
  entity.remove<truc>();

  // // // // // // // // // // // // // // // // // // // // // //

  // create a new entity
  auto entity2 = db.create_entity();

  // this is how an entity is marked for deserialization. entity2 and serialized_data
  // only need to be valid while the add<serializable::deserialization_marker> hasn't returned
  //
  // The entity is deserialized when the add<serializable::deserialization_marker> has returned,
  // but has serializable::deserialization_marker can't be automatically removed it is a good practice to remove it
  // right after the deserialization is done
  entity2.add<serializable::deserialization_marker>(&entity2, &serialized_data);
  entity2.remove<serializable::deserialization_marker>(); // cleanup

  neam::cr::out.log() << LOGGER_INFO << "has<truc>: " << std::boolalpha << entity2.has<truc>() << std::endl;
  neam::cr::out.log() << LOGGER_INFO << "has<truc2>: " << std::boolalpha << entity2.has<truc2>() << std::endl;

  return 0;
}
