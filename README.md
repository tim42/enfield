

enfield: C++14 EC*S thingy


This project is under development, and can't be used in any kind of project.
Yet.


# important notes

This is **not** a "normal" ECS system, but something more C++ friendly and a bit more generic. Enfield is an EC*S with a twist:
 - Entity: Lightweight object that just contains a list of reference to components and views
 - Components: are where the entity's data is and they should only contain the logic related to that data and may not depends on other components (or as little as possible).
 - Views: components are just data/small logic but can implement a concept. Views are a kind of interface toward many components implementing the concept.
   Bounding volumes, input receivers, transforms, ... are good candidates to be views and not components (components would be the data-provider/data-receiver for those kind of views)
   Usualy views does not hold much data, only logic and are mostly manipulated by systems
 - Systems: Manipulate large chunk of entities by using views or components.

There is no flags (you can't flag an entity, a component or a view: I don't see yet why one should have flags on entities).
Enfield supports what is called _entity patterns_ that allow the user (or something else) to pre-bake entities with specific components / specific data and quickly create/list them.

Guarantees:
 - Views: A view can only exists iff one or more components implement the concept of that view.
 - Components dependency: (both for views and components): A component that has not been requested by the user will be destroyed when the last view/component
   requiring it is removed.
 - No interfaces / pure virtual methods where it is possible to do it: Enfield relies mostly on template / CRTP / specialization to work.
 - Views cannot be serialized, only components can. (components are the data).
 - Components must be "default" constructible (respect the enfield construction concept)
 - Iterating over components or views of a given type is O(1) and cache efficient.

Good practices:
 - There should be no direct dynamic allocation in a component or a view. (a leak in either of those and the global memory consumption will increase quite rapidly)
 - 

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
