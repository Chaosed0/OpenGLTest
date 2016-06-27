
#include "Game.h"

#include <SDL.h>
#include <SDL_Image.h>

#include <cstdio>
#include <cstdint>
#include <sstream>
#include <random>
#include <ctime>

#include <Windows.h>
#include <gl\glew.h>
#include <gl\GL.h>
#include <gl\GLU.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include "Renderer/Camera.h"
#include "Util.h"

#include "Framework/Components/ModelRenderComponent.h"
#include "Framework/Components/CameraComponent.h"
#include "Framework/Components/CollisionComponent.h"
#include "Framework/Components/TransformComponent.h"
#include "Framework/Components/PlayerComponent.h"
#include "Framework/Components/RigidbodyMotorComponent.h"

const static int updatesPerSecond = 60;
const static int windowWidth = 1920;
const static int windowHeight = 1080;

// Temp stuff for player movement, will be refactored later
static float cameraHorizontal = 0.0f;
static float cameraVertical = 0.0f;

static void bulletTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
	Game *g = static_cast<Game*>(world->getWorldUserInfo());
	g->fixedUpdate(world, timeStep);
}

Game::Game()
{
	running = false;
	wireframe = false;
	lastUpdate = UINT32_MAX;
	accumulator = 0.0f;
}

int Game::run()
{
	if (setup() < 0) {
		return -1;
	}

	running = true;
	loop();

	teardown();

	return 0;
}

void Game::exit()
{
	running = false;
}

void Game::setWireframe(bool on)
{
	wireframe = on;
}

void Game::setNoclip(bool on)
{
	if (on) {
		playerBody->setGravity(btVector3(0.0f, 0.0f, 0.0f));
	} else {
		playerBody->setGravity(dynamicsWorld->getGravity());
	}
	playerInputSystem->setNoclip(on);
}

