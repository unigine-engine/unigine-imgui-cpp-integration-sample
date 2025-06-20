#include "ImGuiImpl.h"

#include <UnigineTextures.h>
#include <UnigineMeshDynamic.h>
#include <UnigineMaterials.h>
#include <UnigineRender.h>
#include <UnigineGui.h>
#include <UnigineControls.h>
#include <UnigineEngine.h>
#include <UnigineWindowManager.h>
#include <UnigineViewport.h>

using namespace Unigine;

static TexturePtr font_texture;
static MeshDynamicPtr imgui_mesh;
static MaterialPtr imgui_material;
static ImDrawData *frame_draw_data;

static int saved_mouse = 0;
static EventConnections connections;

ImGuiStyle ImGuiImpl::source_style;
float ImGuiImpl::last_scale = 1.0f;

Unigine::Input::MOUSE_HANDLE ImGuiImpl::prev_mouse_handle;

ImVec2 operator*(const ImVec2 &vec, const float value) { return ImVec2(vec.x * value, vec.y * value); }

static int on_key_pressed(Input::KEY key)
{
	auto &io = ImGui::GetIO();
	io.KeysDown[key] = true;
	return 0;
}

static int on_key_released(Input::KEY key)
{
	auto &io = ImGui::GetIO();
	io.KeysDown[key] = false;
	return 0;
}

static int on_button_pressed(Input::MOUSE_BUTTON button)
{
	auto &io = ImGui::GetIO();

	switch (button)
	{
	case Input::MOUSE_BUTTON_LEFT:
		io.MouseDown[0] = true;
		break;
	case Input::MOUSE_BUTTON_RIGHT:
		io.MouseDown[1] = true;
		break;
	case Input::MOUSE_BUTTON_MIDDLE:
		io.MouseDown[2] = true;
		break;
	}

	return 0;
}

static int on_button_released(Input::MOUSE_BUTTON button)
{
	auto &io = ImGui::GetIO();

	switch (button)
	{
	case Input::MOUSE_BUTTON_LEFT:
		io.MouseDown[0] = false;
		break;
	case Input::MOUSE_BUTTON_RIGHT:
		io.MouseDown[1] = false;
		break;
	case Input::MOUSE_BUTTON_MIDDLE:
		io.MouseDown[2] = false;
		break;
	}

	return 0;
}

static int on_unicode_key_pressed(unsigned int key)
{
	auto &io = ImGui::GetIO();

	io.AddInputCharacter(key);

	return 0;
}

static void set_clipboard_text(void *, const char *text)
{
	Input::setClipboard(text);
}

static char const *get_clipboard_text(void *)
{
	return Input::getClipboard();
}

static void create_font_texture()
{
	auto &io = ImGui::GetIO();
	unsigned char *pixels = nullptr;
	int width = 0;
	int height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	font_texture = Texture::create();
	font_texture->create2D(width, height, Texture::FORMAT_RGBA8, Texture::SAMPLER_FILTER_LINEAR);

	auto blob = Blob::create();
	blob->setData(pixels, width * height * 32);
	font_texture->setBlob(blob);
	blob->setData(nullptr,0);

	io.Fonts->TexID = font_texture.get();
}

static void create_imgui_mesh()
{
	imgui_mesh = MeshDynamic::create(MeshDynamic::USAGE_DYNAMIC_ALL);

	MeshDynamic::Attribute attributes[3]{};
	attributes[0].offset = 0;
	attributes[0].size = 2;
	attributes[0].type = MeshDynamic::TYPE_FLOAT;
	attributes[1].offset = 8;
	attributes[1].size = 2;
	attributes[1].type = MeshDynamic::TYPE_FLOAT;
	attributes[2].offset = 16;
	attributes[2].size = 4;
	attributes[2].type = MeshDynamic::TYPE_UCHAR;
	imgui_mesh->setVertexFormat(attributes, 3);

	assert(imgui_mesh->getVertexSize() == sizeof(ImDrawVert) && "Vertex size of MeshDynamic is not equal to size of ImDrawVert");
}

