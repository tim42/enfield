

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

There is no flags (you can't flag an entity, a component or a concept: I don't see yet why one should have flags on entities).
Enfield supports what is called _entity patterns_ that allow the user (or something else) to pre-bake entities with specific components / specific data and quickly create/list them.

Guarantees:
 - Concepts: A concept can only exists iff one or more components implement that concept.
 - Components dependency: (both for concepts and components): A component that has not been requested by the user will be destroyed when the last concept/component
   requiring it is removed.
 - No interfaces / pure virtual methods where it is possible to do it: Enfield relies mostly on template / CRTP / specialization to work.
 - Concepts cannot be serialized, only components can. (components are the data).
 - Components must be constructible with respect of the enfield construction contract
 - Iterating over components or concepts of a given type is O(1) and cache efficient.

Good practices:
 - There should be no direct dynamic allocation in a component or a concept. (a leak in either of those and the global memory consumption will increase quite rapidly)

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