int Game::setup()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf ("SDL could not initialize, error: %s\n", SDL_GetError());
		return -1;
	}

	if (IMG_Init(IMG_INIT_PNG) < 0)
	{
		printf ("SDL_Image could not initialize, error: %s\n", IMG_GetError());
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	window = SDL_CreateWindow("window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (window == NULL)
	{
		printf ("Could not create window, error: %s\n", SDL_GetError());
		return -1;
	}

	context = SDL_GL_CreateContext(window);

	if (context == NULL)
	{
		printf("Could not create OpenGL context, error: %s\n", SDL_GetError());
		return -1;
	}

	if (!renderer.initialize()) {
		return -1;
	}

	SDL_SetRelativeMouseMode(SDL_TRUE);

	/* Console */
	console = std::make_unique<Console>((float)windowWidth, windowHeight * 0.6f, (float)windowWidth, (float)windowHeight);
	console->addCallback("exit", CallbackMap::defineCallback(std::bind(&Game::exit, this)));
	console->addCallback("wireframe", CallbackMap::defineCallback<bool>(std::bind(&Game::setWireframe, this, std::placeholders::_1)));
	console->addCallback("noclip", CallbackMap::defineCallback<bool>(std::bind(&Game::setNoclip, this, std::placeholders::_1)));

	// Initialize renderer debugging output
	//renderer.setDebugLogCallback(std::bind(&Console::print, this->console.get(), std::placeholders::_1));

	/* Physics */
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
	dynamicsWorld->setGravity(btVector3(0.0f, -10.0f, 0.0f));
	dynamicsWorld->setInternalTickCallback(bulletTickCallback, static_cast<void *>(this));

	debugDrawer.initialize();
	dynamicsWorld->setDebugDrawer(&debugDrawer);
	//debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);

	/* Scene */
	shader.compileAndLink("Shaders/basic.vert", "Shaders/lightcolor.frag");
	skinnedShader.compileAndLink("Shaders/skinned.vert", "Shaders/lightcolor.frag");
	lightShader.compileAndLink("Shaders/basic.vert", "Shaders/singlecolor.frag");
	skyboxShader.compileAndLink("Shaders/skybox.vert", "Shaders/skybox.frag");

	glm::vec3 pointLightPositions[] = {
		glm::vec3(0.7f,  0.2f,  2.0f),
		glm::vec3(2.3f, -3.3f, -4.0f),
		glm::vec3(-4.0f,  2.0f, -12.0f),
		glm::vec3(0.0f,  0.0f, -3.0f)
	};

	pointLightTransforms.resize(4);
	for (int i = 0; i < 4; i++) {
		PointLight light;
		light.constant = 1.0f;
		light.linear = 0.09f;
		light.quadratic = 0.032f;
		light.ambient = glm::vec3(0.2f);
		light.diffuse = glm::vec3(0.5f);
		light.specular = glm::vec3(1.0f);
		renderer.setPointLight(i, light);

		pointLightTransforms[i].setPosition(pointLightPositions[i]);
		pointLightTransforms[i].setScale(glm::vec3(0.2f));
	}

	DirLight dirLight;
	dirLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
	dirLight.ambient = glm::vec3(0.2f);
	dirLight.diffuse = glm::vec3(0.9f);
	dirLight.specular = glm::vec3(1.0f);
	renderer.setDirLight(dirLight);

	Mesh pointLightMesh = getBox(std::vector<Texture> {});
	MaterialProperty pointLightColorProperty;
	pointLightColorProperty.type = MaterialPropertyType_vec4;
	pointLightColorProperty.value.vec4 = glm::vec4(1.0f);
	pointLightMesh.material.setProperty("color", pointLightColorProperty);
	pointLightModel = std::vector<Mesh> { pointLightMesh };

	std::vector<std::string> skyboxFaces;
	skyboxFaces.push_back("assets/img/skybox/miramar_ft.tga");
	skyboxFaces.push_back("assets/img/skybox/miramar_bk.tga");
	skyboxFaces.push_back("assets/img/skybox/miramar_up.tga");
	skyboxFaces.push_back("assets/img/skybox/miramar_dn.tga");
	skyboxFaces.push_back("assets/img/skybox/miramar_rt.tga");
	skyboxFaces.push_back("assets/img/skybox/miramar_lf.tga");
	skyboxModel = std::vector<Mesh> { getSkybox(skyboxFaces) };

	// Load some mushrooms
	Model shroomModel = modelLoader.loadModelFromPath("assets/models/shroom/shroom.fbx");

	unsigned seed = time(NULL);
	this->generator.seed(seed);
	printf("USING SEED: %d\n", seed);

	std::uniform_real_distribution<float> positionRand(-5.0f, 5.0f);
	std::uniform_real_distribution<float> scaleRand(0.5f, 2.0f);
	for (int i = 0; i < 10; i++) {
		Entity shroom;
		ModelRenderComponent* modelComponent = shroom.addComponent<ModelRenderComponent>();
		TransformComponent* transformComponent = shroom.addComponent<TransformComponent>();
		CollisionComponent* collisionComponent = shroom.addComponent<CollisionComponent>();

		transformComponent->transform.setPosition(glm::vec3(positionRand(generator), positionRand(generator) + 5.0f, positionRand(generator)));
		transformComponent->transform.setScale(glm::vec3(scaleRand(generator)));

		btSphereShape* shape = new btSphereShape(0.5f * transformComponent->transform.getScale().x);
		btDefaultMotionState* playerMotionState = new btDefaultMotionState(Util::gameToBt(transformComponent->transform));
		collisionComponent->body = new btRigidBody(1.0f, playerMotionState, shape, btVector3(0.0f, 0.0f, 0.0f));
		dynamicsWorld->addRigidBody(collisionComponent->body);

		unsigned int shroomHandle = renderer.getHandle(shroomModel, skinnedShader);
		renderer.setAnimation(shroomHandle, "AnimStack::Armature|move");
		modelComponent->rendererHandle = shroomHandle;
		entities.push_back(shroom);
	}

	/*Model spiderModel = modelLoader.loadModelFromPath("assets/models/spider/spider-tex.fbx");
	unsigned int spiderHandle = renderer.getHandle(spiderModel, skinnedShader);
	renderer.setAnimation(spiderHandle, "AnimStack::walk");
	renderer.updateTransform(spiderHandle, Transform(glm::vec3(), glm::quat(), glm::vec3(0.01, 0.01, 0.01)));*/

	/* Test Room */
	std::uniform_int_distribution<int> seedRand(INT_MIN, INT_MAX);
	RoomGenerator roomGenerator(seedRand(generator));
	Room room = roomGenerator.generate();
	const unsigned height = 6;
	Texture testTexture(TextureType_diffuse, "assets/img/test.png");

	for (unsigned i = 0; i < room.sides.size(); i++) {
		RoomSide side = room.sides[i];
		glm::vec3 scale(1.0f, height, 1.0f);
		if (side.normal.x == 0.0f) {
			scale.x = side.x1 - side.x0;
			scale.z = side.normal.y;
		} else {
			scale.x = side.normal.x;
			scale.z = side.y1 - side.y0;
		}
		Mesh box = getBox({ testTexture }, scale);

		// getBox() centers the mesh
		unsigned handle = renderer.getHandle(std::vector<Mesh> { box }, shader);
		renderer.updateTransform(handle, Transform(glm::vec3(side.x0 + scale.x / 2.0f, scale.y / 2.0f, side.y0 + scale.z / 2.0f)));

		// box shape doesn't do well with negative extents
		btBoxShape* wallShape = new btBoxShape(btVector3(std::abs(scale.x) / 2.0f, std::abs(scale.y) / 2.0f, std::abs(scale.z) / 2.0f));
		btCollisionObject* wallCollider = new btCollisionObject();
		wallCollider->setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(side.x0 + scale.x / 2.0f, scale.y / 2.0f, side.y0 + scale.z / 2.0f)));
		wallCollider->setCollisionShape(wallShape);
		roomData.collisionObjects.push_back(wallCollider);
		dynamicsWorld->addCollisionObject(wallCollider);
	}

	for (unsigned i = 0; i < room.boxes.size(); i++) {
		Box floor = room.boxes[i];
		glm::vec3 scale(floor.right - floor.left, 1.0f, floor.top - floor.bottom);
		Mesh mesh = getBox({ testTexture }, scale);
		unsigned floorHandle = renderer.getHandle(std::vector<Mesh> { mesh }, shader);
		renderer.updateTransform(floorHandle, Transform(glm::vec3(floor.left + scale.x / 2.0f, scale.y / 2.0f, floor.bottom + scale.z / 2.0f)));
		/*unsigned ceilingHandle = renderer.getHandle(std::vector<Mesh> { mesh }, shader);
		renderer.updateTransform(ceilingHandle, Transform(glm::vec3(floor.left + scale.x / 2.0f, height + scale.y / 2.0f, floor.bottom + scale.z / 2.0f)));*/

		btBoxShape* floorShape = new btBoxShape(Util::glmToBt(scale / 2.0f));
		btCollisionObject* floorCollider = new btCollisionObject();
		floorCollider->setCollisionShape(floorShape);
		floorCollider->setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(floor.left  + scale.x / 2.0f, scale.y / 2.0f, floor.bottom + scale.z / 2.0f)));
		roomData.collisionObjects.push_back(floorCollider);
		dynamicsWorld->addCollisionObject(floorCollider);
	}

	// Initialize the player
	TransformComponent* playerTransform = player.addComponent<TransformComponent>();
	CollisionComponent* playerCollisionComponent = player.addComponent<CollisionComponent>();
	PlayerComponent* playerComponent = player.addComponent<PlayerComponent>();
	RigidbodyMotorComponent* playerRigidbodyMotorComponent = player.addComponent<RigidbodyMotorComponent>();

	playerTransform->transform.setPosition(glm::vec3(0.0f, 8.0f, 0.0f));

	btCapsuleShape* shape = new btCapsuleShape(0.5f * playerTransform->transform.getScale().x, 2.0f * playerTransform->transform.getScale().y);
	btDefaultMotionState* motionState = new btDefaultMotionState(Util::gameToBt(playerTransform->transform));
	playerBody = new btRigidBody(1.0f, motionState, shape, btVector3(0.0f, 0.0f, 0.0f));
	playerBody->setAngularFactor(btVector3(0.0f, 0.0f, 0.0f));
	playerBody->setActivationState(DISABLE_DEACTIVATION);
	playerCollisionComponent->body = playerBody;
	dynamicsWorld->addRigidBody(playerCollisionComponent->body);

	playerRigidbodyMotorComponent->jumpSpeed = 5.0f;
	playerRigidbodyMotorComponent->moveSpeed = 5.0f;
	playerRigidbodyMotorComponent->noclip = false;

	entities.push_back(player);

	TransformComponent* cameraTransformComponent = camera.addComponent<TransformComponent>();
	CameraComponent* cameraComponent = camera.addComponent<CameraComponent>();
	cameraComponent->camera = Camera(glm::radians(90.0f), windowWidth, windowHeight, 0.1f, 1000000.0f);
	cameraTransformComponent->transform.setPosition(glm::vec3(0.0f, -0.5f, 0.0f));

	entities.push_back(camera);

	playerTransform->transform.addChild(&cameraTransformComponent->transform);

	renderer.setCamera(&cameraComponent->camera);
	debugDrawer.setCamera(&cameraComponent->camera);

	unsigned int skyboxHandle = renderer.getHandle(skyboxModel, skyboxShader);
	renderer.updateTransform(skyboxHandle, Transform::identity);
	for (unsigned int i = 0; i < pointLightTransforms.size(); i++) {
		unsigned int lightHandle = renderer.getHandle(pointLightModel, lightShader);
		renderer.updateTransform(lightHandle, pointLightTransforms[i]);
	}

	playerInputSystem = std::make_unique<PlayerInputSystem>();
	rigidbodyMotorSystem = std::make_unique<RigidbodyMotorSystem>();
	modelRenderSystem = std::make_unique<ModelRenderSystem>(renderer);
	collisionUpdateSystem = std::make_unique<CollisionUpdateSystem>();
	cameraSystem = std::make_unique<CameraSystem>(renderer);

	return 0;
}

