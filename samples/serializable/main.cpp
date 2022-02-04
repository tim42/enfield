
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <ntools/logger/logger.hpp>

#include <enfield/enfield.hpp>
#include <enfield/concept/serializable.hpp> // we need the serializable builtin concept
#include <enfield/concept/printable.hpp>
#include <enfield/component/name.hpp> // for the name component

using db_conf = neam::enfield::db_conf::conservative_eccs;


// Easy alias for the serializable concept. We want to serialize to JSON and use the current database conf
// NOTE: The neam backend (the default one) is WAY faster than the JSON backend and is recommended (except when you need to debug)
using serializable = neam::enfield::concepts::serializable<db_conf>;
using printable = neam::enfield::concepts::printable<db_conf>;

// create a serializable name component
// If you remove the "serializable::concept_provider", the name component will work the same, but won't be serializable.
// You can even add more concept_providers to the list if you want (like editable).
using name_component = neam::enfield::components::name<db_conf, serializable::concept_provider, printable::concept_provider>;

/// \brief A component + a concept provider
/// This is for manual handling of the serialization. See truc for automatic serialization
/// \note For most concepts, you can privately inherit from the concept::concept_provider<...> class.
class truc2 : public neam::enfield::component<db_conf, truc2>, private serializable::concept_provider<truc2>
{
  public:
    truc2(param_t p)
      : component_t(p),
        serializable_t(*this)
    {
      // unserialize the data member. This effectively performs an assignation.
      if (has_persistent_data())
      {
        refresh_from_deserialization();
      }
    }

    void refresh_from_deserialization()
    {
        data = get_persistent_data();
    }

    /// \brief Print a message
    void print(const std::string &hello_message = "howdy") const
    {
      neam::cr::out().log("{}: truc2", hello_message);
      for (const auto &it : data)
      {
        neam::cr::out().log("{{{}, {}}}", it.first, it.second);
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

/// \brief Another component + a concept provider, this time the component itself is serializable (which makes things easier)
/// The component is auto-serializable, without any input from the user
struct truc : public neam::enfield::component<db_conf, truc>,
              private serializable::concept_provider<truc>,
              private printable::concept_provider<truc>
{
  truc(param_t p)
    : component_t(p),
      serializable_t(*this),
      printable_t(*this)
  {
    // We require truc2
    require<truc2>();
  }

  // our data
  int dummy = -1;
  std::string other_dummy = "some stirng";

  friend serializable_t;
};

N_METADATA_STRUCT(truc)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(dummy),
    N_MEMBER_DEF(other_dummy)
  >;
};

int main(int, char **)
{
  neam::cr::out.min_severity = neam::cr::logger::severity::debug;
  neam::cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

  neam::enfield::database<db_conf> db;
  auto entity = db.create_entity();

  entity.add<truc>().other_dummy = "yay it works !";
  entity.add<name_component>().data = "my awesome name !";

  truc2 *t2 = entity.get<truc2>();
  t2->data =
  {
    {42, 43},
    {43, 44},
    {44, 45},
    {45, 46},
  };

  entity.get<printable>()->print();

  neam::raw_data serialized_data;
  db.for_each([&serialized_data, &db](serializable &s)
  {
    neam::rle::status st;
    serialized_data = s.serialize(st);

    return neam::enfield::for_each::stop;
  });

  // This will destroy both truc and truc2
  entity.remove<truc>();

  // // // // // // // // // // // // // // // // // // // // // //

  // create a new entity from the serialized data:
  auto entity2 = serializable::deserialize(db, serialized_data);

  neam::cr::out().log("Has truc:  {}", entity2.has<truc>());
  neam::cr::out().log("Has truc2: {}", entity2.has<truc2>());
  neam::cr::out().log("Data holder's data: {}", entity2.get<name_component>()->data);

  entity2.get<printable>()->print();

  return 0;
}
