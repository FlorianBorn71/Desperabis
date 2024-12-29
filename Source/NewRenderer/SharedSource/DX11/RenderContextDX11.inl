#pragma once

#include <DX11/RendererDX11.h>

template<GeometryPassType PASSTYPE>
void RenderContextDX11::RenderInstance(const RenderableInstance* instance, TransparencyMode transparencyFilter)
{
    const Renderable* renderable = instance->GetRenderable();
    NativeModelMeshObjectDX11* nativeMesh = renderable->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
    Renderer::InstanceData& inst(m_rendererDX11.m_instanceData);
    Renderer::InstanceMaterialData& instMaterial(m_rendererDX11.m_instanceMaterialData);

    if (PASSTYPE == GeometryPassType::Sky)
    {
        Matrix4 transform = instance->GetTransform();
        const Camera* camera = m_currentCamera;
        assert(camera != nullptr);
        transform.AddTranslation(camera->GetCameraPosition()); // the existing object translation determines the position of moon for instance
        inst.m_modelToWSMat.SetTransposed(transform);
    }
    else
    {
        inst.m_modelToWSMat.SetTransposed(instance->GetTransform());
    }
    inst.m_uniformScaling = instance->m_uniformScale;
    inst.m_vertexOfs = renderable->m_vertexOfs;
    instance->m_tintColor.ToFloat(&inst.m_objectTint.x, 1.f);
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
    if (instance->GetBlendTargetRenderable() != nullptr && instance->m_blendWeight != 0.f)
    {
        inst.m_vertexOfs2 = instance->GetBlendTargetRenderable()->m_vertexOfs;
        inst.m_blendWeight = instance->m_blendWeight;
    }
    else
    {
        inst.m_vertexOfs2 = -1;
        inst.m_blendWeight = 0.f;
    }

    // Prepare the constant buffer to send it to the graphics device.
    m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferInstance.Get(), 0, nullptr, &inst, 0, 0, 0);

    // Send the constant buffer to the graphics device.
    m_d3dContext->VSSetConstantBuffers1(2, 1, nativeMesh->m_positionBuffer.GetAddressOf(), nullptr, nullptr);

    if (PASSTYPE == GeometryPassType::InstancesInFlames)
    {
        // mesh
		NativeModelMeshObjectDX11* nativeFlameMesh = AssetManager::Instance().GetBurningObjectRenderable()->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
		UINT stride = sizeof(ModelVertex);
		UINT offset = 0;
		m_d3dContext->IASetVertexBuffers(0, 1, nativeFlameMesh->m_vertexBuffer.GetAddressOf(), &stride, &offset);
		m_d3dContext->IASetIndexBuffer(nativeFlameMesh->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        // texture
        const int frameCount = AssetManager::Instance().GetNumFlameFrames();
        for (int i = 0; i < frameCount; i++)
        {
			Texture* tex = AssetManager::Instance().GetFlameTexture(i);
            if (i == 0)
            {
                // TODO: need to track a n-times constant buffer into slot 3
				NativeTextureObjectDX11* nativeTex = tex->GetNativeRuntimeObj<NativeTextureObjectDX11>();
				if (nativeTex == nullptr) // only gets here for highres-only quality
					continue;
				// bind texture and constant buffer that holds the atlas data
				m_d3dContext->PSSetShaderResources(0, 1, nativeTex->m_nativeTexView.GetAddressOf());
				m_d3dContext->VSSetConstantBuffers1(3, 1, nativeTex->m_atlasDataBuffer.GetAddressOf(), nullptr, nullptr);
				m_previousTexture = nativeTex;
            }
        }

        // material:
		instMaterial.m_applyLighting = 0.f;
		instMaterial.m_transparency = instance->m_fireIntensity;
		m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferMaterial.Get(), 0, nullptr, &instMaterial, 0, 0, 0);

        int firstIdx = 0; // since we track vertexOfs above, don't add anything on top, but rather start at #0 >>  renderable->m_positionOfs * 6; // 6 indices per quad
    	SubmitDrawcall(firstIdx, renderable->m_numPositions * 2);

		m_previousMesh = nullptr;
        return;
    } // InstancesInFlames

    if (m_previousMesh != nativeMesh)
    {
        // Each vertex is one instance of the ModelVertex struct.
        UINT stride = sizeof(ModelVertex);
        UINT offset = 0;
        m_d3dContext->IASetVertexBuffers(0, 1, nativeMesh->m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_d3dContext->IASetIndexBuffer(nativeMesh->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
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

            NativeTextureObjectDX11* nativeTex = tex->GetNativeRuntimeObj<NativeTextureObjectDX11>();
            if (nativeTex == nullptr) // only gets here for highres-only quality
                continue;

            if (nativeTex != m_previousTexture)
            {
                // bind texture and constant buffer that holds the atlas data
                m_d3dContext->PSSetShaderResources(0, 1, nativeTex->m_nativeTexView.GetAddressOf());
                m_d3dContext->VSSetConstantBuffers1(3, 1, nativeTex->m_atlasDataBuffer.GetAddressOf(), nullptr, nullptr);
                m_previousTexture = nativeTex;
            }
        }

        if (material.m_materialHash != m_lastMaterialHash)
        {
            instMaterial.m_applyLighting = material.m_applyLighting ? 1.f : 0.f;
            //instMaterial.m_applyLighting = 0.f; // REMOVE
            instMaterial.m_transparency = material.m_opacity;
            m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferMaterial.Get(), 0, nullptr, &instMaterial, 0, 0, 0);
            assert(material.m_materialHash != 0 && material.m_materialHash != 0xffffffff);
            m_lastMaterialHash = material.m_materialHash;
        }

        SubmitDrawcall(dc.m_firstIndex, dc.m_primitiveCount);
    }
}