int Game::teardown()
{
	return 0;
}

int Game::loop()
{
	// Initialize lastUpdate to get an accurate time
	lastUpdate = SDL_GetTicks();
	consoleIsVisible = true;
	while (running)
	{
		// Pause while the console is visible
		if (!consoleIsVisible) {
			accumulator += SDL_GetTicks() - lastUpdate;
		}

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			this->handleEvent(event);
		}

		lastUpdate = SDL_GetTicks();
		if (accumulator >= 1000.0f / updatesPerSecond)
		{
			timeDelta = 1.0f / updatesPerSecond;
			update();
			accumulator -= 1000.0f / updatesPerSecond;
		}
	
		draw();
	}

	return 0;
}

void Game::draw()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, this->wireframe ? GL_LINE : GL_FILL);

	renderer.draw();
	debugDrawer.draw();

	if (consoleIsVisible) {
		glDisable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		console->draw();
	}

	SDL_GL_SwapWindow(window);
}

void Game::update()
{
	renderer.update(timeDelta);

	Transform& cameraTransform = camera.getComponent<TransformComponent>()->transform;
	cameraTransform.setRotation(glm::angleAxis(cameraVertical, glm::vec3(-1.0f, 0.0f, 0.0f)));

	playerInputSystem->update(timeDelta, entities);
	rigidbodyMotorSystem->update(timeDelta, entities);

	dynamicsWorld->stepSimulation(timeDelta);

	static bool feh = true;
	if (feh) {
		debugDrawer.reset();
		dynamicsWorld->debugDrawWorld();
		feh = false;
	}

	cameraSystem->update(timeDelta, entities);
	collisionUpdateSystem->update(timeDelta, entities);
	modelRenderSystem->update(timeDelta, entities);
}

