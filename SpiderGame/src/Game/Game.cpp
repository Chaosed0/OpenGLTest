
#include "Game.h"

#include <SDL.h>
#include <SDL_Image.h>

#include <cstdio>
#include <cstdint>
#include <sstream>
#include <random>
#include <ctime>

#include "Framework/Prefab.h"
#include "Renderer/Camera.h"
#include "Util.h"

#include "Game/Components/CollisionComponent.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Events/GameEvents.h"
#include "Game/Extra/Config.h"

#include "Renderer/UI/Label.h"
#include "Renderer/UI/UIQuad.h"

const static int updatesPerSecond = 60;
const static int defaultWindowWidth = 1080;
const static int defaultWindowHeight = 720;

Game::Game()
{
	restart = false;
	running = false;
	wireframe = false;
	started = false;
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
	eid_t player = world.getEntityWithName("Player");
	if (player == World::NullEntity) {
		return;
	}

	CollisionComponent* collisionComponent = world.getComponent<CollisionComponent>(player);
	if (collisionComponent == nullptr) {
		return;
	}

	btRigidBody* rigidBody = (btRigidBody*)collisionComponent->collisionObject;
	if (on) {
		rigidBody->setGravity(btVector3(0.0f, 0.0f, 0.0f));
	} else {
		rigidBody->setGravity(dynamicsWorld->getGravity());
	}
	playerInputSystem->setNoclip(on);
}

void Game::setBulletDebugDraw(bool on)
{
	if (on) {
		debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);
	} else {
		debugDrawer.setDebugMode(0);
	}
}

void Game::refreshBulletDebugDraw()
{
	debugDrawer.reset();
	dynamicsWorld->debugDrawWorld();
}

void Game::restartGame()
{
	world.clear();

	scene->setup();
	std::vector<eid_t> cameraEntities = world.getEntitiesWithComponent<CameraComponent>();
	if (cameraEntities.size() < 0) {
		printf("WARNING: No camera in scene");
	} else {
		CameraComponent* cameraComponent = world.getComponent<CameraComponent>(cameraEntities[0]);
		debugDrawer.setCamera(&cameraComponent->data);
	}
}