static void create_imgui_material()
{
	imgui_material = Materials::findManualMaterial("imgui")->inherit();
}

static void before_render_callback()
{
	auto &io = ImGui::GetIO();
	if (io.WantCaptureMouse)
	{
		Gui::getCurrent()->setMouseButtons(0);
	}
}

static void draw_callback()
{
	if (frame_draw_data == nullptr)
		return;

	auto draw_data = frame_draw_data;
	frame_draw_data = nullptr;

	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;

	auto render_target = Render::getTemporaryRenderTarget();
	render_target->bindColorTexture(0, Renderer::getTextureColor());

	// Render state
	RenderState::saveState();
	RenderState::clearStates();
	RenderState::setBlendFunc(RenderState::BLEND_SRC_ALPHA, RenderState::BLEND_ONE_MINUS_SRC_ALPHA, RenderState::BLEND_OP_ADD);
	RenderState::setPolygonCull(RenderState::CULL_NONE);
	RenderState::setDepthFunc(RenderState::DEPTH_NONE);
	RenderState::setViewport(static_cast<int>(draw_data->DisplayPos.x), static_cast<int>(draw_data->DisplayPos.y),
		static_cast<int>(draw_data->DisplaySize.x),static_cast<int>(draw_data->DisplaySize.y));

	// Orthographic projection matrix
	float left = draw_data->DisplayPos.x;
	float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float top = draw_data->DisplayPos.y;
	float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

	Math::mat4 proj;
	proj.m00 = 2.0f / (right - left);
	proj.m03 = (right + left) / (left - right);
	proj.m11 = 2.0f / (top - bottom);
	proj.m13 = (top + bottom) / (bottom - top);
	proj.m22 = 0.5f;
	proj.m23 = 0.5f;
	proj.m33 = 1.0f;

	Renderer::setProjection(proj);
	auto shader = imgui_material->getShaderForce("imgui");
	auto pass = imgui_material->getRenderPass("imgui");
	Renderer::setShaderParameters(pass, shader, imgui_material, false);

	ImGuiIO& io = ImGui::GetIO();

	imgui_mesh->bind();

	// Write vertex and index data into dynamic mesh
	imgui_mesh->clearVertex();
	imgui_mesh->clearIndices();
	imgui_mesh->allocateVertex(draw_data->TotalVtxCount);
	imgui_mesh->allocateIndices(draw_data->TotalIdxCount);
	for (int i = 0; i < draw_data->CmdListsCount; ++i)
	{
		const ImDrawList *cmd_list = draw_data->CmdLists[i];

		imgui_mesh->addVertexArray(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size);
		imgui_mesh->addIndicesArray(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size);
	}
	imgui_mesh->flushVertex();
	imgui_mesh->flushIndices();

	render_target->enable();
	{
		int global_idx_offset = 0;
		int global_vtx_offset = 0;
		ImVec2 clip_off = draw_data->DisplayPos;
		// Draw command lists
		for (int i = 0; i < draw_data->CmdListsCount; ++i)
		{
			const ImDrawList *cmd_list = draw_data->CmdLists[i];
			for (int j = 0; j < cmd_list->CmdBuffer.Size; ++j)
			{
				const ImDrawCmd *cmd = &cmd_list->CmdBuffer[j];

				if (cmd->UserCallback != nullptr)
				{
					if (cmd->UserCallback != ImDrawCallback_ResetRenderState)
						cmd->UserCallback(cmd_list, cmd);
				}
				else
				{
					float width = (cmd->ClipRect.z - cmd->ClipRect.x) / draw_data->DisplaySize.x;
					float height = (cmd->ClipRect.w - cmd->ClipRect.y) / draw_data->DisplaySize.y;
					float x = (cmd->ClipRect.x - clip_off.x) / draw_data->DisplaySize.x;
					float y = 1.0f - height - (cmd->ClipRect.y - clip_off.y) / draw_data->DisplaySize.y;

					if (cmd->TextureId == 0 || cmd->TextureId == io.Fonts->TexID)
					{
						RenderState::setTexture(RenderState::BIND_FRAGMENT, 0, font_texture);
					}
					else
					{
						auto texture = TexturePtr(static_cast<Texture*>(cmd->TextureId));
						RenderState::setTexture(RenderState::BIND_FRAGMENT, 0, texture);
					}

					RenderState::setScissorTest(x, y, width, height);
					RenderState::flushStates();

					imgui_mesh->renderInstancedSurface(MeshDynamic::MODE_TRIANGLES,
						cmd->VtxOffset + global_vtx_offset,
						cmd->IdxOffset + global_idx_offset,
						cmd->IdxOffset + global_idx_offset + cmd->ElemCount, 1);
				}
			}
			global_vtx_offset += cmd_list->VtxBuffer.Size;
			global_idx_offset += cmd_list->IdxBuffer.Size;
		}

		RenderState::setScissorTest(0.0f, 0.0f, 1.0f, 1.0f);
	}
	render_target->disable();
	imgui_mesh->unbind();

	RenderState::restoreState();

	render_target->unbindColorTexture(0);
	Render::releaseTemporaryRenderTarget(render_target);
}

