#include "lmpch.h"
#include "Scene.h"
#include "Core/OS/Input.h"
#include "Core/Application.h"
#include "Graphics/API/GraphicsContext.h"
#include "Graphics/Layers/LayerStack.h"
#include "Graphics/RenderManager.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Sprite.h"
#include "Utilities/TimeStep.h"
#include "Audio/AudioManager.h"
#include "Physics/LumosPhysicsEngine/SortAndSweepBroadphase.h"
#include "Physics/LumosPhysicsEngine/Octree.h"
#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"
#include "Physics/LumosPhysicsEngine/SphereCollisionShape.h"
#include "Physics/LumosPhysicsEngine/CuboidCollisionShape.h"
#include "Physics/LumosPhysicsEngine/PyramidCollisionShape.h"

#include "Graphics/Layers/LayerStack.h"
#include "Maths/Transform.h"
#include "Core/OS/FileSystem.h"
#include "Scene/Component/Components.h"
#include "Scripting/Lua/LuaScriptComponent.h"
#include "Scripting/Lua/LuaManager.h"
#include "Graphics/MeshFactory.h"
#include "Graphics/Light.h"
#include "Graphics/Model.h"
#include "Graphics/Environment.h"
#include "Scene/EntityManager.h"

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <entt/entity/registry.hpp>
//#include <cereal/cereal.hpp>
#include <sol/sol.hpp>

namespace Lumos
{
	Scene::Scene(const std::string& friendly_name)
		: m_SceneName(friendly_name)
		, m_ScreenWidth(0)
		, m_ScreenHeight(0)
	{
		m_LayerStack = new LayerStack();
		m_EntityManager = CreateUniqueRef<EntityManager>(this);
	}

	Scene::~Scene()
	{
		delete m_LayerStack;

		m_EntityManager->Clear();
	}

	entt::registry& Scene::GetRegistry()
	{
		return m_EntityManager->GetRegistry();
	}

	void Scene::OnInit()
	{
		LuaManager::Get().GetState().set("registry", &m_EntityManager->GetRegistry());
		LuaManager::Get().GetState().set("scene", this);

		m_CurrentScene = true;

		//Default physics setup
		Application::Get().GetSystem<LumosPhysicsEngine>()->SetDampingFactor(0.998f);
		Application::Get().GetSystem<LumosPhysicsEngine>()->SetIntegrationType(IntegrationType::RUNGE_KUTTA_4);
		Application::Get().GetSystem<LumosPhysicsEngine>()->SetBroadphase(Lumos::CreateRef<Octree>(5, 3, Lumos::CreateRef<SortAndSweepBroadphase>()));

		m_SceneGraph.Init(m_EntityManager->GetRegistry());

		LuaManager::Get().OnInit(this);
	}

	void Scene::OnCleanupScene()
	{
		m_LayerStack->Clear();

		DeleteAllGameObjects();

		LuaManager::Get().GetState().collect_garbage();

		Application::Get().GetRenderManager()->Reset();

		auto audioManager = Application::Get().GetSystem<AudioManager>();
		if(audioManager)
		{
			audioManager->ClearNodes();
		}

		m_CurrentScene = false;
	};

	void Scene::DeleteAllGameObjects()
	{
		m_EntityManager->Clear();
	}

	void Scene::OnUpdate(const TimeStep& timeStep)
	{
		const Maths::Vector2 mousePos = Input::GetInput()->GetMousePosition();

		auto defaultCameraControllerView = m_EntityManager->GetEntitiesWithType<DefaultCameraController>();

		if(!defaultCameraControllerView.Empty())
		{
            auto& cameraController = defaultCameraControllerView.Front().GetComponent<DefaultCameraController>();
            auto trans = defaultCameraControllerView.Front().TryGetComponent<Maths::Transform>();
			if(Application::Get().GetSceneActive() && trans)
			{
				cameraController.GetController()->HandleMouse(*trans, timeStep.GetMillis(), mousePos.x, mousePos.y);
				cameraController.GetController()->HandleKeyboard(*trans, timeStep.GetMillis());
			}
		}

		m_SceneGraph.Update(m_EntityManager->GetRegistry());
	}