int Game::setup()
{
	Config config;
	config.loadConfig("config.txt");
	int windowWidth = config.getValue<int>("resX", defaultWindowWidth);
	int windowHeight = config.getValue<int>("resY", defaultWindowHeight);
	bool fullscreen = config.getValue<bool>("fullscreen", false);
	bool borderless = config.getValue<bool>("borderless", false);
	bool nativeres = config.getValue<bool>("nativeres", false);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK) < 0)
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

	int windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	if (fullscreen) {
		if (nativeres) {
			windowFlags = windowFlags | SDL_WINDOW_FULLSCREEN_DESKTOP;
		} else {
			windowFlags = windowFlags | SDL_WINDOW_FULLSCREEN;
		}
	}

	if (borderless) {
		windowFlags = windowFlags | SDL_WINDOW_BORDERLESS;
	}

	window = SDL_CreateWindow("window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, windowFlags);

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

	if (!soundManager.initialize()) {
		return -1;
	}

	SDL_SetRelativeMouseMode(SDL_TRUE);

	/* Input */
	input.initialize();
	input.setDefaultMapping("Horizontal", KbmAxis_D, KbmAxis_A);
	input.setDefaultMapping("Vertical", KbmAxis_W, KbmAxis_S);
	input.setDefaultMapping("LookHorizontal", KbmAxis_MouseXPos, KbmAxis_MouseXNeg, AxisProps(0.1f, 0.2f, 0.3f));
	input.setDefaultMapping("LookVertical", KbmAxis_MouseYPos, KbmAxis_MouseYNeg, AxisProps(0.1f, 0.2f, 0.3f));
	input.setDefaultMapping("Jump", KbmAxis_Space, KbmAxis_None);
	input.setDefaultMapping("Use", KbmAxis_E, KbmAxis_None);
	input.setDefaultMapping("Fire", KbmAxis_MouseLeft, KbmAxis_None);
	input.setDefaultMapping("Reload", KbmAxis_R, KbmAxis_None);
	input.setDefaultMapping("Start", KbmAxis_Return, KbmAxis_None);

	input.setDefaultMapping("Horizontal", ControllerAxis_LStickXPos, ControllerAxis_LStickXNeg);
	input.setDefaultMapping("Vertical", ControllerAxis_LStickYPos, ControllerAxis_LStickYNeg);
	input.setDefaultMapping("LookHorizontal", ControllerAxis_RStickXPos, ControllerAxis_RStickXNeg, AxisProps(3.0f, 0.2f, 0.3f));
	input.setDefaultMapping("LookVertical", ControllerAxis_RStickYPos, ControllerAxis_RStickYNeg, AxisProps(3.0f, 0.2f, 0.3f));
	input.setDefaultMapping("Jump", ControllerAxis_A, ControllerAxis_None);
	input.setDefaultMapping("Use", ControllerAxis_X, ControllerAxis_None);
	input.setDefaultMapping("Fire", ControllerAxis_RightTrigger, ControllerAxis_None);
	input.setDefaultMapping("Reload", ControllerAxis_Y, ControllerAxis_None);
	input.setDefaultMapping("Start", ControllerAxis_Start, ControllerAxis_None);

	/*! Event Manager */
	eventManager = std::make_unique<EventManager>(world);

	ShaderLoader shaderLoader;
	Shader textShader = shaderLoader.compileAndLink("Shaders/basic2d.vert", "Shaders/text.frag");
	Shader backShader = shaderLoader.compileAndLink("Shaders/basic2d.vert", "Shaders/singlecolor.frag");

	/* Console */
	std::shared_ptr<Font> font(std::make_shared<Font>("assets/font/Inconsolata.otf", 18));
	console = std::make_unique<Console>(font, glm::vec2((float)windowWidth, windowHeight * 0.6f));
	console->addCallback("exit", CallbackMap::defineCallback(std::bind(&Game::exit, this)));
	console->addCallback("wireframe", CallbackMap::defineCallback<bool>(std::bind(&Game::setWireframe, this, std::placeholders::_1)));
	console->addCallback("noclip", CallbackMap::defineCallback<bool>(std::bind(&Game::setNoclip, this, std::placeholders::_1)));
	console->addCallback("enableBulletDebugDraw", CallbackMap::defineCallback<bool>(std::bind(&Game::setBulletDebugDraw, this, std::placeholders::_1)));
	console->addCallback("refreshBulletDebugDraw", CallbackMap::defineCallback(std::bind(&Game::refreshBulletDebugDraw, this)));
	console->addCallback("restart", CallbackMap::defineCallback(std::bind(&Game::restartGame, this)));
	console->addToRenderer(uiRenderer, backShader, textShader);

	/* Renderer */
	renderer.setDebugLogCallback(std::bind(&Console::print, this->console.get(), std::placeholders::_1));
	uiRenderer.setProjection(glm::ortho(0.0f, (float)windowWidth, (float)windowHeight, 0.0f, 1000.0f, -1000.0f));

	/* Physics */
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();
	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
	dynamicsWorld->setGravity(btVector3(0.0f, -10.0f, 0.0f));

	debugDrawer.initialize();
	dynamicsWorld->setDebugDrawer(&debugDrawer);

	physics = std::make_unique<Physics>(dynamicsWorld, *eventManager);

	unsigned seed = (unsigned)time(NULL);
	this->generator.seed(seed);
	printf("USING SEED: %ud\n", seed);

	shootingSystem = std::make_unique<ShootingSystem>(world, dynamicsWorld, renderer, *eventManager, generator);
	playerInputSystem = std::make_unique<PlayerInputSystem>(world, input, *eventManager);
	rigidbodyMotorSystem = std::make_unique<RigidbodyMotorSystem>(world);
	modelRenderSystem = std::make_unique<ModelRenderSystem>(world, renderer);
	collisionUpdateSystem = std::make_unique<CollisionUpdateSystem>(world);
	cameraSystem = std::make_unique<CameraSystem>(world, renderer);
	followSystem = std::make_unique<FollowSystem>(world, dynamicsWorld);
	spiderSystem = std::make_unique<SpiderSystem>(world, *eventManager, dynamicsWorld, renderer, soundManager, generator);
	expiresSystem = std::make_unique<ExpiresSystem>(world);
	velocitySystem = std::make_unique<VelocitySystem>(world);
	playerFacingSystem = std::make_unique<PlayerFacingSystem>(world, dynamicsWorld);
	audioListenerSystem = std::make_unique<AudioListenerSystem>(world, soundManager);
	audioSourceSystem = std::make_unique<AudioSourceSystem>(world, soundManager);
	pointLightSystem = std::make_unique<PointLightSystem>(world, renderer);
	spawnerSystem = std::make_unique<SpawnerSystem>(world, dynamicsWorld, generator);
	playerDeathSystem = std::make_unique<PlayerDeathSystem>(world, *eventManager);
	gemSystem = std::make_unique<GemSystem>(world, renderer, *eventManager);
	gameEndingSystem = std::make_unique<GameEndingSystem>(world, *eventManager, soundManager);
	shakeSystem = std::make_unique<ShakeSystem>(world, generator);

	SceneInfo sceneInfo;
	sceneInfo.dynamicsWorld = dynamicsWorld;
	sceneInfo.eventManager = eventManager.get();
	sceneInfo.generator = &generator;
	sceneInfo.renderer = &renderer;
	sceneInfo.soundManager = &soundManager;
	sceneInfo.uiRenderer = &uiRenderer;
	sceneInfo.world = &world;

	sceneInfo.windowHeight = windowHeight;
	sceneInfo.windowWidth = windowWidth;

	scene = std::make_unique<Scene>(sceneInfo);
	restartGame();

	TextureLoader textureLoader;
	launchScreen = std::shared_ptr<UIQuad>(new UIQuad(textureLoader.loadFromFile(TextureType_diffuse, "assets/img/SPIDERGAME.png"), glm::vec2(windowWidth, windowHeight)));
	launchScreen->transform = Transform(glm::vec3(0.0f, 0.0f, 1.0f)).matrix();
	launchScreenHandle = uiRenderer.getEntityHandle(launchScreen, shaderLoader.compileAndLink("shaders/basic2d.vert", "shaders/texture2d.frag"));

	std::function<void(const RestartEvent& event)> restartCallback =
		[game = this, world = &world](const RestartEvent& event) {
			// Defer the restart so we avoid invalidating any entity iterators
			game->restart = true;
		};
	eventManager->registerForEvent<RestartEvent>(restartCallback);

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
	console->setVisible(true);
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			this->handleEvent(event);
		}

		accumulator += SDL_GetTicks() - lastUpdate;
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
	renderer.draw();
	debugDrawer.draw();
	uiRenderer.draw();

	SDL_GL_SwapWindow(window);
}