void ImGuiImpl::init()
{
	auto main_window = WindowManager::getMainWindow();
	if (!main_window)
	{
		Engine::get()->quit();
		return;
	}

	Input::setMouseHandle(Input::MOUSE_HANDLE_GRAB);
	prev_mouse_handle = Input::getMouseHandle();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	Input::getEventKeyDown().connect(connections, on_key_pressed);
	Input::getEventKeyUp().connect(connections, on_key_released);

	Input::getEventMouseDown().connect(connections, on_button_pressed);
	Input::getEventMouseUp().connect(connections, on_button_released);

	Input::getEventTextPress().connect(connections, on_unicode_key_pressed);

	Engine::get()->getEventBeginRender().connect(connections, before_render_callback);

	main_window->getViewport()->getEventEndScreen().connect(connections, draw_callback);

	ImGuiIO &io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	io.BackendPlatformName = "imgui_impl_unigine";
	io.BackendRendererName = "imgui_impl_unigine";

	io.KeyMap[ImGuiKey_Tab] = Input::KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = Input::KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = Input::KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = Input::KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = Input::KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = Input::KEY_PGUP;
	io.KeyMap[ImGuiKey_PageDown] = Input::KEY_PGDOWN;
	io.KeyMap[ImGuiKey_Home] = Input::KEY_HOME;
	io.KeyMap[ImGuiKey_End] = Input::KEY_END;
	io.KeyMap[ImGuiKey_Insert] = Input::KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = Input::KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = Input::KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = Input::KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = Input::KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = Input::KEY_ESC;
	io.KeyMap[ImGuiKey_KeyPadEnter] = Input::KEY_ENTER;
	io.KeyMap[ImGuiKey_A] = Input::KEY_A;
	io.KeyMap[ImGuiKey_C] = Input::KEY_C;
	io.KeyMap[ImGuiKey_V] = Input::KEY_V;
	io.KeyMap[ImGuiKey_X] = Input::KEY_X;
	io.KeyMap[ImGuiKey_Y] = Input::KEY_Y;
	io.KeyMap[ImGuiKey_Z] = Input::KEY_Z;

	io.SetClipboardTextFn = set_clipboard_text;
	io.GetClipboardTextFn = get_clipboard_text;
	io.ClipboardUserData = nullptr;

	create_font_texture();
	create_imgui_mesh();
	create_imgui_material();

	ImGui::StyleColorsDark();
	source_style = ImGui::GetStyle();
}

static bool previous_want_capture_mouse;
static Input::MOUSE_HANDLE saved_mouse_handle;

