
#include "Room.h"

#include <algorithm>

#include <sstream>

static Room boxesToRoom(std::vector<Box> boxes);

RoomGenerator::RoomGenerator(int seed)
{
	generator.seed(seed);
}

Room RoomGenerator::generate()
{
	const int minimumArea = 2500;
	const int minBoxSize = 5;
	const int maxBoxSize = 15;

	unsigned currentArea = 0;
	std::uniform_int_distribution<int> boxSizeRand(minBoxSize, maxBoxSize);
	// Right-pointing-down, right-pointing-up, left-pointing-down, left-pointing-up,
	// bot-pointing-right, bot-pointing-left, top-pointing-right, top-pointing-left
	std::uniform_int_distribution<int> dirRand(0, 7);

	unsigned maxRighti = 0;
	unsigned maxLefti = 0;
	unsigned maxBoti = 0;
	unsigned maxTopi = 0;
	std::vector<Box> boxes;

	// Generate the root box
	glm::ivec2 size(boxSizeRand(this->generator), boxSizeRand(this->generator));
	currentArea += size.x * size.y;
	boxes.push_back(Box());
	boxes[0].right = std::ceil(size.x / 2.0f);
	boxes[0].top = std::ceil(size.y / 2.0f);
	boxes[0].left = -std::floor(size.x / 2.0f);
	boxes[0].bottom = -std::floor(size.y / 2.0f);

	int i = 0;
	while (currentArea < minimumArea) {
		Box newBox;
		glm::ivec2 size(boxSizeRand(this->generator), boxSizeRand(this->generator));
		currentArea += size.x * size.y;

		unsigned direction = dirRand(this->generator);
		Box matchingBox;
		if (direction < 2) {
			// Match left to right
			matchingBox = boxes[maxRighti];
			newBox.left = matchingBox.right;
			newBox.right = newBox.left + size.x;
		} else if (direction < 4) {
			// Match right to left
			matchingBox = boxes[maxLefti];
			newBox.right = matchingBox.left;
			newBox.left = newBox.right - size.x;
		} else if (direction < 6) {
			// Match top to bottom
			matchingBox = boxes[maxBoti];
			newBox.top = matchingBox.bottom;
			newBox.bottom = newBox.top - size.y;
		} else {
			// Match bottom to top
			matchingBox = boxes[maxTopi];
			newBox.bottom = matchingBox.top;
			newBox.top = newBox.bottom + size.y;
		}

		if (direction < 4) {
			if (direction % 2 == 0) {
				// Match top to top
				newBox.top = matchingBox.top;
				newBox.bottom = newBox.top - size.y;
			} else {
				// Match bottom to bottom
				newBox.bottom = matchingBox.bottom;
				newBox.top = newBox.bottom + size.y;
			}
		} else {
			if (direction % 2 == 0) {
				// Match left to left
				newBox.left = matchingBox.left;
				newBox.right = newBox.left + size.x;
			} else {
				// Match right to right
				newBox.right = matchingBox.right;
				newBox.left = newBox.right - size.x;
			}
		}

		if (newBox.right > boxes[maxRighti].right) {
			maxRighti = boxes.size();
		}
		if (newBox.left < boxes[maxLefti].left) {
			maxLefti = boxes.size();
		}
		if (newBox.bottom < boxes[maxBoti].bottom) {
			maxBoti = boxes.size();
		}
		if (newBox.top > boxes[maxTopi].top) {
			maxTopi = boxes.size();
		}

		boxes.push_back(newBox);

		std::stringstream sstream;
		sstream << "room" << i << ".bmp";
		Room room = boxesToRoom(boxes);
		SDL_Surface* surf = room.saveToSurface();
		SDL_SaveBMP(surf, sstream.str().c_str());
		SDL_FreeSurface(surf);
		i++;
	}

	return boxesToRoom(boxes);
}

SDL_Surface* Room::saveToSurface()
{
	SDL_Surface* surface = SDL_CreateRGBSurface(0, this->maxX - this->minX + 1, this->maxY - this->minY + 1, 32, 0, 0, 0, 0);
	SDL_Rect full {0, 0, surface->w, surface->h};
	SDL_FillRect(surface, &full, SDL_MapRGB(surface->format, 0, 0, 0));

	std::default_random_engine generator;
	std::uniform_int_distribution<int> colorRand(127, 255);
	for (unsigned i = 0; i < this->sides.size(); i++) {
		RoomSide side = this->sides[i];
		SDL_Rect rect;

		rect.x = side.x0 - this->minX;
		rect.y = side.y0 - this->minY;
		if (side.x0 == side.x1) {
			// Vertical
			rect.w = 1;
			rect.h = std::abs(side.y1 - side.y0);
		} else {
			// Horizontal
			rect.w = std::abs(side.x1 - side.x0);
			rect.h = 1;
		}
		SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, colorRand(generator), colorRand(generator), colorRand(generator)));
	}
	return surface;
}