void Game::update()
{
	input.update();

	if (!started) {
		for (int i = Device_Kbm; i <= Device_Controller3; i++) {
			if (input.getButtonDown("Start", (Device)i)) {
				started = true;
				playerInputSystem->setDevice((Device)i);
				launchScreen->isVisible = false;
			}
		}
	}

	if (!console->isVisible() || !started) {
		/* AI/Input */
		playerInputSystem->update(timeDelta);
		followSystem->update(timeDelta);
		spiderSystem->update(timeDelta);
		spawnerSystem->update(timeDelta);

		/* Physics */
		rigidbodyMotorSystem->update(timeDelta);
		velocitySystem->update(timeDelta);
		shootingSystem->update(timeDelta);
		gemSystem->update(timeDelta);

		dynamicsWorld->stepSimulation(timeDelta);

		/* Display */
		playerFacingSystem->update(timeDelta);
		collisionUpdateSystem->update(timeDelta);
		shakeSystem->update(timeDelta);
		cameraSystem->update(timeDelta);
		modelRenderSystem->update(timeDelta);
		pointLightSystem->update(timeDelta);
		audioSourceSystem->update(timeDelta);
		audioListenerSystem->update(timeDelta);

		renderer.update(timeDelta);
		soundManager.update();

		/* Cleanup */
		expiresSystem->update(timeDelta);
		playerDeathSystem->update(timeDelta);
		gameEndingSystem->update(timeDelta);

		world.cleanupEntities();
	}

	if (restart) {
		this->restartGame();
		restart = false;
	}
}

void Game::handleEvent(SDL_Event& event)
{
	if (!this->console->isVisible()) {
		input.handleEvent(event);
	}

	switch(event.type) {
	case SDL_QUIT:
		running = false;
		break;
	case SDL_TEXTINPUT:
		if (console->isVisible()) {
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
			console->setVisible(!console->isVisible());
			if (console->isVisible()) {
				SDL_StartTextInput();
				SDL_SetRelativeMouseMode(SDL_FALSE);
			} else {
				SDL_StopTextInput();
				SDL_SetRelativeMouseMode(SDL_TRUE);
			}
		}

		if (console->isVisible()) {
			switch(event.key.keysym.sym) {
			case SDLK_RETURN:
				console->endLine();
				break;
			case SDLK_BACKSPACE:
				console->backspace();
				break;
			case SDLK_UP:
			case SDLK_DOWN:
				console->recallHistory(event.key.keysym.sym == SDLK_UP);
				break;
			}
			break;
		} else if (event.key.repeat) {
			break;
		}

		switch(event.key.keysym.sym) {
		case SDLK_ESCAPE:
			running = false;
			break;
		}
		break;
	}
}
