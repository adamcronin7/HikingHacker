#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "animation.h"
#include "gameobject.h"
#include "tmx.h"
#include <filesystem>


using namespace std;
struct SDLState
{
	SDL_Window* window{};
	SDL_Renderer* renderer{};

	// Width and height of the game window
	int width{ 1280 };
	int height{ 720 };

	// Logical width and height of the screen
	int logW{ 1280 };
	int logH{ 720 };

	const bool* keys{};
	bool fullscreen{};

	SDLState() = default;
};

const int TILE_SIZE{ 64 };

enum class GameOver { none, restart, complete };

enum class ObjectiveId
{
	none,
	followSigns,
	enterTent,
	bearFullHealth,
	bearDamaged,
	bearDefeated,
	collectFinalFlag
};

struct GameState
{
	GameOver gameOver{ GameOver::none };
	float fadeAlpha{};
	float fadeSpeed{};
	std::vector<std::vector<GameObject>> layers;
	int playerIndex;
	int playerLayer;
	glm::vec2 playerInitialPos;
	SDL_FRect mapViewport;
	float bg2Scroll, bg3Scroll, bg4Scroll;
	//bool debugMode;
	int level;
	bool gotFlag;
	int flagCounter;
	bool chestOpened;
	bool bearIsAlive;
	bool bearHasBeenDamaged;
	bool bearFlagCollected;
	bool chestFlagCollected;
	bool bearFlagShown;
	bool chestFlagShown;
	ObjectiveId currentObjective{ ObjectiveId::none };


	GameState(const SDLState& state)
	{
		reset(state);
	}

	// To be used to initialize and re-initialize the game state every start/restart
	void reset(const SDLState& state)
	{
		layers.clear();
		gameOver = GameOver::none;
		fadeAlpha = 0.0f;
		fadeSpeed = 0.0f;
		playerLayer = -1;
		playerIndex = -1;
		playerInitialPos = glm::vec2(0);
		mapViewport = SDL_FRect{
			.x = 0, .y = 0,
			.w = static_cast<float>(state.logW), .h = static_cast<float>(state.logH)
		};
		bg2Scroll = bg3Scroll = bg4Scroll = 0.0f;
		//debugMode = false;
		level = 1;
		gotFlag = false;
		flagCounter = 0;
		chestOpened = false;
		bearIsAlive = true;
		bearHasBeenDamaged = false;
		bearFlagCollected = false;
		chestFlagCollected = false;
		bearFlagShown = false;
		chestFlagShown = false;
		currentObjective = ObjectiveId::none;
	}

	void setObjective(ObjectiveId objective)
	{
		currentObjective = objective;
	}

	GameObject& player() { return layers[playerLayer][playerIndex]; }
};

struct TileSetTextures
{
	int firstGid;
	SDL_Texture* sheet = nullptr;          // spritesheet case
	std::vector<SDL_Texture*> textures;    // image-collection case
};

struct MenuText
{
	SDL_Texture* tex{};
	float w{}, h{};
};

struct Resources
{
	const int ANIM_PLAYER_IDLE = 0;
	const int ANIM_PLAYER_RUN = 1;
	std::vector<Animation> playerAnims;
	const int ANIM_BEAR_WALK = 0;
	const int ANIM_BEAR_DIE = 1;
	std::vector<Animation> bearAnims;
	const int ANIM_FLAG_FLOAT = 0;
	std::vector<Animation> flagAnims;

	std::vector<SDL_Texture*> textures;
	SDL_Texture* texIdle, * texRun, * texGrassTop, * texGrassCenter, * texBg1, * texBg2, * texBg3, * texBg4
		, * texRock, * texFlower, * texRockBlockRight, * texRockBlockCenter
		, * texBearWalk, * texBearDie, * texFlag, * texChestClosed, * texChestOpen;

	std::vector<TileSetTextures> tilesetTextures;
	std::unique_ptr<tmx::Map> map;

	SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& filepath)
	{
		SDL_Texture* tex = IMG_LoadTexture(renderer, filepath.c_str());
		if (!tex)
		{
			SDL_Log("Texture load failed: %s (%s)", filepath.c_str(), SDL_GetError());
			return nullptr;
		}
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
		textures.push_back(tex);
		return tex;
	}


	void load(SDLState& state, string level)
	{
		playerAnims.resize(2);
		playerAnims[ANIM_PLAYER_IDLE] = Animation(4, 0.8f);
		playerAnims[ANIM_PLAYER_RUN] = Animation(4, 0.8f);
		bearAnims.resize(2);
		bearAnims[ANIM_BEAR_WALK] = Animation(4, 0.8f);
		bearAnims[ANIM_BEAR_DIE] = Animation(7, 0.8f);
		flagAnims.resize(1);
		flagAnims[ANIM_FLAG_FLOAT] = Animation(8, 0.8f);

		texIdle = loadTexture(state.renderer, "assets/sprites/player-idle.png");
		texRun = loadTexture(state.renderer, "assets/sprites/player-run.png");
		texBearWalk = loadTexture(state.renderer, "assets/sprites/bear-walk.png");
		texBearDie = loadTexture(state.renderer, "assets/sprites/bear-death.png");
		texFlag = loadTexture(state.renderer, "assets/tiles/flag.png");
		texChestClosed = loadTexture(state.renderer, "assets/tiles/chest-closed.png");
		texChestOpen = loadTexture(state.renderer, "assets/tiles/chest-open.png");
		texBg1 = loadTexture(state.renderer, "assets/backgrounds/background_solid_sky.svg");
		texBg2 = loadTexture(state.renderer, "assets/backgrounds/cloudLayer1.png");
		texBg3 = loadTexture(state.renderer, "assets/backgrounds/mountains.png");
		texBg4 = loadTexture(state.renderer, "assets/backgrounds/cloudLayer2.png");


		// load the map XML and preload images
		map = tmx::loadMap(level);
		for (tmx::TileSet& tileSet : map->tileSets)
		{
			TileSetTextures tst;
			tst.firstGid = tileSet.firstgid;
			tst.textures.reserve(tileSet.tiles.size());


			if (!tileSet.image.source.empty())
			{
				tst.sheet = loadTexture(state.renderer,
					"assets/tiles/" + std::filesystem::path(tileSet.image.source).filename().string());
			}



			for (tmx::Tile& tile : tileSet.tiles)
			{
				const std::string imagePath = "assets/tiles/" + std::filesystem::path(tile.image.source).filename().string();
				tst.textures.push_back(loadTexture(state.renderer, imagePath));
			}
			tilesetTextures.push_back(std::move(tst));
		}

	}

	void unload()
	{
		for (SDL_Texture* tex : textures)
		{
			SDL_DestroyTexture(tex);
		}
		textures.clear();
		tilesetTextures.clear();
		map.reset();
	}
};


