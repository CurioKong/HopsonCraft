#include "Chunk_Generator.h"

#include <iostream>

#include "../../Util/Hasher.h"
#include "../../Maths/General_Maths.h"

#include "../World_Settings.h"
#include "../Chunk/CFull_Chunk.h"

#include "G_ID.h"

namespace
{
    enum Biomee
    {
        Grassland,
        Forest,
        Mountains,
        Ocean,
        Desert,
    };

    Biomee getBiome(int val)
    {
        if (val > 230)
        {
            return Mountains;
        }
        else if (Maths::inRange(val, 185, 230))
        {
            return Forest;
        }
        else if (Maths::inRange(val, 150, 185))
        {
            return Grassland;
        }
        else
        {
            return Ocean;
        }
    }

    Noise::Data& getBiomeNoise (Biomee b)
    {
                                        //Octaves   amplitude   roughness,  smoothness  height offset
        static Noise::Data forest       {5,         100,        0.52,       230         -40     };
        static Noise::Data desert       {5,         93,         0.45,       230         -20     };
        static Noise::Data grassland    {7,         85,         0.51,       235,        -10     };
        static Noise::Data mountains    {8,         550,        0.50,       280,        -395    };
        //static Noise::Data mountains    {8,         350,        0.50,       283,        -320    };
        static Noise::Data ocean        {7,         43,         0.5,        55,         0};

        switch(b)
        {
            case Biomee::Desert:
                return desert;
//
            case Biomee::Forest:
                return forest;

            case Biomee::Grassland:
                return grassland;

            case Biomee::Mountains:
                return mountains;

            case Biomee::Ocean:
                return ocean;
        }
        return ocean;
    }

}


Chunk_Generator::Chunk_Generator(const World_Settings& worldSettings)
:   m_worldGenType      ("Classic")
,   m_pWorldSettings    (&worldSettings)
{
    m_heightGen.setSeed            (worldSettings.seed);
    m_heightGen.setNoiseFunction   (worldSettings.noiseData);

    m_biomeNoise.setSeed            (worldSettings.seed);
    m_biomeNoise.setNoiseFunction   (m_worldGenType.getBiomeMapNoise());
}

void Chunk_Generator::reset()
{
    m_maxHeight = 0;
    m_heightMap.reset();
    m_biomeMap.reset();
    m_oakTreeLocations.clear();
    m_littleBlockLoc.clear();
}


void Chunk_Generator::generateBlocksFor(Chunk::Full_Chunk& chunk)
{
    //Only one chunk can generate a time smh
    m_genMutex.lock();

    m_pChunk = &chunk;
    reset();

    //Makes a superflat world
    if (m_pWorldSettings->isSuperFlat)
    {
        makeSuperFlatWorld();
        m_genMutex.unlock();
        return;
    }
    else
    {
        makeRegularWorld();
    }

    chunk.setHasGeneratedFlag();
    m_genMutex.unlock();
}

void Chunk_Generator::makeSuperFlatWorld()
{
    for (int y = 0; y < CHUNK_SIZE; ++y)
    for (int x = 0; x < CHUNK_SIZE; ++x)
    for (int z = 0; z < CHUNK_SIZE; ++z)
    {
        Block::ID block = Block::ID::Grass;
        if (y == CHUNK_SIZE - 1)
        {
            block = Block::ID::Grass;
        }
        else if (Maths::inRange(y, 12, 15))
        {
            block = Block::ID::Dirt;
        }
        else
        {
            block = Block::ID::Stone;
        }

        m_pChunk->qSetBlock({x, y, z}, block);
    }
    m_pChunk->setHasGeneratedFlag();
}

void Chunk_Generator::makeRegularWorld()
{
    setRandomSeed   ();
    makeBiomeMap    ();

    m_pWorldSettings->isExperimentalMode ?
            makeAdvancedHeigtMap() :
            makeHeightMap();

    m_maxHeight = std::max(m_maxHeight, WATER_LEVEL);

    for (int y = 0; y < m_maxHeight + 1; ++y)
    for (int x = 0; x < CHUNK_SIZE; ++x)
    for (int z = 0; z < CHUNK_SIZE; ++z)
    {
        auto block = getBlock({x, y, z});
        if (block != Block::ID::Air)
        {
            m_pChunk->qSetBlock({x, y, z}, block);
        }
    }

    for (auto& pos : m_littleBlockLoc)
    {
        m_pChunk->qSetBlock({pos.first.x, pos.first.y + 1, pos.first.z}, pos.second);
    }

    for (auto& pos : m_oakTreeLocations)
    {
        makeOakTree(*m_pChunk, pos, m_randomGenerator);
    }
}


