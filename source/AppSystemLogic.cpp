#include "AppSystemLogic.h"
#include <UnigineWorld.h>
#include <UnigineControls.h>
#include <UnigineInput.h>

#include "ImGuiImpl.h"

using namespace Unigine;

int AppSystemLogic::init()
{
	Unigine::Engine::get()->setBackgroundUpdate(Engine::BACKGROUND_UPDATE_RENDER_NON_MINIMIZED);

	Unigine::World::loadWorld("imgui");

	// initialize ImGui backend, set up the Engine
	ImGuiImpl::init();

	// create texture
	custom_texture = Texture::create();
	custom_texture->load("core/textures/common/checker_d.texture");

	return 1;
}

int AppSystemLogic::update()
{
	EngineWindowPtr main_window = WindowManager::getMainWindow();
	if (!main_window)
	{
		Engine::get()->quit();
		return 1;
	}

	ImGuiImpl::newFrame();

	// feel free to use ImGui API here...
	ImGui::ShowDemoWindow();

	// show custom texture with ImGui
	int image_width = custom_texture->getWidth();
	int image_height = custom_texture->getHeight();
	ImGui::Begin("Texture Test");
	ImGui::Image((ImTextureID)(intptr_t)custom_texture.get(), ImVec2(float(image_width), float(image_height)));
	ImGui::End();

	// impementation render
	ImGuiImpl::render();
	return 1;
}


int AppSystemLogic::shutdown()
{
	custom_texture = nullptr;

	// shutdown ImGui backend
	ImGuiImpl::shutdown();
	return 1;
}