void Game::fixedUpdate(btDynamicsWorld* world, float dt) {
	assert(world == this->dynamicsWorld);

	player.getComponent<RigidbodyMotorComponent>()->canJump = false;
	int numManifolds = world->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		btPersistentManifold* contactManifold = world->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject* obA = contactManifold->getBody0();
		const btCollisionObject* obB = contactManifold->getBody1();
		int numContacts = contactManifold->getNumContacts();

		if ((obA == playerBody || obB == playerBody) && numContacts > 0) {
			player.getComponent<RigidbodyMotorComponent>()->canJump = true;
		}
	}
}

void Game::handleEvent(SDL_Event& event)
{
	switch(event.type) {
	case SDL_QUIT:
		running = false;
		break;
	case SDL_MOUSEMOTION:
		cameraHorizontal += event.motion.xrel * timeDelta * 0.2f;
		cameraVertical += event.motion.yrel * timeDelta * 0.2f;
		cameraVertical = glm::clamp(cameraVertical, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
		playerInputSystem->setHorizontalVerticalRotation(cameraHorizontal, cameraVertical);
		break;
	case SDL_TEXTINPUT:
		if (consoleIsVisible) {
			char c;
			for (int i = 0; (c = event.text.text[i]) != '\0'; i++) {
				if (c == '`') {
					// Ignore backticks
					continue;
				}
				console->inputChar(event.text.text[i]);
			}
		}
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_BACKQUOTE) {
			consoleIsVisible = !consoleIsVisible;
			if (consoleIsVisible) {
				SDL_StartTextInput();
			} else {
				SDL_StopTextInput();
			}
		}

		if (consoleIsVisible) {
			switch(event.key.keysym.sym) {
			case SDLK_RETURN:
				console->endLine();
				break;
			case SDLK_BACKSPACE:
				console->backspace();
				break;
			}
			break;
		} else if (event.key.repeat) {
			break;
		}

		switch(event.key.keysym.sym) {
		case SDLK_ESCAPE:
			SDL_SetRelativeMouseMode(SDL_FALSE);
			break;
		case SDLK_w:
			playerInputSystem->startMoving(glm::vec2(1,0));
			break;
		case SDLK_s:
			playerInputSystem->startMoving(glm::vec2(-1,0));
			break;
		case SDLK_d:
			playerInputSystem->startMoving(glm::vec2(0,1));
			break;
		case SDLK_a:
			playerInputSystem->startMoving(glm::vec2(0,-1));
			break;
		case SDLK_SPACE:
			playerInputSystem->startJump();
			break;
		}
		break;
	case SDL_KEYUP:
		if (consoleIsVisible) {
			break;
		}

		switch(event.key.keysym.sym) {
		case SDLK_w:
			playerInputSystem->stopMoving(glm::vec2(1,0));
			break;
		case SDLK_s:
			playerInputSystem->stopMoving(glm::vec2(-1,0));
			break;
		case SDLK_d:
			playerInputSystem->stopMoving(glm::vec2(0,1));
			break;
		case SDLK_a:
			playerInputSystem->stopMoving(glm::vec2(0,-1));
			break;
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		switch(event.button.button) {
		case SDL_BUTTON_LEFT:
			SDL_SetRelativeMouseMode(SDL_TRUE);
			break;
		}
	}
}

void Game::generateTestTerrain()
{
	/* Test Terrain */
	std::uniform_int_distribution<int> seedRand(INT_MIN, INT_MAX);
	const unsigned patchSize = 257;
	const float xzsize = 0.5f;
	Terrain terrain(patchSize, 0.005f, 6, 1.0f, 0.5f, seedRand(generator));
	for (unsigned i = 0; i < 4; i++) {
		this->terrainData.push_back(GameTerrainData());
		GameTerrainData& terrainData = this->terrainData[i];

		glm::ivec2 origin((i % 2) -1, (i >= 2) - 1);
		glm::vec3 scale(xzsize, 20.0f, xzsize);
		glm::vec3 position(origin.x * (int)(patchSize - 1) * xzsize, 0.0f, origin.y * (int)(patchSize - 1) * xzsize);

		terrainData.patch = terrain.generatePatch(origin.x, origin.y);
		terrainData.model = terrainData.patch.toModel(glm::ivec2(), scale);
		terrainData.model.meshes[0].material.setProperty("shininess", MaterialProperty(1000000.0f));

		unsigned int terrainHandle = renderer.getHandle(terrainData.model, shader);
		renderer.updateTransform(terrainHandle, Transform(position));

		terrainData.collision = terrainData.patch.getCollisionData(glm::ivec2(), scale);
		terrainData.vertArray = new btTriangleIndexVertexArray(terrainData.collision.indices.size() / 3, terrainData.collision.indices.data(), 3 * sizeof(unsigned), terrainData.collision.vertices.size(), terrainData.collision.vertices.data(), 3 * sizeof(float));
		terrainData.shape = new btBvhTriangleMeshShape(terrainData.vertArray, true);
		terrainData.object = new btCollisionObject();
		terrainData.object->setCollisionShape(terrainData.shape);
		terrainData.object->setWorldTransform(btTransform(Util::glmToBt(glm::quat()), Util::glmToBt(position)));
		dynamicsWorld->addCollisionObject(terrainData.object);
	}
}