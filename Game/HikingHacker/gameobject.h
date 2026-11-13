#pragma once

#include <glm/glm.hpp>
#include <SDL3_image/SDL_image.h>
#include "animation.h"


enum class PlayerState
{
	idle, running, jumping
};

enum class EnemyState
{
	walking, dead
};

enum class ChestState
{
	closed, open
};

struct PlayerData
{
	PlayerState state;
	PlayerData()
	{
		state = PlayerState::idle;
	}
};

struct ChestData
{
	ChestState state;
	bool isOpen;
	ChestData()
	{
		state = ChestState::closed;
		isOpen = false;
	}
};

struct FlagData
{
	bool chestFlag;
	bool bearFlag;
	FlagData()
	{
		chestFlag = false;
		bearFlag = false;
	}
};

struct LevelData {};

struct EnemyData
{
	EnemyState state;
	int healthPoints;
	EnemyData()
	{
		state = EnemyState::walking;
		healthPoints = 1337;
	}
};

union ObjectData
{
	PlayerData player;
	LevelData level;
	EnemyData enemy;
	ChestData chest;
	FlagData flag;
};


enum class ObjectType
{
	player, level, enemy, flag, nextLevel, chest
};

struct GameObject
{
	ObjectType type;
	ObjectData data;
	glm::vec2 position, velocity, acceleration;
	float direction;
	float maxSpeedX;
	std::vector<Animation> animations;
	int currentAnimation;
	SDL_Texture* texture;
	bool dynamic;
	bool grounded;
	// Collision is opt-in: decorative/background tiles must never enter collision tests.
	bool collidable;
	SDL_FRect collider;
	int spriteFrame;
	SDL_FRect tileSrc;
	float tileAngle;
	SDL_FlipMode tileFlip;

	float width;
	float height;


	GameObject() : data{ .level = LevelData() }, collider{ 0 }
	{
		type = ObjectType::level;
		direction = 1;
		maxSpeedX = 0;
		position = velocity = acceleration = glm::vec2(0);
		currentAnimation = -1;
		texture = nullptr;
		dynamic = false;
		grounded = false;
		collidable = false;
		spriteFrame = 1;
		tileSrc = { 0, 0, 0, 0 };
		tileAngle = 0;
		tileFlip = SDL_FLIP_NONE;
		width = 64.0f;
		height = 64.0f;
	}
};