	void Scene::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Scene::OnWindowResize));
	}

	bool Scene::OnWindowResize(WindowResizeEvent& e)
	{
		if(!Application::Get().GetSceneActive())
			return false;

		auto cameraView = m_EntityManager->GetRegistry().view<Camera>();
		if(!cameraView.empty())
		{
			m_EntityManager->GetRegistry().get<Camera>(cameraView.front()).SetAspectRatio(static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight()));
		}

		return false;
	}

	void Scene::PushLayer(Layer* layer, bool overlay)
	{
		if(overlay)
			m_LayerStack->PushOverlay(layer);
		else
			m_LayerStack->PushLayer(layer);

		layer->OnAttach();
	}

#define ALL_COMPONENTS Maths::Transform, NameComponent, ActiveComponent, Hierarchy, Camera, LuaScriptComponent, Graphics::Model, Graphics::Light, Physics3DComponent, Graphics::Environment, Graphics::Sprite, Physics2DComponent, DefaultCameraController
	void Scene::Serialise(const std::string& filePath, bool binary)
	{
		std::string path = filePath;
		path += RemoveSpaces(m_SceneName);
		if(binary)
		{
			path += std::string(".bin");

			std::ofstream file(path, std::ios::binary);

			{
				// output finishes flushing its contents when it goes out of scope
				cereal::BinaryOutputArchive output{file};
				entt::snapshot{m_EntityManager->GetRegistry()}.entities(output).component<ALL_COMPONENTS>(output);
				output(*this);
			}
			file.close();
		}
		else
		{
			std::stringstream storage;
			path += std::string(".lsn");

			{
				// output finishes flushing its contents when it goes out of scope
				cereal::JSONOutputArchive output{storage};
				entt::snapshot{m_EntityManager->GetRegistry()}.entities(output).component<ALL_COMPONENTS>(output);
				output(*this);
			}
			FileSystem::WriteTextFile(path, storage.str());
		}
	}

	void Scene::Deserialise(const std::string& filePath, bool binary)
	{
		m_EntityManager->Clear();

		std::string path = filePath;
		path += RemoveSpaces(m_SceneName);

		if(binary)
		{
			path += std::string(".bin");

			if(!FileSystem::FileExists(path))
			{
				Lumos::Debug::Log::Error("No saved scene file found");
				return;
			}

			std::ifstream file(path, std::ios::binary);
			cereal::BinaryInputArchive input(file);
			entt::snapshot_loader{m_EntityManager->GetRegistry()}.entities(input).component<ALL_COMPONENTS>(input); //continuous_loader
			input(*this);
		}
		else
		{
			path += std::string(".lsn");

			if(!FileSystem::FileExists(path))
			{
				Lumos::Debug::Log::Error("No saved scene file found");
				return;
			}

			std::string data = FileSystem::ReadTextFile(path);
			std::istringstream istr;
			istr.str(data);
			cereal::JSONInputArchive input(istr);
			entt::snapshot_loader{m_EntityManager->GetRegistry()}.entities(input).component<ALL_COMPONENTS>(input); //continuous_loader
			input(*this);
		}
	}

	void Scene::UpdateSceneGraph()
	{
		m_SceneGraph.Update(m_EntityManager->GetRegistry());
	}

	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::entity src, entt::registry& registry)
	{
		if(registry.has<T>(src))
		{
			auto& srcComponent = registry.get<T>(src);
			registry.emplace_or_replace<T>(dst, srcComponent);
		}
	}
    
    Entity Scene::CreateEntity()
    {
        return m_EntityManager->Create();
    }

	   Entity Scene::CreateEntity(const std::string& name)
    {
        return m_EntityManager->Create(name);
    }
    
    void Scene::DuplicateEntity(Entity entity)
    {
        Entity newEntity = m_EntityManager->Create();

		CopyComponentIfExists<Maths::Transform>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Model>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<LuaScriptComponent>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Camera>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Sprite>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<RigidBody2D>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<RigidBody3D>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Light>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<SoundComponent>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Environment>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
	}

	void Scene::DuplicateEntity(Entity entity, Entity parent)
	{
		Entity newEntity = m_EntityManager->Create();

		CopyComponentIfExists<Maths::Transform>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Model>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<LuaScriptComponent>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Camera>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Sprite>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<RigidBody2D>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<RigidBody3D>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Light>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<SoundComponent>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());
		CopyComponentIfExists<Graphics::Environment>(newEntity.GetHandle(), entity.GetHandle(), m_EntityManager->GetRegistry());

		if(parent)
            newEntity.SetParent(parent);
	}
}