static Room boxesToRoom(std::vector<Box> boxes)
{
	Room room;
	room.boxes = boxes;
	room.minX = room.minY = INT_MAX;
	room.maxX = room.maxY = INT_MIN;
	for (unsigned i = 0; i < boxes.size(); i++) {
		Box box = boxes[i];
		room.minX = std::min(box.left, room.minX);
		room.maxX = std::max(box.right, room.maxX);
		room.minY = std::min(box.bottom, room.minY);
		room.maxY = std::max(box.top, room.maxY);

		room.sides.push_back({ box.left, box.top, box.right, box.top, glm::ivec2(0.0f, -1.0f) });
		room.sides.push_back({ box.right, box.bottom, box.right, box.top, glm::ivec2(-1.0f, 0.0f) });
		room.sides.push_back({ box.left, box.bottom, box.right, box.bottom, glm::ivec2(0.0f, 1.0f) });
		room.sides.push_back({ box.left, box.bottom, box.left, box.top, glm::ivec2(1.0f, 0.0f) });
	}

	for (unsigned i = 0; i < room.sides.size(); i++) {
		RoomSide& side = room.sides[i];
		for (unsigned j = 0; j < room.sides.size(); j++) {
			RoomSide& otherSide = room.sides[j];
			if (i == j || (otherSide.x0 == otherSide.x1 && otherSide.y0 == otherSide.y1)) {
				continue;
			}
			
			// Check if we merge
			if (side.y0 == side.y1 && otherSide.y0 == otherSide.y1 && side.y0 == otherSide.y0) {
				// Horizontal match
				if (side.x0 <= otherSide.x0 && side.x1 >= otherSide.x1) {
					// This side subsumes the other side
					int tmp = side.x1;
					side.x1 = otherSide.x0;
					otherSide.x0 = otherSide.x1;
					otherSide.x1 = tmp;
					otherSide.normal = side.normal;
				} else if (side.x0 >= otherSide.x0 && side.x1 <= otherSide.x1) {
					// The other side subsumes us
					int tmp = otherSide.x1;
					otherSide.x1 = side.x0;
					side.x0 = side.x1;
					side.x1 = tmp;
					side.normal = otherSide.normal;
				} else if (side.x0 > otherSide.x0 && side.x0 < otherSide.x1 && side.x1 > otherSide.x1) {
					// Left point is contained in the other side
					int tmp = otherSide.x1;
					otherSide.x1 = side.x0;
					side.x0 = tmp;
				} else if (side.x1 > otherSide.x0 && side.x1 < otherSide.x1 && side.x0 < otherSide.x0) {
					// Right point is contained in the other side
					int tmp = side.x1;
					side.x1 = otherSide.x0;
					otherSide.x0 = tmp;
				}
			}

			if (side.x0 == side.x1 && otherSide.x0 == otherSide.x1 && side.x0 == otherSide.x0) {
				// Vertical match
				if (side.y0 <= otherSide.y0 && side.y1 >= otherSide.y1) {
					// This side subsumes the other side
					int tmp = side.y1;
					side.y1 = otherSide.y0;
					otherSide.y0 = otherSide.y1;
					otherSide.y1 = tmp;
					otherSide.normal = side.normal;
				} else if (side.y0 >= otherSide.y0 && side.y1 <= otherSide.y1) {
					// The other side subsumes us
					int tmp = otherSide.y1;
					otherSide.y1 = side.y0;
					side.y0 = side.y1;
					side.y1 = tmp;
					side.normal = otherSide.normal;
				} else if (side.y0 > otherSide.y0 && side.y0 < otherSide.y1 && side.y1 > otherSide.y1) {
					// Top point is contained in the other side
					int tmp = otherSide.y1;
					otherSide.y1 = side.y0;
					side.y0 = tmp;
				} else if (side.y1 > otherSide.y0 && side.y1 < otherSide.y1 && side.y0 < otherSide.y0) {
					// Bottom point is contained in the other side
					int tmp = side.y1;
					side.y1 = otherSide.y0;
					otherSide.y0 = tmp;
				}
			}
		}
	}

	// Postprocessing - remove empty sides
	room.sides.erase(std::remove_if(room.sides.begin(), room.sides.end(), [] (const RoomSide& side) {
		return side.x0 == side.x1 && side.y0 == side.y1;
	}), room.sides.end());

	return room;
}