bool initialize(SDLState& state);
void cleanup(SDLState& state);
void drawObject(const SDLState& state, GameState& gs, GameObject& obj, float width, float height, float deltaTime);
void update(const SDLState& state, GameState& gs, Resources& res, GameObject& obj, float deltaTime);
void createTiles(const SDLState& state, GameState& gs, const Resources& res);
void checkCollision(const SDLState& state, GameState& gs, Resources& res, GameObject& a
	, GameObject& b, float deltaTime);
void handleKeyInput(const SDLState& state, GameState& gs, GameObject& obj
	, SDL_Scancode key, bool keyDown);
void drawParalaxBackground(float viewportY, SDL_Renderer* renderer, SDL_Texture* texture
	, float xVelocity, float& scrollPos, float scrollFactor, float deltaTime);
MenuText makeMenuText(SDL_Renderer* renderer, TTF_Font* font, const char* text, float areaW, bool isObjective);
void drawText(SDL_Renderer* renderer, const MenuText& mt, float areaW, float areaH, bool isObjective);

class ObjectiveTextCache
{
private:
	SDL_Renderer* renderer{};
	TTF_Font* font{};
	float areaW{};
	std::unordered_map<std::string, MenuText> cache;

public:
	ObjectiveTextCache(SDL_Renderer* renderer, TTF_Font* font, float areaW)
		: renderer(renderer), font(font), areaW(areaW)
	{
	}

	~ObjectiveTextCache()
	{
		clear();
	}

	ObjectiveTextCache(const ObjectiveTextCache&) = delete;
	ObjectiveTextCache& operator=(const ObjectiveTextCache&) = delete;

	const MenuText* get(const std::string& text)
	{
		auto it = cache.find(text);
		if (it != cache.end())
			return &it->second;

		MenuText rendered = makeMenuText(renderer, font, text.c_str(), areaW, true);
		if (!rendered.tex)
			return nullptr;

		auto result = cache.emplace(text, rendered);
		return &result.first->second;
	}

	void clear()
	{
		for (auto& [text, menuText] : cache)
		{
			if (menuText.tex)
				SDL_DestroyTexture(menuText.tex);
		}
		cache.clear();
	}
};

const char* getObjectiveText(ObjectiveId id)
{
	switch (id)
	{
	case ObjectiveId::followSigns:
		return "Objective: Follow the signs to get to the flag.\n\nControl the player with:\nA (left) D (right) SPACE (jump)";
	case ObjectiveId::enterTent:
		return "Objective: Enter the tent to find the other flags.";
	case ObjectiveId::bearFullHealth:
		return "Objective: There must be some way to get rid of that bear...\n\nThe bear currently has full health.";
	case ObjectiveId::bearDamaged:
		return "Objective: There must be some way to get rid of that bear...\n\nThe bear has taken some damage.";
	case ObjectiveId::bearDefeated:
		return "Objective: Well done, looks like he had a flag! Now lets try and open that chest.";
	case ObjectiveId::collectFinalFlag:
		return "Objective: You've done it! Collect the third and final flag.";
	case ObjectiveId::none:
	default:
		return nullptr;
	}
}

void gameEnd(GameState& gs, string action);


