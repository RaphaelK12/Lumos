#include <LumosEngine.h>
#include <Core/EntryPoint.h>
#include "Scenes/Scene3D.h"
#include "Scenes/GraphicsScene.h"
#include "Scenes/SceneModelViewer.h"
#include "Scenes/Scene2D.h"
#include "Scenes/MaterialTest.h"

using namespace Lumos;

class Game : public Application
{
    public:
	explicit Game(const WindowProperties& windowProperties)
		: Application(windowProperties)
	{
        Application::Get().GetWindow()->SetWindowTitle("Sandbox");
	}
    
	~Game()
	{
	}
    
	void Init() override
	{
		Application::Init();
        
		const std::string root = ROOT_DIR;
		VFS::Get()->Mount("Meshes", root + "/Sandbox/res/meshes");
		VFS::Get()->Mount("Textures", root + "/Sandbox/res/textures");
		VFS::Get()->Mount("Sounds", root + "/Sandbox/res/sounds");
		VFS::Get()->Mount("Scripts", root + "/Sandbox/res/scripts");
        
		GetSceneManager()->EnqueueScene<SceneModelViewer>("SceneModelViewer");
		GetSceneManager()->EnqueueScene<Scene2D>("2D Test");
		GetSceneManager()->EnqueueScene<Scene3D>("Physics Scene");
		GetSceneManager()->EnqueueScene<GraphicsScene>("Terrain Test");
		GetSceneManager()->EnqueueScene<MaterialTest>("Material Test");
		GetSceneManager()->SwitchScene(2);
		GetSceneManager()->ApplySceneSwitch();
	}
};

Lumos::Application* Lumos::CreateApplication()
{
#ifdef LUMOS_PLATFORM_IOS
	WindowProperties windowProperties;
	windowProperties.RenderAPI = static_cast<int>(Graphics::RenderAPI::VULKAN);
#else
	WindowProperties windowProperties = LuaManager::Get().LoadConfigFile(ROOT_DIR "/Sandbox/Settings.lua");
	windowProperties.ShowConsole = true;
#endif
    
	return new Game(windowProperties);
}
