
// #include <neam/reflective/reflective.hpp>

// #define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
// #define ENFIELD_ENABLE_DEBUG_CHECKS

#include <enfield/tools/logger/logger.hpp>

#include <enfield/enfield.hpp>

template<typename FinalClass>
using component = neam::enfield::base_component<neam::enfield::default_database_conf, FinalClass>;
template<typename FinalClass>
using concept = neam::enfield::base_concept<neam::enfield::default_database_conf, FinalClass>;

/// \brief Defines the "printable" concept
class printable : public concept<printable>
{
  private:
    class concept_logic : public concept<printable>::base_concept_logic
    {
      protected:
        concept_logic(base_t *_base) : concept<printable>::base_concept_logic(_base) {}

        virtual void _do_print() const = 0;
        friend class printable;
    };

  public:
    template<typename ConceptProvider>
    class concept_provider : public concept_logic
    {
      public:
        concept_provider(base_t *_base) : concept_logic(_base) {}
        virtual ~concept_provider() = default;

      private:
        void _do_print() const final { get_base_as<ConceptProvider>().print(); }
    };

  public:
    printable(param_t p) : concept<printable>(p) {}

    void print_all() const
    {
      neam::cr::out.log() << LOGGER_INFO << " ------ printing all ------" << std::endl;
      for (size_t i = 0; i < get_concept_providers_count(); ++i)
        get_concept_provider(i)->_do_print();

      // could also be:
//       for_each([](const concept_logic *cp)
//       {
//         cp->_do_print();
//       });
      neam::cr::out.log() << LOGGER_INFO << " ------ ------------ ------" << std::endl;
    }

    friend concept<printable>;
};


/// \brief A component + a concept provider
class truc2 : public component<truc2>, private printable::concept_provider<truc2>
{
  public:
    truc2(param_t p) : component<truc2>(p), printable::concept_provider<truc2>(this) {}

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
      : component<truc>(p), printable::concept_provider<truc>(this)
    {
      require<truc2>().print("greetings from truc::truc");
    }

    std::string print() const
    {
      get_required<truc2>().print("greetings from truc::print");

      neam::cr::out.log() << LOGGER_INFO << "hello: truc" << std::endl;

      return "truc";
    }
};


int main(int, char **)
{
  neam::cr::out.log_level = neam::cr::stream_logger::verbosity_level::debug;

  neam::enfield::database<neam::enfield::default_database_conf> db;
  auto entity = db.create_entity();

  entity.add<truc>().print();

  entity.get<truc2>()->print();
  neam::cr::out.log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  entity.get<printable>()->print_all();

  entity.remove<truc>();

  // Will fail the compilation (operation not permitted):
  // entity.add<printable>();
  // entity.remove<printable>();

  neam::cr::out.log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;
  neam::cr::out.log() << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;

  entity.add<truc>();
  entity.add<truc2>();
  entity.remove<truc>();

  neam::cr::out.log() << "has<truc2>: " << std::boolalpha << entity.has<truc2>() << std::endl;
  neam::cr::out.log() << "has<printable>: " << std::boolalpha << entity.has<printable>() << std::endl;

  return 0;
}
