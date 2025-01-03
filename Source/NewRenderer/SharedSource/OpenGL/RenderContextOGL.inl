#pragma once

template<GeometryPassType PASSTYPE>
void RenderContextOGL::RenderInstance(const RenderableInstance* instance, TransparencyMode transparencyFilter)
{
    const Renderable* renderable = instance->m_renderable;
    NativeModelMeshObjectOGL* nativeMesh = renderable->GetNativeRuntimeObj<NativeModelMeshObjectOGL>();
    Renderer::InstanceData& inst(m_rendererOGL.m_instanceData);
    Renderer::InstanceMaterialData& instMaterial(m_rendererOGL.m_instanceMaterialData);

    if (PASSTYPE == GeometryPassType::Sky)
    {
        Matrix4 transform = instance->m_transform;
        const Camera* camera = m_currentCamera;
        assert(camera != nullptr);
        transform.SetTranslation(camera->GetCameraPosition());
        inst.m_modelToWSMat.Set(transform);
    }
    else
    {
        inst.m_modelToWSMat.Set(instance->m_transform);
    }
    inst.m_uniformScaling = instance->m_uniformScale;
    inst.m_vertexOfs = renderable->m_vertexOfs;
	renderable->m_objectTint.ToFloat(&inst.m_objectTint.x, 1.f);

#ifdef _DEBUG
    if (instance->m_debugColorTint)
    {
        inst.debugColorTint = 1.f;
    }
    else
    {
        inst.debugColorTint = 0.f;
    }
#endif
    if (instance->m_blendToRenderable != nullptr && instance->m_blendWeight != 0.f)
    {
        inst.m_vertexOfs2 = instance->m_blendToRenderable->m_vertexOfs;
        inst.m_blendWeight = instance->m_blendWeight;
    }
    else
    {
        inst.m_vertexOfs2 = -1;
        inst.m_blendWeight = 0.f;
    }


    // Prepare the constant buffer to send it to the graphics device.
    m_rendererOGL.m_instance.Update(inst);
    m_rendererOGL.m_instance.Bind(1);
    m_rendererOGL.m_instanceMaterial.Bind(4);

    if (m_previousMesh != nativeMesh)
    {
        nativeMesh->Bind();
        m_previousMesh = nativeMesh;
    }

    // Draw the objects.
    for (int i = 0; i < renderable->m_drawCalls.size(); i++)
    {
        const DrawCall& dc(renderable->m_drawCalls[i]);
        const DrawCallMaterial& material = (i < instance->m_materialOverride.size()) ? instance->m_materialOverride[i] : dc.m_material;
        if (material.m_transparencyType != transparencyFilter)
        {
            continue;
        }

        if (PASSTYPE != GeometryPassType::MirrorReceiver)
        {
            Texture* tex = material.m_texture;
            if (tex == nullptr)
            {
                //assert(false); // should not get here
                continue;
            }

            NativeTextureObjectOGL* nativeTex = tex->GetNativeRuntimeObj<NativeTextureObjectOGL>();
            assert(nativeTex != nullptr);

            if (nativeTex != m_previousTexture)
            {
                nativeTex->Bind(0);
                m_previousTexture = nativeTex;
            }
        }

        if (material.m_materialHash != m_lastMaterialHash)
        {
            instMaterial.m_applyLighting = material.m_applyLighting ? 1.f : 0.f;
            instMaterial.m_applyLighting = 0.f; // REMOVE
            instMaterial.m_transparency = material.m_opacity;
            m_rendererOGL.m_instanceMaterial.Update(instMaterial);
        }

        SubmitDrawcall(dc.m_firstIndex, dc.m_primitiveCount);
    }
}