int main(int argc, char* argv[])
{
	SDLState state{};
	state.width = 1280;
	state.height = 720;
	state.logW = 1280;
	state.logH = 720;

	if (!initialize(state))
		return 1;

	if (!TTF_Init())
	{
		SDL_Log("SDL_ttf initialization failed: %s", SDL_GetError());
		cleanup(state);
		return 1;
	}

	TTF_Font* objectiveFont = TTF_OpenFont("assets/fonts/font.ttf", 20);
	TTF_Font* menuFont = TTF_OpenFont("assets/fonts/font.ttf", 50);
	if (!objectiveFont || !menuFont)
	{
		SDL_Log("Font loading failed: %s", SDL_GetError());
		if (objectiveFont) TTF_CloseFont(objectiveFont);
		if (menuFont) TTF_CloseFont(menuFont);
		TTF_Quit();
		cleanup(state);
		return 1;
	}

	Resources res;
	GameState gs(state);

	// The objective cache outlives GameState resets, so textures can be reused when the player restarts or revisits an objective
	ObjectiveTextCache objectiveCache(state.renderer, objectiveFont, static_cast<float>(state.logW));

	// Menu messages are created once and reused while the menu is shown
	MenuText restartText = makeMenuText(
		state.renderer, menuFont,
		"That wasn't a good idea...\n\nPress Enter to restart\nOr Esc to quit",
		static_cast<float>(state.logW), false);
	MenuText completeText = makeMenuText(
		state.renderer, menuFont,
		"Well done! You've found all of the flags\n\nPress Enter to play again\nOr Esc to quit",
		static_cast<float>(state.logW), false);

	uint64_t prevTime{ SDL_GetTicks() };


	// This is the main game loop
	bool running{ true };
	int currentLevel{ 0 };
	std::string mapPath;
	SDL_Event event{ 0 };

	while (running)
	{
		if (gs.gameOver == GameOver::none)
		{
			if (gs.level != currentLevel)
			{
				res.unload();
				mapPath.clear();

				switch (gs.level)
				{
				case 1:
					mapPath = "assets/maps/level1.tmx";
					gs.setObjective(ObjectiveId::followSigns);
					break;

				case 2:
					mapPath = "assets/maps/level2.tmx";
					gs.bearIsAlive = true;
					gs.bearHasBeenDamaged = false;
					break;

				default:
					SDL_Log("Unknown level: %d", gs.level);
					gs.gameOver = GameOver::complete;
					break;
				}

				if (!mapPath.empty())
				{
					res.load(state, mapPath);
					createTiles(state, gs, res);
					currentLevel = gs.level;
				}
			}

			if (gs.gameOver != GameOver::none)
				continue;

			uint64_t nowTime{ SDL_GetTicks() };
			float deltaTime{ (nowTime - prevTime) / 1000.0f };

			// Never allow deltaTime to exceed 1/30th of a second
			// Large delays are discarded and the max delta time is used instead to prevent physics/collision update issues
			constexpr float MAX_DELTA_TIME = 1.0f / 30.0f;
			deltaTime = std::min(deltaTime, MAX_DELTA_TIME);

			// Limits to 120 updates per second
			if (deltaTime < 1.0f / 120.0f)
			{
				SDL_Delay(1);
				continue;
			}

			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					running = false;
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					state.width = event.window.data1;
					state.height = event.window.data2;
					break;

				case SDL_EVENT_KEY_DOWN:
					if (gs.playerLayer >= 0 && gs.playerIndex >= 0)
						handleKeyInput(state, gs, gs.player(), event.key.scancode, true);
					break;

				case SDL_EVENT_KEY_UP:
					if (gs.playerLayer >= 0 && gs.playerIndex >= 0)
						handleKeyInput(state, gs, gs.player(), event.key.scancode, false);

					//if (event.key.scancode == SDL_SCANCODE_B)
					//	gs.debugMode = !gs.debugMode;
					if (event.key.scancode == SDL_SCANCODE_F11)
					{
						state.fullscreen = !state.fullscreen;
						SDL_SetWindowFullscreen(state.window, state.fullscreen);
					}
					break;
				}
			}

			for (auto& layer : gs.layers)
			{
				for (GameObject& obj : layer)
					update(state, gs, res, obj, deltaTime);
			}

			if (gs.gameOver != GameOver::none)
				continue;

			const int mapWPixels = res.map->mapWidth * res.map->tileWidth;
			const int mapHPixels = res.map->mapHeight * res.map->tileHeight;

			float prevCamX = gs.mapViewport.x;
			float camX = (gs.player().position.x + TILE_SIZE / 2) - gs.mapViewport.w / 2;
			gs.mapViewport.x = SDL_clamp(camX, 0.0f, mapWPixels - gs.mapViewport.w);
			float camY = (gs.player().position.y + TILE_SIZE / 2) - gs.mapViewport.h / 2;
			gs.mapViewport.y = SDL_clamp(camY, 0.0f, mapHPixels - gs.mapViewport.h);

			const float camVelX = (deltaTime > 0.0f)
				? (gs.mapViewport.x - prevCamX) / deltaTime
				: 0.0f;

			SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
			SDL_RenderClear(state.renderer);

			SDL_RenderTexture(state.renderer, res.texBg1, nullptr, nullptr);
			drawParalaxBackground(gs.mapViewport.y, state.renderer, res.texBg2, camVelX,
				gs.bg2Scroll, 0.075f, deltaTime);
			drawParalaxBackground(gs.mapViewport.y, state.renderer, res.texBg3, camVelX,
				gs.bg3Scroll, 0.150f, deltaTime);
			drawParalaxBackground(gs.mapViewport.y, state.renderer, res.texBg4, camVelX,
				gs.bg4Scroll, 0.3f, deltaTime);

			for (auto& layer : gs.layers)
			{
				for (GameObject& obj : layer)
				{
					float w = (obj.type == ObjectType::level)
						? static_cast<float>(res.map->tileWidth)
						: static_cast<float>(obj.width);
					drawObject(state, gs, obj, w, w, deltaTime);
				}
			}

			//if (gs.debugMode)
			//{
			//	SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			//	SDL_RenderDebugText(state.renderer, 5.0f, 5.0f,
			//		std::format("S: {}, G: {}",
			//			static_cast<int>(gs.player().data.player.state), gs.player().grounded).c_str());
			//}

			if (const char* objectiveText = getObjectiveText(gs.currentObjective))
			{
				if (const MenuText* objective = objectiveCache.get(objectiveText))
					drawText(state.renderer, *objective, static_cast<float>(state.logW),
						static_cast<float>(state.logH), true);
			}

			SDL_RenderPresent(state.renderer);
			prevTime = nowTime;
		}
		else
		{
			// Game-over menu, level textures are no longer needed while this screen is shown
			res.unload();

			uint64_t nowTime{ SDL_GetTicks() };
			float deltaTime{ (nowTime - prevTime) / 1000.0f };
			if (deltaTime < 1.0f / 120.0f)
			{
				SDL_Delay(1);
				continue;
			}

			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					running = false;
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					state.width = event.window.data1;
					state.height = event.window.data2;
					break;

				case SDL_EVENT_KEY_UP:
					if (event.key.scancode == SDL_SCANCODE_F11)
					{
						state.fullscreen = !state.fullscreen;
						SDL_SetWindowFullscreen(state.window, state.fullscreen);
					}
					else if (event.key.scancode == SDL_SCANCODE_ESCAPE)
						running = false;
					else if (event.key.scancode == SDL_SCANCODE_RETURN)
					{
						// Reset the existing gameplay state
						gs.reset(state);
						currentLevel = 0;
						gs.level = 1;
					}
					break;
				}
			}

			SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
			SDL_RenderClear(state.renderer);

			switch (gs.gameOver)
			{
			case GameOver::restart:
				drawText(state.renderer, restartText, static_cast<float>(state.logW),
					static_cast<float>(state.logH), false);
				break;
			case GameOver::complete:
				drawText(state.renderer, completeText, static_cast<float>(state.logW),
					static_cast<float>(state.logH), false);
				break;
			case GameOver::none:
				break;
			}

			SDL_RenderPresent(state.renderer);
			prevTime = nowTime;
		}
	}

	// Destroy every texture while the SDL renderer is still alive
	objectiveCache.clear();
	if (restartText.tex) SDL_DestroyTexture(restartText.tex);
	if (completeText.tex) SDL_DestroyTexture(completeText.tex);
	res.unload();

	TTF_CloseFont(objectiveFont);
	TTF_CloseFont(menuFont);
	TTF_Quit();
	cleanup(state);
	return 0;
}