Block::ID Chunk_Generator::getBlock(const Block::Position& pos)
{
    auto blockID    = Block::ID::Air;
    int heightHere  = m_heightMap.at(pos.x, pos.z);
    int y           = pos.y;

    if (y > heightHere)
    {
        if (y > WATER_LEVEL)
            return Block::ID::Air;
        else
            blockID = Block::ID::Water;
    }
    else if (y == heightHere)
    {
        setTopBlock(pos, blockID);
    }
    else if (Maths::inRange(y, heightHere - 3, heightHere))
    {
        blockID = Block::ID::Dirt;
    }
    else if (y <= heightHere - 4)
    {
        blockID = Block::ID::Stone;
    }

    return blockID;
}

void Chunk_Generator::setTopBlock(const Block::Position& pos, Block::ID& blockID)
{

    int y = pos.y;
    int biomeKey = m_biomeMap.at(pos.x, pos.z);

    const Biome& biome = m_worldGenType.getBiome(biomeKey);

    if (y >= WATER_LEVEL)   //Above water
    {
        if (y <= BEACH_LEVEL)   //Beach
        {
            blockID = Block::ID::Sand;
        }
        else //Ground blocks
        {
            blockID = (Block::ID)biome.getSurfaceBlock(m_randomGenerator).id;
        }
    }
    else //Underwater
    {
        blockID = m_randomGenerator.intInRange(0, 5) > 1 ?
            Block::ID::Sand :
            Block::ID::Dirt;
    }
}


void Chunk_Generator::setRandomSeed()
{
    m_randomGenerator.setSeed(Hasher::hash((int32_t)m_pWorldSettings->seed,
                                           m_pChunk->getPosition().x,
                                           m_pChunk->getPosition().y));
}

void Chunk_Generator::makeHeightMap()
{
    for (int32_t x = 0 ; x < CHUNK_SIZE; ++x)
    for (int32_t z = 0 ; z < CHUNK_SIZE; ++z)
    {
        int32_t height =
            m_heightGen.getValue(x, z,
                                    m_pChunk->getPosition().x,
                                    m_pChunk->getPosition().y);
        m_heightMap.at(x, z) = height;
        m_maxHeight = std::max(m_maxHeight, height);
    }
}



void Chunk_Generator::advancedHeightSection(int xMin, int zMin, int xMax, int zMax)
{

    auto getVal = [&](int indexA, int indexB)
    {
        m_heightGen.setNoiseFunction(m_worldGenType.getBiome(m_biomeMap.at(indexA, indexB)).getNoise());
        return m_heightGen.getValue(indexA,
                                    indexB,
                                    m_pChunk->getPosition().x,
                                    m_pChunk->getPosition().y);

    };

    int bottomLeft  = getVal(xMin, zMin);
    int bottomRight = getVal(xMax, zMin);
    int topLeft     = getVal(xMin, zMax);
    int topRight    = getVal(xMax, zMax);

    int itr = 0;
    for (int32_t x = xMin ; x < xMax; ++x)
    for (int32_t z = zMin ; z < zMax; ++z)
    {
        itr++;
        int height =
            Maths::bilinearInterpolate(bottomLeft, topLeft, bottomRight, topRight, //The values to interpolate between
                                       xMin, xMax,             //X range of the values
                                       zMin, zMax,             //Z Range of the values
                                       x, z);               //X and Z position to find value for

        m_heightMap.at(x, z) = height;
        m_maxHeight = std::max(m_maxHeight, height);
    }
}

//This uses interpolation to um interpolate between values rather
//than using noise function on every point.
void Chunk_Generator::makeAdvancedHeigtMap()
{
    advancedHeightSection(0, 0, 8,  8);
    advancedHeightSection(8, 0, 16, 8);
    advancedHeightSection(0, 8, 8,  16);
    advancedHeightSection(8, 8, 16, 16);
    advancedHeightSection(4, 4, 12, 12);
}

void Chunk_Generator::makeBiomeMap()
{
    for (int32_t x = 0 ; x < CHUNK_SIZE + 1; ++x)
    for (int32_t z = 0 ; z < CHUNK_SIZE + 1; ++z)
    {
        int32_t height =
            m_biomeNoise.getValue(x, z,
                                m_pChunk->getPosition().x,
                                m_pChunk->getPosition().y);
        m_biomeMap.at(x, z) = height;
    }
}

