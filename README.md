

Enfield: C++14 EWS `[Entity Whatever System]` thingy.

This includes ECS `[entity component system]`, ECCS/E2CS/EC²S `[entity component concept system]`, ...

---

This project is under development, and can't be used in any kind of project.
Yet.

---


# important notes

This is **not** a "normal" ECS system, but something more C++ friendly and a bit more generic. Enfield is an E*S with a twist:
 - Entity: Lightweight object that just contains a pointer
 - Components: are where the entity's data is and they should only contain the logic related to that data and may not depends on other components (or as little as possible).
 - Concepts: components are just data/small logic but can implement a concept. Concepts are a kind of interface toward many components implementing the concept.
   Bounding volumes, input receivers, transforms, ... are good candidates to be concepts and not components (components would be the data-provider/data-receiver for those kind of concepts)
   Usualy concepts does not hold much data, only logic and are mostly manipulated by systems
 - Systems: Manipulate large chunk of entities by using concepts or components.

There is no flags (you can't flag an entity, a component or a concept: I don't see yet why one should have flags, but there's nothing that stops you from adding flags (see the configuration section below)).

Enfield documentation extensively use the term _attached object_. An _attached object_ is anything that can be attached to an entity like components or concepts.
Likewise, an _attached object class_ is a category of attached objects that follow the same pattern. Component is one class of _attached object_, as concept is another one.


Guarantees:
 - Concepts: A concept can only exists iff one or more components implement that concept.
 - Components dependency: (both for concepts and components): A component that has not been requested by the user will be destroyed when the last concept/component
   requiring it is removed.
 - Components dependency: dependency cycles throws an exception or (if in super-release) a hard-segfault over a poisoned pointer.
 - No interfaces / pure virtual methods where it is possible to do it: Enfield relies mostly on template / CRTP / specialization to work.
 - Concepts can implement other concepts
 - Components must be constructible with respect of the enfield construction contract
 - Iterating over components or concepts of a given type is O(1) and cache efficient.
 - Fully modifiable (via the db-configuration pattern) with strong compile-time guarantees (access rights, ...).
   see `default_database_conf` in `database.hpp`. See also the section below:

Good practices:
 - There should be no direct dynamic allocation in a component or a concept. (a leak in either of those and the global memory consumption will increase quite rapidly)

# configuration

You can have multiple DB implementing different patterns (like a pure ECS, or a ECCS or whatever you want), all you have to do is providing the correct configuration to the db.

A big chunk of the configuration is the rights management. There are two classes that a configuration class must declare:
 - `class_rights`: The global usage rights of an _attached object class_
 - `specific_class_rights`: An override of `class_rights` when the corresponding _attached object class_ is used by another _attached object class_. (you can either set more or less rights for a given class)

Only `class_rights` can define "user-rights" (rights given to the public API of the entity class).
 
When an unauthorized operation is done (aka. an operation not explicitly permitted in the rights), the compilation aborts.

# systems

Enfield systems have been designed to be heavily threadable, super lightweight (without much overhead) and easy to create.

```c++
class auto_updatabe_system : public neam::enfield::system<db_conf, auto_updatabe_system>
{
  public:
    system(neam::enfield::database<db_conf> &_db)
      : neam::enfield::system<db_conf, auto_updatabe_system>(_db)
    {}

  private:
    /// \brief Called at the start of a "frame"
    virtual void begin() final {}

    /// \brief Called for every compatible entities
    void on_entity(auto_updatable &au)
    {
      au.update_all();
    }

    /// \brief Called after all the entities have been processed
    virtual void end() final {}

    friend class neam::enfield::system<db_conf, auto_updatabe_system>;
};
```

The `on_entity` function can have the signature it wants (a bit like with for_each) and the database will populate its parameters with the corresponding attached objects, skipping the entity
if there's a missing attached object. So, if you want your system to work with any entities that have both an auto_updatable and a serializable concept:

`void on_entity(auto_updatable &au, const serializable &ser)`



---

## How to build:
```
git submodule init
git submodule update

mkdir build && cd build && cmake ..
make
```

---


Made w/ <3 by Timothée Feuillet.
