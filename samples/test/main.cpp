
// #include <neam/reflective/reflective.hpp>

#define N_ALLOW_DEBUG true // we want full debug information
// #define N_DISABLE_CHECKS // we don't want anything
#define ENFIELD_ENABLE_DEBUG_CHECKS

#include <enfield/tools/logger/logger.hpp>

#include <enfield/enfield.hpp>


class truc2 : public neam::enfield::base_component<neam::enfield::default_database_conf, truc2>
{
  public:
    truc2(entity_t **ent) : neam::enfield::base_component<neam::enfield::default_database_conf, truc2>(ent)
    {
    }

    ~truc2() = default;

    void print() const
    {
      neam::cr::out.log() << LOGGER_INFO << "hello: truc2" << std::endl;
    }
};

class truc : public neam::enfield::base_component<neam::enfield::default_database_conf, truc>
{
  public:
    truc(entity_t **ent) : neam::enfield::base_component<neam::enfield::default_database_conf, truc>(ent)
    {
      require<truc2>().print();
    }

    ~truc() = default;

    void print() const
    {
      neam::cr::out.log() << LOGGER_INFO << "hello: truc" << std::endl;
    }
};

int main(int, char **)
{
  neam::enfield::database<neam::enfield::default_database_conf> db;
  auto entity = db.create_entity();

  entity.add<truc>().print();

  entity.get<truc>()->print();
  return 0;
}
