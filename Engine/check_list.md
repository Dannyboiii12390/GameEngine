# Networked Physics Simulation Lab - Checklist

## Level 1: Core Features (60%)

### Scenes
- [x] Load scenes from FlatBuffers
- [x] Assign default values for missing FlatBuffer fields
- [x] Scene properties:
  - [x] Unique name
  - [x] Description
  - [x] Gravity flag
  - [x] Cameras list
  - [x] Objects list
  - [x] Spawners list
  - [x] Material interactions
- [x] Global UI to switch between scenes

### Cameras
- [x] Support multiple cameras per scene
- [x] Cameras have names for runtime switching
- [x] Local UI to switch between cameras
- [x] Mouse & keyboard control for camera movement
- [x] Support Perspective and Orthographic cameras
- [x] FlatBuffer schemas implemented:
  - [x] PerspectiveCamera: fov, near, far
  - [x] OrthographicCamera: size, near, far

### Physics Objects
- [ ] Object properties:
  - [x] Name (generate unique if missing)
  - [x] Transform: position, orientation, scale
  - [x] Material
  - [x] Shape: Sphere, Capsule, Cylinder, Plane, Cuboid
  - [ ] Behaviour: Static, Animated, Simulated
  - [x] Collision type (SOLID or CONTAINER)
- [x] Color-code objects by owner (Red, Green, Blue, Yellow)
- [x] Render containers correctly to see inside

### Materials & Interactions
- [x] Materials have name and density
- [x] MaterialInteraction table implemented:
  - [x] Coefficient of restitution
  - [x] Static and dynamic friction between material pairs

### Object Behaviours
#### Static Objects
- [x] Each user has local copy

#### Simulated Objects
- [x] Properties:
  - [x] Linear & angular velocity
  - [x] Owner (ONE, TWO, THREE, FOUR)
- [x] Full rigid body physics implemented (momentum, inertia)
- [x] Owner simulates object and handles collisions
- [x] Network dynamic state to all peers every frame

#### Animated Objects
- [x] Waypoints with position, orientation, and absolute time
- [x] Path modes implemented:
  - [x] STOP
  - [x] LOOP
  - [x] REVERSE
- [x] Easing types implemented (LINEAR, SMOOTHSTEP)
- [x] Collision with simulated objects transfers momentum correctly

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
- [x] Peer-to-peer networking (min 2 peers)
- [x] Each peer:
  - [x] Dedicated graphics thread
  - [x] Dedicated physics thread
  - [x] Shared scene state
- [x] Identical rendered images across peers

### Distributed Ownership
- [x] Static & Animated objects owned by all peers
- [x] Simulated objects assigned ownership per FlatBuffer or spawner
- [ ] Sequential spawner ownership implemented
- [x] Owners handle simulation & collision response

### Networking
- [x] Implement TCP or UDP communication
- [x] Ease of network configuration demonstrated

### Parallel Architecture
- [x] Major components operate asynchronously
- [x] Different update frequencies for:
  - [x] Graphics (e.g., 30 Hz)
  - [x] Physics simulation (e.g., 1000 Hz)
- [x] Controlled via ImGui

### Process Affinity
- [x] Graphics → Core 1
- [x] Networking → Core 2-3
- [x] Simulation → Core 4+
- [x] Additional threads allowed, cores must match mapping

---

## Level 2: Advanced Features (20%)

### Advanced Simulation (choose one)
- [ ] Cloth Simulation
- [ ] Compound Rigid Bodies
- [ ] Hinged Objects
- [x] Flocking & Steering
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
- [x] Extend chosen advanced simulation feature:
  - Cloth → wind, tearing, burning
  - Compound → fracturing
  - Hinged → ragdoll
  - Flocking → spatial partitioning (uniform grid, octree) with performance comparison

### Extended Concurrency
- [x] Compute shaders for simulation elements

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