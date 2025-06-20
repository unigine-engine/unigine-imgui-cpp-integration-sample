#pragma once
#include <UnigineInput.h>
#include "imgui/imgui.h"

class ImGuiImpl
{
public:
	static void init();
	static void newFrame();
	static void render();
	static void shutdown();

private:

	static ImGuiStyle source_style;
	static float last_scale;

	static Unigine::Input::MOUSE_HANDLE prev_mouse_handle;
};