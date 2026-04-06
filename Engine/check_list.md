# Networked Physics Simulation Lab - Checklist

## Level 1: Core Features (60%)

### Scenes
- [x] Load scenes from FlatBuffers
- [ ] Assign default values for missing FlatBuffer fields
- [ ] Scene properties:
  - [ ] Unique name
  - [ ] Description
  - [ ] Gravity flag
  - [ ] Cameras list
  - [ ] Objects list
  - [ ] Spawners list
  - [ ] Material interactions
- [x] Global UI to switch between scenes

### Cameras
- [x] Support multiple cameras per scene
- [x] Cameras have names for runtime switching
- [x] Local UI to switch between cameras
- [x] Mouse & keyboard control for camera movement
- [ ] Support Perspective and Orthographic cameras
- [x] FlatBuffer schemas implemented:
  - [x] PerspectiveCamera: fov, near, far
  - [ ] OrthographicCamera: size, near, far

### Physics Objects
- [ ] Object properties:
  - [ ] Name (generate unique if missing)
  - [x] Transform: position, orientation, scale
  - [ ] Material
  - [x] Shape: Sphere, Capsule, Cylinder, Plane, Cuboid
  - [ ] Behaviour: Static, Animated, Simulated
  - [ ] Collision type (SOLID or CONTAINER)
- [ ] Color-code objects by owner (Red, Green, Blue, Yellow)
- [ ] Render containers correctly to see inside

### Materials & Interactions
- [ ] Materials have name and density
- [ ] MaterialInteraction table implemented:
  - [ ] Coefficient of restitution
  - [ ] Static and dynamic friction between material pairs

### Object Behaviours
#### Static Objects
- [ ] Each user has local copy

#### Simulated Objects
- [ ] Properties:
  - [ ] Linear & angular velocity
  - [ ] Owner (ONE, TWO, THREE, FOUR)
- [x] Full rigid body physics implemented (momentum, inertia)
- [ ] Owner simulates object and handles collisions
- [ ] Network dynamic state to all peers every frame

#### Animated Objects
- [ ] Waypoints with position, orientation, and absolute time
- [ ] Path modes implemented:
  - [ ] STOP
  - [ ] LOOP
  - [ ] REVERSE
- [ ] Easing types implemented (LINEAR, SMOOTHSTEP)
- [ ] Collision with simulated objects transfers momentum correctly

### Object Spawners
- [ ] BaseSpawner properties:
  - [ ] Name
  - [ ] Start time
  - [ ] SpawnType (SingleBurst, Repeating)
  - [ ] SpawnLocation (Fixed, Box, Sphere)
  - [ ] Linear & angular velocity ranges
  - [ ] Material
  - [ ] Owner (ONE, TWO, THREE, FOUR, SEQUENTIAL)
- [ ] Implement SingleBurstSpawn
- [ ] Implement RepeatingSpawn
- [ ] Support different shape spawners (Sphere, Cylinder, Capsule, Cuboid)
- [ ] Spawned objects are Simulated and SOLID

---

## Level 1: Core Concurrency Features (30%)

### Distributed Architecture
- [ ] Peer-to-peer networking (min 2 peers)
- [ ] Each peer:
  - [ ] Dedicated graphics thread
  - [ ] Dedicated physics thread
  - [ ] Shared scene state
- [ ] Identical rendered images across peers

### Distributed Ownership
- [ ] Static & Animated objects owned by all peers
- [ ] Simulated objects assigned ownership per FlatBuffer or spawner
- [ ] Sequential spawner ownership implemented
- [ ] Owners handle simulation & collision response

### Networking
- [ ] Implement TCP or UDP communication
- [ ] Ease of network configuration demonstrated

### Parallel Architecture
- [x] Major components operate asynchronously
- [ ] Different update frequencies for:
  - [ ] Graphics (e.g., 30 Hz)
  - [ ] Physics simulation (e.g., 1000 Hz)
- [ ] Controlled via ImGui

### Process Affinity
- [ ] Graphics → Core 1
- [ ] Networking → Core 2-3
- [ ] Simulation → Core 4+
- [ ] Additional threads allowed, cores must match mapping

---

## Level 2: Advanced Features (20%)

### Advanced Simulation (choose one)
- [ ] Cloth Simulation
- [ ] Compound Rigid Bodies
- [ ] Hinged Objects
- [ ] Flocking & Steering
- [ ] Include UI controls & debugging visualization
- [ ] Extend FlatBuffer schema for chosen feature

### Advanced Concurrency
- [ ] ≥3 peers supported
- [ ] Data integrity maintained (interpolation/correction for drift)
- [ ] Network QoS:
  - [ ] Handle latency: 100ms ± 50ms
  - [ ] Handle packet loss: 20%

---

## Level 3: Extended Advanced Features (20%)

### Extended Simulation
- [ ] Extend chosen advanced simulation feature:
  - Cloth → wind, tearing, burning
  - Compound → fracturing
  - Hinged → ragdoll
  - Flocking → spatial partitioning (uniform grid, octree) with performance comparison

### Extended Concurrency
- [ ] Compute shaders for simulation elements

---

## Implementation Requirements
- [x] Vulkan graphics with ImGui UI
- [x] Physics implemented (example code allowed)
- [x] glm math library only
- [x] Winsock 2 networking only
- [x] Win32 or C++23 threads only
- [ ] Demonstrable on RBB-335 PCs

---

## Deliverables
- [ ] Report (Markdown on GitHub):
  - [ ] System architecture & threads/networking (≤1000 words)
  - [ ] Motion physics & collision detection/response (≤1000 words)
- [ ] Source code on GitHub
- [ ] Narrated video demonstrating features submitted via Canvas