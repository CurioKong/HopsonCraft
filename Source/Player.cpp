#include "Player.h"

#include "Camera.h"

#include "World/Chunk/CMap.h"

#include <iostream>

Player::Player(Camera& camera)
:   m_p_camera  (&camera)
,   box         ({0.5, 0.5, 0.5})
{
    position = {0, 100, 0};
}

void Player::addForce(const Vector3& force)
{
    m_velocity += force;
}

void Player::testForCollide(Chunk::Map& chunkMap, float dt)
{
    //std::cout << position.x << "\n";
/*
    box.update(position + (m_velocity * dt));

    if(chunkMap.getBlockAt(Vector3{position.x, position.y, position.x} + m_velocity.x * dt) != Block::ID::Air)
    {
        m_velocity.x = 0;
    }*/
}



void Player::input()
{
    addForce(m_p_camera->input());
    rotation = m_p_camera->rotation;
    m_p_camera->position = position;
}

void Player::update(float dt)
{
    position += m_velocity * dt;

    if (position.x < 0.2)
    {
        position.x = 0.2;
    }
    if (position.z < 0.2)
    {
        position.z = 0.2;
    }

    m_velocity *= 0.95;
}