// Function to initialise SDL state for rendering content
bool initialize(SDLState& state)
{
	bool initSuccess{ true };

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
		initSuccess = false;
		return initSuccess;
	}

	// The window is created with default resolution
	state.keys = SDL_GetKeyboardState(nullptr);
	state.window = SDL_CreateWindow("Hiking Hacker", state.width, state.height, SDL_WINDOW_RESIZABLE);
	if (!state.window)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
		cleanup(state);
		initSuccess = false;
		return initSuccess;
	}

	// Create the renderer
	state.renderer = SDL_CreateRenderer(state.window, nullptr);
	if (!state.renderer)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", nullptr);
		cleanup(state);
		initSuccess = false;
		return initSuccess;
	}


	// Configure presentation
	SDL_SetRenderLogicalPresentation(state.renderer, state.logW, state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	return initSuccess;
}

// Function to ensure a cleanup of the SDL state when closing
void cleanup(SDLState& state)
{
	SDL_DestroyRenderer(state.renderer);
	SDL_DestroyWindow(state.window);
	SDL_Quit();
}

void drawObject(const SDLState& state, GameState& gs, GameObject& obj
	, float width, float height, float deltaTime)
{
	float srcX = obj.currentAnimation != -1
		? obj.animations[obj.currentAnimation].currentFrame() * width : (obj.spriteFrame - 1) * width;

	SDL_FRect src = obj.tileSrc.w > 0
		? obj.tileSrc
		: SDL_FRect{ srcX, 0, width, width };

	// Define position of sprite on screen
	SDL_FRect dst
	{
		.x = obj.position.x - gs.mapViewport.x,
		.y = obj.position.y - gs.mapViewport.y,
		.w = obj.tileSrc.w > 0 ? obj.width : width,
		.h = obj.tileSrc.w > 0 ? obj.height : width
	};


	const bool isTile = obj.tileSrc.w > 0;
	SDL_FlipMode flipMode = isTile
		? obj.tileFlip
		: (obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

	// Draw sprite on screen
	SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst,
		isTile ? obj.tileAngle : 0, nullptr, flipMode);


	//if (gs.debugMode)
	//{
	//	SDL_FRect rectA{
	//		.x = obj.position.x + obj.collider.x - gs.mapViewport.x,
	//		.y = obj.position.y + obj.collider.y - gs.mapViewport.y,
	//		.w = obj.collider.w,
	//		.h = obj.collider.h
	//	};
	//	SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
	//	SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 150);
	//	SDL_RenderFillRect(state.renderer, &rectA);
	//	SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
	//}

}

