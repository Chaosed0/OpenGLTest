
#include "Terrain.h"

#include <algorithm>

Terrain::Terrain()
{
	patchSize = 512;
	stepSize = 0.001;
}

Terrain::Terrain(int seed)
{
	noise.SetSeed(seed);
}

Terrain::Terrain(unsigned patchSize, float stepSize, unsigned octaves, float baseFrequency, float persistence, int seed)
	: patchSize(patchSize),
	stepSize(stepSize)
{
	noise.SetOctaveCount(octaves);
	noise.SetFrequency(baseFrequency);
	noise.SetPersistence(persistence);
	noise.SetSeed(seed);
}

TerrainPatch Terrain::generatePatch(int x, int y)
{
	glm::vec2 start(x * (int)patchSize * stepSize, y * (int)patchSize * stepSize);
	TerrainPatch patch;
	patch.size.x = patchSize;
	patch.size.y = patchSize;
	patch.terrain.resize(patchSize*patchSize);
	patch.min = FLT_MAX;
	patch.max = FLT_MIN;

	for (unsigned y = 0; y < patchSize; y++) {
		for (unsigned x = 0; x < patchSize; x++) {
			float value = noise.GetValue(start.x + x * stepSize, start.y + y * stepSize, 0.0);
			patch.terrain[y * patchSize + x] = value;
			patch.min = min(value, patch.min);
			patch.max = max(value, patch.max);
		}
	}

	return patch;
}

void Terrain::setPatchSize(unsigned patchSize)
{
	this->patchSize = patchSize;
}
