#pragma once

class Entity;

class IScene
{
public:

	IScene() = default;
	virtual ~IScene() = default;

	/// <summary>
	/// Optional: called after initialization, when the scene becomes active. Useful for triggering animations or starting background music. useful for resetting state when returning to a scene. Example: start a timer, play music, trigger an animation.
	/// </summary>
	/// <param name="deltaTime"></param>
	virtual void Start(float deltaTime) = 0;

	/// <summary>
	/// called to pause the scene. Useful for stopping timers, pausing animations, or halting background music. Example: pause a timer, stop music, pause an animation.
	/// </summary>
	virtual void Stop() = 0;

	/// <summary>
	/// Updates the scene’s systems and entities. deltaTime ensures time - based movements are smooth and frame - independent.
	/// </summary>
	/// <param name="deltaTime"></param>
	virtual void Update(float deltaTime) = 0;

	/// <summary>
	/// Called at a fixed timestep, often used for physics updates.
	/// </summary>
	virtual void FixedUpdate() = 0;


	/// <summary>
	/// called after Update, responsible for rendering the scene. This is where system Renderer would be invoked to draw entities. Example: clear the screen, draw entities, present the frame.
	/// </summary>
	virtual void Draw() = 0;


	/// <summary>
	/// functions for saving and loading the scene’s state, such as entity positions, component data, or game progress. This allows players to save their progress and return to it later. Example: serialize entity states to a file, deserialize them when loading the scene.
	/// </summary>
	virtual void SerializeState() = 0;
	virtual void DeserializeState() = 0;

	virtual void HandleInput(float deltaTime) = 0;

private:
	/// <summary>
	/// private helper functions for managing entities and components within the scene. These functions can be used internally by the scene to add or remove entities, manage component data, or handle entity lifecycle events. Example: AddEntity() creates a new entity and adds it to the scene, RemoveEntity() deletes an entity and cleans up its components.
	/// </summary>
	virtual void AddEntity(Entity&& entity) = 0;
	virtual void RemoveEntity(int index) = 0;
	



	

};