void update(const SDLState& state, GameState& gs, Resources& res, GameObject& obj, float deltaTime)
{
	// Update the animation
	if (obj.currentAnimation != -1)
	{
		obj.animations[obj.currentAnimation].step(deltaTime);
	}

	if (obj.dynamic && !obj.grounded)
	{
		// Apply gravity
		obj.velocity += glm::vec2(0, 500) * deltaTime;
	}

	float currentDirection{ 0 };
	if (obj.type == ObjectType::player)
	{
		if (state.keys[SDL_SCANCODE_A])
		{
			// This check is to stop gliding if switching running directions
			if (obj.velocity.x > 0)
			{
				obj.velocity.x = 0;
			}

			currentDirection += -1;
		}
		if (state.keys[SDL_SCANCODE_D])
		{
			// This check is to stop gliding if switching running directions
			if (obj.velocity.x < 0)
			{
				obj.velocity.x = 0;
			}

			currentDirection += +1;
		}


		switch (obj.data.player.state)
		{
		case PlayerState::idle:
		{
			if (currentDirection)
			{
				obj.data.player.state = PlayerState::running;
				obj.texture = res.texRun;
				obj.currentAnimation = res.ANIM_PLAYER_RUN;
			}
			else
			{
				// Decelerate 
				if (obj.velocity.x)
				{
					const float factor = obj.velocity.x > 0 ? -1.5f : +1.5f;
					float amount = factor * obj.acceleration.x * deltaTime;
					if (std::abs(obj.velocity.x) < std::abs(amount))
					{
						obj.velocity.x = 0;
					}
					else
					{
						obj.velocity.x = 0;
					}
				}
				obj.texture = res.texIdle;
				obj.currentAnimation = res.ANIM_PLAYER_IDLE;
			}
			break;
		}
		case PlayerState::running:
		{
			if (!currentDirection)
			{
				obj.data.player.state = PlayerState::idle;
			}
			obj.texture = res.texRun;
			obj.currentAnimation = res.ANIM_PLAYER_RUN;
			break;
		}
		case PlayerState::jumping:
		{
			obj.texture = res.texRun;
			obj.currentAnimation = res.ANIM_PLAYER_RUN;
			break;
		}
		}
	}

	if (obj.type == ObjectType::enemy)
	{
		if (obj.data.enemy.healthPoints == 1337)
		{
			gs.setObjective(ObjectiveId::bearFullHealth);
		}
		else if (obj.data.enemy.healthPoints <= 0)
		{
			obj.data.enemy.state = EnemyState::dead;
			if (gs.bearIsAlive)
			{
				gs.bearIsAlive = false;
				gs.setObjective(ObjectiveId::bearDefeated);
			}
		}
		else if (obj.data.enemy.healthPoints < 1337 && !gs.bearHasBeenDamaged)
		{
			// Only change the objective on the first transition away from full health
			gs.bearHasBeenDamaged = true;
			if (gs.currentObjective == ObjectiveId::bearFullHealth)
				gs.setObjective(ObjectiveId::bearDamaged);
		}

		switch (obj.data.enemy.state)
		{
		case EnemyState::walking:
		{
			currentDirection = obj.direction;
			break;
		}
		case EnemyState::dead:
		{
			obj.collidable = false;
			obj.texture = res.texBearDie;
			obj.currentAnimation = res.ANIM_BEAR_DIE;
			obj.velocity = glm::vec2(0);
			if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone())
			{
				// Remove animation and set to last frame
				obj.currentAnimation = -1;
				obj.spriteFrame = 18;
			}
			break;
		}
		}
	}

	if (obj.type == ObjectType::chest)
	{
		switch (obj.data.chest.state)
		{
		case ChestState::closed:
		{
			if (obj.data.chest.isOpen && gs.flagCounter == 2)
			{
				// Only allow the chest to be opened if the player has two flags already
				obj.data.chest.state = ChestState::open;
				obj.texture = res.texChestOpen;
				gs.chestOpened = true;
				gs.setObjective(ObjectiveId::collectFinalFlag);
			}
			break;
		}
		case ChestState::open:
		{
			if (!obj.data.chest.isOpen)
			{
				obj.data.chest.state = ChestState::closed;
				obj.texture = res.texChestClosed;
			}
			break;
		}
		}
	}

	if (obj.type == ObjectType::flag)
	{
		// Hidden special flags become collectible when unlocked, but are not re-enabled after the player has collected them
		if (obj.data.flag.chestFlag && gs.chestOpened && !gs.chestFlagCollected && !gs.chestFlagShown)
		{
			obj.collidable = true;
			obj.currentAnimation = res.ANIM_FLAG_FLOAT;
			obj.spriteFrame = 0;
			gs.chestFlagShown = true;
		}
		else if (obj.data.flag.bearFlag && !gs.bearIsAlive && !gs.bearFlagCollected && !gs.bearFlagShown)
		{
			obj.collidable = true;
			obj.currentAnimation = res.ANIM_FLAG_FLOAT;
			obj.spriteFrame = 0;
			gs.bearFlagShown = true;
		}
	}

	if (currentDirection)
	{
		obj.direction = currentDirection;
	}
	// Add acceleration to velocity
	obj.velocity += currentDirection * obj.acceleration * deltaTime;
	if (std::abs(obj.velocity.x) > obj.maxSpeedX)
	{
		obj.velocity.x = currentDirection * obj.maxSpeedX;
	}

	// Add velocity to position which moves the player accross the screen
	obj.position += obj.velocity * deltaTime;

	// Handle collision detection. Decorative layers are explicitly non-collidable, so they are ignored by both collision response and the grounded sensor
	bool foundGround = false;
	for (auto& layer : gs.layers)
	{
		for (GameObject& objB : layer)
		{
			if (&obj != &objB && obj.collidable && objB.collidable)
			{
				checkCollision(state, gs, res, obj, objB, deltaTime);
			}

			if (objB.type != ObjectType::level || !objB.collidable)
			{
				continue;
			}

			// Grounded sensor. Only explicitly collidable terrain can ground the player
			const float inset = 2.0f;
			SDL_FRect sensor{
				.x = obj.position.x + obj.collider.x + inset,
				.y = obj.position.y + obj.collider.y + obj.collider.h,
				.w = std::max(0.0f, obj.collider.w - inset * 2), .h = 1.0f
			};
			SDL_FRect rectB{
				.x = objB.position.x + objB.collider.x,
				.y = objB.position.y + objB.collider.y,
				.w = objB.collider.w, .h = objB.collider.h
			};
			SDL_FRect rectC{ 0 };
			if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &rectC))
			{
				foundGround = true;
			}
		}
	}

	obj.grounded = foundGround;
	if (foundGround && obj.type == ObjectType::player
		&& obj.data.player.state == PlayerState::jumping
		&& obj.velocity.y >= 0) // not rising, so this is a landing, not a takeoff
	{
		obj.data.player.state = PlayerState::running;
	}
}

