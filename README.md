

Enfield: C++ EWS `[Entity Whatever System]` thingy.

This includes ECS `[entity component system]`, ECCS `[entity component concept system]`, ...

---

This project is under development.

---


# Important Notes

Enfield makes a trade-off of usability, speed and adaptability.
The current direction favor usability and adaptability, and rely only on core C++ (no additional tools/external code parsing/processing).

The core... concept... of this usability and adaptability is what is called concepts.
They can be seens as both an interface and a multiplexer over components, fixing one of the major drawbacks of pure ecs, component/system creep and poor code reuse.
Systems may operate on concepts instead of components, making the same systems capable of running over mutliple distinct components (like the auto-updatable concept present in the samples which acts a bit like Unity's MonoBehaviour Update function).

(Pure ECS is similar to plain C code, with structs being components and functions being systems, whereas concepts are closer to abstractions/C++20 concepts).

# Enfield

## Attached Objects (the Wathever in EWS)

Attached objects are objects that can be... attached... to an entity, the entity being the glue between those.
Both components and concepts are "classes" of attached objects, and more classes of attached objects can be implemented as needed.

Classes of attached objects share the same behavior (ex: concepts cannot be required or created and have a specific architecture to them).

Attached objects can require other attached objects (as allowed by the database configuration) and attached object required this way will not be destructed until no other attached object require them.

## Components

Hold data, might implement/provide a concept.

## Concepts

Abstraction of the capabilities of a set of components/concepts. (like serializable, auto-updatable, renderable, ...)
Is a CRTP abstract class + interface.

May hold a bit of logic.

## Systems

Logic. (can be seen as a transform operation on entities)

Enfield provide a system manager (or a system stack), which allow to indicate dependency between systems.
Multiple system managers can run concurrently.

There are two different mode that system managers can run their systems:
 - per entity (fastest): an entity will go through all the systems in the stack, each entities being run independently.
   Used when the systems don't have side effects on other entities, and only have an order requirement on a per-entity basis
 - per system: all the entities will go through a system, and once all of them are done they will all go through the next one, ...
   Used when systems have a dependency on another entity (like a look-at system might require that the looked-at entity has gone though the update-transform system)

Systems managers uses the neam::threading task manager (ntools) to dispatch their tasks (so a system-stack can be confined to a task-group and dependency among system-stack can be handled by having dependency between task-groups (which has a strictly constant & trivial cost in the current implementation)).

## Entities

Unique pointer to a small set of data which has ownership/control the lifespan of a set of attached objects.
Cannot be copied. (but if duplicating an entity is required, the serializable concept might help).

## Database

Where the entities and attached object live.
Are highly configurable, with some existing presets present in `enfield/databse_conf_impl.hpp`.

---

## How to build:
```
git submodule init
git submodule update

mkdir build && cd build && cmake ..
make
```

---
