
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything (super release)
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <enfield/tools/logger/logger.hpp>

#include <enfield/enfield.hpp>

// I will use aliases to make my life easier.
namespace enfield = neam::enfield;

// The db configuration tells enfield how to create the database. Most (if not all) is done at compile-time,
// so the configured DB benefit from the same optimisations as if it were coded from the start with that configuration in mind.
//
// Here we use the conservative_eccs predefined configuration. ECCS stands for Entity-Component-Concept-System and the conservative
// is because with that configuration concepts can't require components. (a compilation error is issued).
// You can create your own configuration for your specific needs. (like tags aren't included in enfield, but if you really like them you can create a configuration
// that add them (bwah)).
using db_conf = enfield::db_conf::conservative_eccs;

// Alias for a component base class
template<typename FinalClass>
using component = enfield::base_component<db_conf, FinalClass>;

// Alias for a concept base class
template<typename FinalClass>
using concept = enfield::base_concept<db_conf, FinalClass>;

/// \brief Defines the "printable" concept
/// The contract of the printable concept is that a method [...] print([...]) const is present in the class implementing the concept,
/// and that method can be called without argument.
///
/// truc and truc2 are "printable" and thus have a print method, but both have a different signature.
class printable : public concept<printable>
{
  private:
    /// \brief concept_logic is the class that defines the communication interface of the concept provider <-> the concept.
    /// It can provide functions to manipulate the concept (otherwise inaccessible to the concept provider) and let the concept
    /// make calls/retrieve data of the concept provider.
    class concept_logic : public concept<printable>::base_concept_logic
    {
      protected:
        concept_logic(base_t *_base) : concept<printable>::base_concept_logic(_base) {}

        virtual void _do_print() const = 0;
        friend class printable;
    };

  public:
    /// \brief The concept provider is a CRTP class that will be inherited by the actual concept provider (like a component or an object managed by a component).
    /// This class has both access to the real concept provider type and the concept type, and thus should be used as a glue between both.
    template<typename ConceptProvider>
    class concept_provider : public concept_logic
    {
      public:
        concept_provider(base_t *_base) : concept_logic(_base) {}

      private:
        void _do_print() const final
        {
          // retrieve a reference to the class that inherit from this one
          // and call print() on that class.
          get_base_as<ConceptProvider>().print();
        }
    };

  public:
    // standard enfield constructor
    printable(param_t p) : concept<printable>(p) {}

    /// \brief Call print() on all printable attached objects
    void print_all() const
    {
      neam::cr::out.log() << LOGGER_INFO << " ------ printing all ------" << std::endl;

      // get the list of (const) references of the concept providers.
      // The returned type is: [const] concept_logic &
      for (size_t i = 0; i < get_concept_providers_count(); ++i)
        get_concept_provider(i)._do_print();

      // could also be:
//       for_each([](const concept_logic &cp)
//       {
//         cp._do_print();
//       });
      neam::cr::out.log() << LOGGER_INFO << " ------ ------------ ------" << std::endl;
    }

    friend concept<printable>;
};

/// \brief A component + a concept provider
/// \note For most concepts, you can privately inherit from the concept::concept_provider<...> class.
class truc2 : public component<truc2>, private printable::concept_provider<truc2>
{
  public:
    truc2(param_t p)
      : component<truc2>(p),
        printable::concept_provider<truc2>(this)
    {
    }

    /// \brief Print a message
    /// \note the printable concept DOES NOT constrain the concept provider class to have a specific API.
    ///       all it asks is that you can do a xxx.print(); without causing a compilation error.
    void print(const std::string &hello_message = "howdy") const
    {
      neam::cr::out.log() << LOGGER_INFO << hello_message << ": truc2" << std::endl;
    }
};

/// \brief Another component + a concept provider
class truc : public component<truc>, private printable::concept_provider<truc>
{
  public:
    truc(param_t p)
      : component<truc>(p),
        printable::concept_provider<truc>(this)
    {
      // require another component. (could also be any other kind of attached object a component have the right to require)
      // required attached object are guaranteed to be living until after your component is completely destructed (or the last attached object
      // requiring it), so you can safely store a pointer / reference to a required attached object, as long as the reference lifetime does not
      // exceed the lifetime of the component requiring it.
      //
      // NOTE: circular dependencies will generate exceptions (or if disabled) a segfault over a poisoned pointer
      //       but if you have a circular dependency, you may need to rethink a bit you architecture.
      // NOTE: require will take care of creating the component if it does not already exists.
      //
      // WARNING: The use of the entity public interface IS FORBIDDEN. Doing so will most likely trigger exceptions (or crashes) at some points BECAUSE THAT'S NOT A CORRECT USAGE.
      //          That is the reason you don't have a pointer to the entity. (Moreover entity can be moved around in memory, so... Good luck with that).
      require<truc2>().print("greetings from truc::truc");
    }

    std::string print() const
    {
      // If you don't want to store a reference (or can't) you can still use get_required<...>()
      // that function will check that the component is created and is effectively required, and return the reference to it.
      // This is wayy slower than storing a reference to it when you required it, but you can use it.
      //
      // There's a get_unsafe<...>() that returns a pointer (null or valid, it stills throw when it's a poisoned pointer),
      // but there's nothing stopping another thread to remove the component just after the call has completed, so the returned pointer
      // is NOT guaranteed to be valid (thus the get_unsafe<...>())
      get_required<truc2>().print("greetings from truc::print");

      neam::cr::out.log() << LOGGER_INFO << "hello: truc" << std::endl;

      return "truc";
    }
};

int main(int, char **)
{
  neam::cr::out.log_level = neam::cr::stream_logger::verbosity_level::debug;

  // create a database using the db_conf configuration
  enfield::database<db_conf> db;

  // create an entity
  auto entity = db.create_entity();

  entity.add<truc>().print();

  entity.get<truc2>()->print();
  neam::cr::out.log() << LOGGER_INFO << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  entity.get<printable>()->print_all();

  entity.remove<truc>();

  // Will fail the compilation (operation not permitted):
  // entity.add<printable>();
  // entity.remove<printable>();

  neam::cr::out.log() << LOGGER_INFO << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;
  neam::cr::out.log() << LOGGER_INFO << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;

  entity.add<truc>();
  entity.add<truc2>();
  entity.remove<truc>();

  // iterate over every printable of the DB. This operation is O(1)
  db.for_each<printable>([](printable &t)
  {
    t.print_all();
  });

  // perform a query on the db: it will return every printable whose entity has either a truc or a truc2 component
  const auto query = db.query<printable>().filter<truc2, truc>(enfield::query_condition::any);

  neam::cr::out.log() << LOGGER_INFO << query.result.size() << std::endl;

  neam::cr::out.log() << LOGGER_INFO << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;
  neam::cr::out.log() << LOGGER_INFO << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  return 0;
}