static void collisionResponse(const SDLState& state, GameState& gs, const Resources& res, SDL_FRect& rectA
	, SDL_FRect& rectB, SDL_FRect& rectC, GameObject& objA, GameObject& objB, float deltaTime)
{
	if (objA.type == ObjectType::player)
	{
		switch (objB.type)
		{
		case ObjectType::level:
		{
			if (rectC.w < rectC.h) // Width collision
			{
				if (objA.velocity.x > 0) // Going right
				{
					objA.position.x -= rectC.w;
					objA.velocity.x = 0;
				}
				else if (objA.velocity.x < 0) // Going left
				{
					objA.position.x += rectC.w;
					objA.velocity.x = 0;
				}
			}
			else
			{
				if (objA.velocity.y > 0)
				{
					objA.position.y -= rectC.h; // Going down
				}
				else if (objA.velocity.y < 0)
				{
					objA.position.y += rectC.h; // Going up
				}
				objA.velocity.y = 0;
			}
			break;
		}
		case ObjectType::nextLevel:
		{
			if (gs.flagCounter == 1)
			{
				objA.velocity.x = 0;
				objA.velocity.y = 0;
				objA.grounded = true;
				gs.level++;
			}
			break;
		}
		case ObjectType::flag:
		{
			// Record collection before disabling collision
			if (objB.data.flag.bearFlag)
				gs.bearFlagCollected = true;
			if (objB.data.flag.chestFlag)
				gs.chestFlagCollected = true;

			// A flag can only increment the counter once
			if (!objB.collidable)
				break;

			objB.collidable = false;
			objB.width = 0;
			objB.height = 0;
			gs.flagCounter++;

			switch (gs.flagCounter)
			{
			case 1:
			{
				gs.setObjective(ObjectiveId::enterTent);
				break;
			}
			case 2:
			{
				gs.setObjective(ObjectiveId::bearDefeated);
				break;
			}
			case 3:
				if (gs.bearFlagCollected && gs.chestFlagCollected)
				{
					gameEnd(gs, "complete");
					break;
				}
				break;
			}
			break;
		}
		case ObjectType::enemy:
		{
			objA.data.player.state = PlayerState::idle;
			objA.texture = res.texIdle;
			objA.currentAnimation = res.ANIM_PLAYER_IDLE;
			gameEnd(gs, "restart");
			break;
		}
		}

	}
	if (objA.type == ObjectType::enemy)
	{
		switch (objB.type)
		{
		case ObjectType::level:
		{
			if (rectC.w < rectC.h) // Width collision
			{
				if (objA.velocity.x > 0) // Going right
				{
					objA.position.x -= rectC.w;
					objA.velocity.x = 0;
					objA.direction = -1;
				}
				else if (objA.velocity.x < 0) // Going left
				{
					objA.position.x += rectC.w;
					objA.velocity.x = 0;
					objA.direction = 1;
				}
			}
			else
			{
				if (objA.velocity.y > 0)
				{
					objA.position.y -= rectC.h; // Going down
				}
				else if (objA.velocity.y < 0)
				{
					objA.position.y += rectC.h; // Going up
				}
				objA.velocity.y = 0;
			}
			break;
		}
		}

	}
}

void checkCollision(const SDLState& state, GameState& gs, Resources& res, GameObject& a
	, GameObject& b, float deltaTime)
{
	// Never test decorative tiles with zero-sized hitboxes
	if (!a.collidable || !b.collidable
		|| a.collider.w <= 0.0f || a.collider.h <= 0.0f
		|| b.collider.w <= 0.0f || b.collider.h <= 0.0f)
	{
		return;
	}


	SDL_FRect rectA{
		.x = a.position.x + a.collider.x,
		.y = a.position.y + a.collider.y,
		.w = a.collider.w,
		.h = a.collider.h
	};
	SDL_FRect rectB{
		.x = b.position.x + b.collider.x,
		.y = b.position.y + b.collider.y,
		.w = b.collider.w,
		.h = b.collider.h
	};
	SDL_FRect rectC{ 0 };

	if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC))
	{
		// Found intersection, respond accordingly
		collisionResponse(state, gs, res, rectA, rectB, rectC, a, b, deltaTime);
	}
}

