
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <ntools/logger/logger.hpp>

#include <enfield/enfield.hpp>
#include <enfield/concept/serializable.hpp>
#include <enfield/component/data_holder.hpp>
#include <enfield/system/system_manager.hpp>

using db_conf = neam::enfield::db_conf::eccs;

using serializable = neam::enfield::concepts::serializable<db_conf, neam::cr::persistence_backend::json>;

/// \brief Defines the "printable" concept
class printable : public neam::enfield::ecs_concept<db_conf, printable>
{
  private:
    using ecs_concept = neam::enfield::ecs_concept<db_conf, printable>;

    class concept_logic : public ecs_concept::base_concept_logic
    {
      protected:
        concept_logic(base_t *_base) : ecs_concept::base_concept_logic(_base) {}

        virtual void _do_print() const = 0;
        friend class printable;
    };

  public:
    template<typename ConceptProvider>
    class concept_provider : public concept_logic
    {
      public:
        concept_provider(base_t *_base) : concept_logic(_base) {}

      private:
        void _do_print() const final { get_base_as<ConceptProvider>().print(); }
    };

  public:
    printable(param_t p) : ecs_concept(p) {}

    void print_all() const
    {
      neam::cr::out().log(" ------ printing all ------");

      // get the list of (const) references of the concept providers.
      // The returned type is: [const] concept_logic &
      for (size_t i = 0; i < get_concept_providers_count(); ++i)
        get_concept_provider(i)._do_print();

      // could also be:
//       for_each([](const concept_logic &cp)
//       {
//         cp._do_print();
//       });
      neam::cr::out().log(" ------ ------------ ------");
    }

    // Mandatory
    friend ecs_concept;
};

/// \brief A system
class truc2;
class printable_sys : public neam::enfield::system<db_conf, printable_sys>
{
  private:
    using system = neam::enfield::system<db_conf, printable_sys>;
  public:
    printable_sys(neam::enfield::database<db_conf> &_db) : system(_db) {}

  private:
    virtual void begin() final {}

    void on_entity(printable &prt)
    {
      prt.print_all();
    }

    virtual void end() final {}

    friend system;
};

/// \brief A component + a concept provider
/// \note For most concepts, you can privately inherit from the concept::concept_provider<...> class.
class truc2 : public neam::enfield::component<db_conf, truc2>, private printable::concept_provider<truc2>, private serializable::concept_provider<truc2>
{
  private:
    using component = neam::enfield::component<db_conf, truc2>;
  public:
    truc2(param_t p)
      : component(p),
        printable::concept_provider<truc2>(this),
        serializable::concept_provider<truc2>(this)
    {
    }

    truc2(param_t p, neam::enfield::concepts::from_deserialization_t)
      : component(p),
        printable::concept_provider<truc2>(this),
        serializable::concept_provider<truc2>(this)
    {
      print("deser !");
      std::map<int, int> ret = get_persistent_data();
      for (const auto &it : ret)
      {
        neam::get_global_logger().log() << LOGGER_INFO << "  { " << it.first << ", " << it.second << " }" << std::endl;
      }
    }

    void refresh_from_deserialization()
    {
      // You should update the state of the component here
    }

    /// \brief Print a message
    /// \note the printable concept DOES NOT constrain the concept provider class to have a specific API.
    ///       all it asks is that you can do a xxx.print(); without causing a compilation error.
    void print(const std::string &hello_message = "howdy") const
    {
      neam::get_global_logger().log() << LOGGER_INFO << hello_message << ": truc2" << std::endl;
    }

    std::map<int, int> get_data_to_serialize() const
    {
      return
      {
        {42, 41},
        {21, 21}
      };
    }
};

/// \brief Another component + a concept provider
class truc : public neam::enfield::component<db_conf, truc>,
  private printable::concept_provider<truc>, private serializable::concept_provider<truc>
{
  private:
    using component = neam::enfield::component<db_conf, truc>;
  public:
    truc(param_t p)
      : component(p),
        printable::concept_provider<truc>(this),
        serializable::concept_provider<truc>(this)
    {
      require<truc2>().print("greetings from truc::truc");
    }

    truc(param_t p, neam::enfield::concepts::from_deserialization_t)
      : component(p),
        printable::concept_provider<truc>(this),
        serializable::concept_provider<truc>(this)
    {
      require<truc2>(neam::enfield::concepts::from_deserialization_t()).print("greetings from truc::truc");
      unrequire<truc2>();
    }

    std::string print() const
    {
//       get_required<truc2>().print("greetings from truc::print");

      neam::get_global_logger().log() << "hello: truc" << std::endl;

      return "truc";
    }

    int get_data_to_serialize() const { return -1; }

    void refresh_from_deserialization()
    {
      // You should update the state of the component here
    }
};


int main(int, char **)
{
  neam::get_global_logger().log_level = neam::cr::stream_logger::verbosity_level::debug;

  neam::enfield::database<db_conf> db;
  neam::enfield::system_manager<db_conf> sysmgr;

  sysmgr.add_system<printable_sys>(db);

  auto entity = db.create_entity();

  entity.add<truc>().print();

//   entity.get<truc2>()->print();
//   neam::get_global_logger().log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  neam::cr::raw_data dt;
  db.for_each([&dt, &db](serializable &s)
  {
    dt = s.serialize();
    db.break_for_each();
//     neam::get_global_logger().log() << (char *)dt.data << std::endl;
  });

  auto entity2 = db.create_entity();
  entity2.add<serializable::deserialization_marker>(&entity2, &dt);
  entity2.remove<serializable::deserialization_marker>();

  sysmgr.start_new_cycle();
  sysmgr.run_systems(db);

  entity.remove<truc>();

  // Will fail the compilation (operation not permitted):
  // entity.add<printable>();
  // entity.remove<printable>();

//   neam::get_global_logger().log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;
//   neam::get_global_logger().log() << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;

  entity.add<truc>();
  entity.add<truc2>();
  entity.remove<truc>();

//   neam::get_global_logger().log() << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;
//   neam::get_global_logger().log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  return 0;
}