void ImGuiImpl::newFrame()
{
	EngineWindowPtr main_window = WindowManager::getMainWindow();
	if (!main_window)
	{
		Engine::get()->quit();
		return;
	}

	auto &io = ImGui::GetIO();

	io.DisplaySize = ImVec2(Math::toFloat(main_window->getClientRenderSize().x), Math::toFloat(main_window->getClientRenderSize().y));
	io.DeltaTime = Engine::get()->getIFps();

	if (Input::isMouseGrab() == false)
	{
		Unigine::ControlsApp::setEnabled(!io.WantCaptureKeyboard);
		if (io.WantCaptureKeyboard)
		{
			Unigine::ControlsApp::setMouseDX(0);
			Unigine::ControlsApp::setMouseDY(0);
		}

		io.KeyCtrl = Input::isKeyPressed(Input::KEY_ANY_CTRL);
		io.KeyShift = Input::isKeyPressed(Input::KEY_ANY_SHIFT);
		io.KeyAlt = Input::isKeyPressed(Input::KEY_ANY_ALT);;
		io.KeySuper = Input::isKeyPressed(Input::KEY_ANY_CMD);

		if (io.WantSetMousePos)
			Input::setMousePosition(Math::ivec2(Math::ftoi(io.MousePos.x), Math::ftoi(io.MousePos.y)));

		const Math::ivec2 mouse_coord = Input::getMousePosition() - main_window->getClientPosition();

		io.MousePos = ImVec2(static_cast<float>(mouse_coord.x), static_cast<float>(mouse_coord.y));

		io.MouseWheel += static_cast<float>(Input::getMouseWheel());
		io.MouseWheelH += static_cast<float>(Input::getMouseWheelHorizontal());

		if (io.WantCaptureMouse)
			Input::setMouseHandle(Input::MOUSE_HANDLE_SOFT);
		else
			Input::setMouseHandle(prev_mouse_handle);
	}

	float scale = main_window->getDpiScale();
	if (Math::compare(last_scale, scale) == 0)
	{
		auto & style = ImGui::GetStyle();
		last_scale = scale;

		style.WindowPadding = source_style.WindowPadding * scale;
		style.WindowRounding = source_style.WindowRounding * scale;
		style.WindowMinSize = source_style.WindowMinSize * scale;
		style.ChildRounding = source_style.ChildRounding * scale;
		style.PopupRounding = source_style.PopupRounding * scale;
		style.FramePadding = source_style.FramePadding * scale;
		style.FrameRounding = source_style.FrameRounding * scale;
		style.ItemSpacing = source_style.ItemSpacing * scale;
		style.ItemInnerSpacing = source_style.ItemInnerSpacing * scale;
		style.CellPadding = source_style.CellPadding * scale;
		style.TouchExtraPadding = source_style.TouchExtraPadding * scale;
		style.IndentSpacing = source_style.IndentSpacing * scale;
		style.ColumnsMinSpacing = source_style.ColumnsMinSpacing * scale;
		style.ScrollbarSize = source_style.ScrollbarSize * scale;
		style.ScrollbarRounding = source_style.ScrollbarRounding * scale;
		style.GrabMinSize = source_style.GrabMinSize * scale;
		style.GrabRounding = source_style.GrabRounding * scale;
		style.LogSliderDeadzone = source_style.LogSliderDeadzone * scale;
		style.TabRounding = source_style.TabRounding * scale;
		style.TabMinWidthForCloseButton = (source_style.TabMinWidthForCloseButton != FLT_MAX) ? (source_style.TabMinWidthForCloseButton * scale) : FLT_MAX;
		style.DisplayWindowPadding = source_style.DisplayWindowPadding * scale;
		style.DisplaySafeAreaPadding = source_style.DisplaySafeAreaPadding * scale;
		style.MouseCursorScale = source_style.MouseCursorScale * scale;

		io.FontGlobalScale = scale;
	}

	ImGui::NewFrame();
}

void ImGuiImpl::render()
{
	ImGui::Render();
	frame_draw_data = ImGui::GetDrawData();
}

void ImGuiImpl::shutdown()
{
	imgui_material.deleteLater();
	imgui_mesh = nullptr;
	font_texture = nullptr;

	connections.disconnectAll();

	ImGui::DestroyContext();
}