void createTiles(const SDLState& state, GameState& gs, const Resources& res)
{
	// Remove all objects belonging to the previous level
	gs.layers.clear();

	// Reset the player reference before creating the new level
	gs.playerIndex = -1;
	gs.playerLayer = -1;

	struct LayerVisitor
	{
		const SDLState& state;
		GameState& gs;
		const Resources& res;

		LayerVisitor(const SDLState& state, GameState& gs, const Resources& res) : state(state), gs(gs), res(res) {}

		auto createObject(int r, int c, SDL_Texture* tex, ObjectType type)
		{
			GameObject o;
			o.type = type;
			o.position = glm::vec2(
				c * res.map->tileWidth,
				r * res.map->tileHeight);
			o.texture = tex;
			o.collider = { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
			return o;
		}


		void operator()(tmx::Layer& layer) // Tile layers
		{
			std::vector<GameObject> newLayer;
			int i = 0;
			for (uint32_t raw : layer.data)
			{
				const int tGid = raw & 0x0FFFFFFF; // real tile ID
				const bool flipH = raw & 0x80000000;
				const bool flipV = raw & 0x40000000;
				const bool flipD = raw & 0x20000000;

				if (tGid) // if not an empty slot
				{
					int idx = -1;
					for (int k = 0; k < (int)res.tilesetTextures.size(); ++k)
					{
						if (tGid >= res.tilesetTextures[k].firstGid) idx = k;
					}
					if (idx < 0) { i++; continue; }

					const TileSetTextures& tst = res.tilesetTextures[idx];
					const tmx::TileSet& ts = res.map->tileSets[idx];
					int local = tGid - tst.firstGid;

					SDL_Texture* tex = tst.sheet;
					SDL_FRect srcRect{};
					if (tex)
					{
						int col = local % ts.columns, row = local / ts.columns;
						srcRect = { float(ts.margin + col * (ts.tileWidth + ts.spacing)),
									float(ts.margin + row * (ts.tileHeight + ts.spacing)),
									float(ts.tileWidth), float(ts.tileHeight) };
					}
					else if (local < (int)tst.textures.size())
					{
						tex = tst.textures[local];
						// Draw the entire image depending on width and height
						float iw = 0, ih = 0;
						SDL_GetTextureSize(tex, &iw, &ih);
						srcRect = { 0, 0, iw, ih };
					}
					else { i++; continue; }

					int r = i / res.map->mapWidth;
					int c = i % res.map->mapWidth;

					auto tile = createObject(r, c, tex, ObjectType::level);
					tile.spriteFrame = 1;
					tile.tileSrc = srcRect;
					tile.width = srcRect.w;
					tile.height = srcRect.h;
					// Tiled anchors tiles bigger than the grid at the bottom left of the cell
					tile.position.y -= (srcRect.h - res.map->tileHeight);

					// Layers can be collidable or not based on the collidable parameter in Tiled
					tile.collidable = layer.collidable;

					// Tiled applies the diagonal flip first, then H, then V
					// The equivalent SDL angle and flip combinations are made here
					float angle = 0; SDL_FlipMode flip = SDL_FLIP_NONE;
					if (!flipD) {
						if (flipH && flipV) angle = 180;
						else if (flipH)     flip = SDL_FLIP_HORIZONTAL;
						else if (flipV)     flip = SDL_FLIP_VERTICAL;
					}
					else {
						if (flipH && flipV) { angle = 90;  flip = SDL_FLIP_HORIZONTAL; }
						else if (flipH) { angle = 90; }
						else if (flipH) { angle = 90; }
						else if (flipV) { angle = 270; }
						else { angle = 90;  flip = SDL_FLIP_VERTICAL; }
					}
					tile.tileAngle = angle;
					tile.tileFlip = flip;

					if (!tile.collidable)
					{
						tile.collider = { 0, 0, 0, 0 };
					}
					newLayer.push_back(tile);
				}
				i++;
			}
			gs.layers.push_back(newLayer);
		}
		void operator()(tmx::ObjectGroup& objectGroup) // Object layers
		{
			std::vector<GameObject> newLayer;
			for (tmx::LayerObject& obj : objectGroup.objects)
			{
				glm::vec2 objPos(
					obj.x - res.map->tileWidth / 2,
					obj.y - res.map->tileHeight / 2);

				if (obj.type == "Player")
				{
					// Create player object
					GameObject player = createObject(1, 1, res.texIdle, ObjectType::player);
					player.position = gs.playerInitialPos = objPos;
					player.data.player = PlayerData();
					player.animations = res.playerAnims;
					player.currentAnimation = res.ANIM_PLAYER_IDLE;
					player.acceleration = glm::vec2(300, 0);
					player.maxSpeedX = 200;
					player.dynamic = true;
					player.collidable = true;
					player.collider = {
						.x = 33, .y = 10, .w = 30, .h = 80
					};
					player.width = 96;
					player.height = 96;
					newLayer.push_back(player);
					gs.playerIndex = 0;
					gs.playerLayer = gs.layers.size();
				}
				else if (obj.type == "Enemy")
				{
					// Create bear object
					GameObject bear = createObject(1, 1, res.texBearWalk, ObjectType::enemy);
					bear.position = objPos;
					bear.direction = -1;
					bear.data.enemy = EnemyData();
					bear.currentAnimation = res.ANIM_BEAR_WALK;
					bear.animations = res.bearAnims;
					bear.acceleration = glm::vec2(300, 0);
					bear.maxSpeedX = 100;
					bear.collider = SDL_FRect{
						.x = 30, .y = 100, .w = 200, .h = 150
					};
					bear.width = 256;
					bear.height = 256;
					bear.dynamic = true;
					bear.collidable = true;
					newLayer.push_back(bear);
				}
				else if (obj.type == "Flag")
				{
					if (obj.name == "flag2")
					{
						// Create chest flag object for level 2 which stays hidden until the chest is opened
						GameObject flagChest = createObject(1, 1, res.texFlag, ObjectType::flag);
						flagChest.data.flag = FlagData();
						flagChest.data.flag.chestFlag = true;
						flagChest.position = objPos;
						// Remove animation and set to last frame
						flagChest.currentAnimation = -1;
						flagChest.spriteFrame = 18;
						flagChest.animations = res.flagAnims;
						flagChest.collidable = false;
						flagChest.collider = SDL_FRect{
							.x = 22, .y = 16, .w = 20, .h = 28
						};
						newLayer.push_back(flagChest);
					}
					else if (obj.name == "flag1")
					{
						// Create chest flag object for level 1 which stays hidden until the bear is killed
						GameObject flagBear = createObject(1, 1, res.texFlag, ObjectType::flag);
						flagBear.data.flag = FlagData();
						flagBear.data.flag.bearFlag = true;
						flagBear.position = objPos;
						// Remove animation and set to last frame
						flagBear.currentAnimation = -1;
						flagBear.spriteFrame = 18;
						flagBear.animations = res.flagAnims;
						flagBear.collidable = false;
						flagBear.collider = SDL_FRect{
							.x = 22, .y = 16, .w = 20, .h = 28
						};
						newLayer.push_back(flagBear);
					}
					else
					{
						// Create default flag object
						GameObject flag = createObject(1, 1, res.texFlag, ObjectType::flag);
						flag.data.flag = FlagData();
						flag.position = objPos;
						flag.currentAnimation = res.ANIM_FLAG_FLOAT;
						flag.animations = res.flagAnims;
						flag.collidable = true;
						flag.collider = SDL_FRect{
							.x = 22, .y = 16, .w = 20, .h = 28
						};
						newLayer.push_back(flag);
					}
				}
				else if (obj.type == "NextLevel")
				{
					GameObject nextLevel = createObject(1, 1, nullptr, ObjectType::nextLevel);
					nextLevel.position = objPos;
					nextLevel.collidable = true;
					nextLevel.collider = SDL_FRect{
						.x = 10, .y = 4, .w = 12, .h = 28
					};
					newLayer.push_back(nextLevel);
				}
				else if (obj.type == "Chest")
				{
					GameObject chest = createObject(1, 1, res.texChestClosed, ObjectType::chest);
					chest.data.chest = ChestData();
					chest.position = objPos;
					chest.collidable = false;
					newLayer.push_back(chest);
				}
			}
			gs.layers.push_back(std::move(newLayer));
		}
	};

	for (auto& layer : res.map->layers)
	{
		std::visit(LayerVisitor(state, gs, res), layer);
	}
}

void handleKeyInput(const SDLState& state, GameState& gs, GameObject& obj
	, SDL_Scancode key, bool keyDown)
{
	const float JUMP_FORCE = -275.0f;
	if (obj.type == ObjectType::player)
	{
		switch (obj.data.player.state)
		{
		case PlayerState::idle:
		{
			if (key == SDL_SCANCODE_SPACE && keyDown)
			{
				obj.data.player.state = PlayerState::jumping;
				obj.velocity.y += JUMP_FORCE;
			}
			break;
		}
		case PlayerState::running:
		{
			if (key == SDL_SCANCODE_SPACE && keyDown)
			{
				obj.data.player.state = PlayerState::jumping;
				obj.velocity.y += JUMP_FORCE;
			}
			break;
		}
		}
	}
}

void drawParalaxBackground(float viewportY, SDL_Renderer* renderer, SDL_Texture* texture
	, float xVelocity, float& scrollPos, float scrollFactor, float deltaTime)
{
	scrollPos -= xVelocity * scrollFactor * deltaTime;
	if (scrollPos <= -texture->w)
	{
		scrollPos = 0;
	}

	SDL_FRect dst{
		.x = scrollPos, .y = 750.0f - viewportY,
		.w = texture->w * 2.0f,
		.h = static_cast<float>(texture->h)
	};

	SDL_RenderTextureTiled(renderer, texture, nullptr, 1, &dst);
}

MenuText makeMenuText(SDL_Renderer* renderer, TTF_Font* font, const char* text, float areaW, bool isObjective)
{
	const SDL_Color white{ 255, 255, 255, 255 };

	if (isObjective)
	{
		TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_LEFT);
	}
	else
	{
		TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_CENTER);
	}


	MenuText mt;
	SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(
		font, text, 0, white, static_cast<int>(areaW - 12.0f * 2 - 40));
	if (!surf) return mt;

	mt.tex = SDL_CreateTextureFromSurface(renderer, surf);
	mt.w = static_cast<float>(surf->w);
	mt.h = static_cast<float>(surf->h);
	SDL_DestroySurface(surf);
	return mt;
}

void drawText(SDL_Renderer* renderer, const MenuText& mt, float areaW, float areaH, bool isObjective)
{
	float padding = 12.0f;

	if (!mt.tex) return;

	float x = (areaW - mt.w) / 2.0f;
	float y = (areaH - mt.h) / 2.0f;

	if (isObjective)
	{
		x = 20.0f + padding;
		y = 20.0f + padding;

		SDL_FRect background{
			20.0f,
			20.0f,
			mt.w + padding * 2,
			mt.h + padding * 2
		};

		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
		SDL_RenderFillRect(renderer, &background);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	}

	SDL_FRect dest{ x, y, mt.w, mt.h };
	SDL_RenderTexture(renderer, mt.tex, nullptr, &dest);
}


void gameEnd(GameState& gs, string action) {
	if (action == "restart")
	{
		gs.gameOver = GameOver::restart;
	}
	else if (action == "complete")
	{
		gs.gameOver = GameOver::complete;
	}
}