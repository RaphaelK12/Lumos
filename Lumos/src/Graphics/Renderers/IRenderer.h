#pragma once
#include "lmpch.h"
#include "RenderCommand.h"
#include "Maths/Maths.h"
#include "Maths/Transform.h"

namespace Lumos
{
	class RenderList;
	class Scene;
	class Camera;

	namespace Graphics
	{
		class Pipeline;
		class DescriptorSet;
		class RenderPass;
		class Framebuffer;
		class TextureCube;
		class Texture;
		class Shader;
    	class Material;

		typedef std::vector<RenderCommand> CommandQueue;
		typedef std::vector<RendererUniform> SystemUniformList;

		class LUMOS_EXPORT IRenderer
		{
		public:
			virtual ~IRenderer();
			virtual void RenderScene(Scene* scene) = 0;
			virtual void Init() = 0;
			virtual void Begin() = 0;
			virtual void BeginScene(Scene* scene, Camera* overrideCamera, Maths::Transform* overrideCameraTransform) = 0;
			virtual void Submit(const RenderCommand& command) {};
			virtual void SubmitMesh(Mesh* mesh, Material* material, const Maths::Matrix4& transform, const Maths::Matrix4& textureMatrix) {};
			virtual void EndScene() = 0;
			virtual void End() = 0;
			virtual void Present() = 0;
			virtual void PresentToScreen() = 0;
			virtual void OnResize(u32 width, u32 height) = 0;
			virtual void OnImGui() {};

			virtual void SetScreenBufferSize(u32 width, u32 height)
			{
				if(width == 0)
					width = 1;
				if(height == 0)
					height = 1;
				m_ScreenBufferWidth = width;
				m_ScreenBufferHeight = height;
			}

			virtual void SetRenderTarget(Graphics::Texture* texture, bool rebuildFramebuffer = true)
			{
				m_RenderTexture = texture;
			}

			Texture* GetRenderTarget() const
			{
				return m_RenderTexture;
			}

			const Ref<Shader>& GetShader() const
			{
				return m_Shader;
			}

			void SetCamera(Camera* camera, Maths::Transform* transform)
			{
				m_Camera = camera;
				m_CameraTransform = transform;
			}

		protected:
			Camera* m_Camera = nullptr;
			Maths::Transform* m_CameraTransform = nullptr;

			std::vector<Ref<Framebuffer>> m_Framebuffers;
			std::vector<Ref<CommandBuffer>> m_CommandBuffers;
			Ref<Shader> m_Shader = nullptr;

			Ref<Lumos::Graphics::RenderPass> m_RenderPass;
			Ref<Lumos::Graphics::Pipeline> m_Pipeline;
			Ref<Graphics::DescriptorSet> m_DescriptorSet;

			u32 m_ScreenBufferWidth = 0, m_ScreenBufferHeight = 0;
			CommandQueue m_CommandQueue;
			SystemUniformList m_SystemUniforms;
			Texture* m_RenderTexture = nullptr;
			Texture* m_DepthTexture = nullptr;

			Maths::Frustum m_Frustum;

			u8* m_VSSystemUniformBuffer = nullptr;
			u32 m_VSSystemUniformBufferSize = 0;
			u8* m_PSSystemUniformBuffer = nullptr;
			u32 m_PSSystemUniformBufferSize = 0;

			std::vector<u32> m_VSSystemUniformBufferOffsets;
			std::vector<u32> m_PSSystemUniformBufferOffsets;
			Maths::Vector4 m_ClearColour;

		};
	}
}
