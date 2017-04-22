#include "RLiquid.h"

#include <iostream>

#include "../Maths/Matrix_Maths.h"

#include "../Camera.h"

#include "../World/Chunk/CSection.h"

namespace Renderer
{
    void RLiquid::draw(const Chunk::Section& section)
    {
        if (section.getMeshes().liquidMesh.getFaceCount() > 0)
        {
            m_renderInfo.emplace_back(section.getMeshes().liquidMesh.getModel().getVAO(),
                                      section.getMeshes().liquidMesh.getModel().getIndicesCount());
        }
    }

    void RLiquid::update(const Camera& camera)
    {
        glDisable (GL_CULL_FACE);
        glEnable  (GL_BLEND);

        m_shader.bind();

        m_shader.setProjMatrix(camera.getProjectionMatrix());
        m_shader.setViewMatrix(camera.getViewMatrix());
        m_shader.loadWaveTimer(m_waveTimer.getElapsedTime().asSeconds());

        for (const auto& renderInfo : m_renderInfo)
        {
            glBindVertexArray(renderInfo.vao);

            glDrawElements(GL_TRIANGLES,
                           renderInfo.indexCount,
                           GL_UNSIGNED_INT,
                           nullptr);

        }
        m_renderInfo.clear();
    }
}
