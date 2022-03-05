

Enfield: C++ EWS `[Entity Whatever System]` thingy.

This includes ECS `[entity component system]`, ECCS/E2CS/EC²S `[entity component concept system]`, ...

---

This project is under development, and might not be usable in most projects.
Yet.

---


# important notes

Enfield makes a trade-off of usability, speed and adaptability. The main interesting specificity is something named "concepts".

Enfield operates on entities and what is called "attached-objects", which are objects that can be attached to an entity (like components or concepts).
Rules governing the behavior of a category of such attached objects is governed by the database configuration (see `database_conf_impl.hpp` and the section below). Configuration allows for specific behaviors, most of which are enforced at compile-time.

The current existing categories of attached-objects are:
 - **components**: Like normal ECS components, mainly holds data. They can be added and removed, and can have dependency to other components.
 - **concepts**: A more generic view on components. Cannot be added or removed (they are automatically added and removed as components implementing them are added or removed), but are used to represent and abstract a higher level... concept. Such concepts can be "serializable", "updatable", "renderable", ... They bridge the gap between systems and components, avoiding making system too specific and working only with a set of known components (which is the problem almost all current ECS implementations have).

Systems are also presents and have multiple modes of multi-threading support, depending on what is required. Support for the ntools' task_manager is also present.


Guarantees:
 - Concepts: A concept can only exists iff one or more components implement that concept.
 - Components dependency: (both for concepts and components): A component that has not been requested by the user will be destroyed when the last concept/component
   requiring it is removed.
 - Components dependency: dependency cycles throws an exception or (if in super-release) a hard-segfault over a poisoned pointer.
 - Concepts can implement other concepts
 - Components must be constructible with respect of the enfield construction contract
 - Iterating over components or concepts of a given type is O(1) (for the next component/entity) and cache efficient.
   - Iterating over entities that matches a set of component is less cache friendly and a bit less efficient (worst case is trying to match entities with two components where no entities have both components but entities with only one of them are in very big quantity)
 - Fully modifiable (via the db-configuration pattern) with strong compile-time guarantees (access rights, ...).
   see `database_conf_impl.hpp`. See also the section below:

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
