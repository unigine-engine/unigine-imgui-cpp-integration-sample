#include "ImGuiImpl.h"

#include <UnigineControls.h>
#include <UnigineEngine.h>
#include <UnigineGui.h>
#include <UnigineMaterials.h>
#include <UnigineMeshDynamic.h>
#include <UnigineRender.h>
#include <UnigineTextures.h>
#include <UnigineViewport.h>
#include <UnigineWindowManager.h>

using namespace Unigine;

static TexturePtr font_texture;
static MeshDynamicPtr imgui_mesh;
static MaterialPtr imgui_material;
static ImDrawData *frame_draw_data;

static EventConnections connections;

ImGuiStyle ImGuiImpl::source_style;
float ImGuiImpl::last_scale = 1.0f;

Unigine::Input::MOUSE_HANDLE ImGuiImpl::prev_mouse_handle;

static ImGuiKey keymap[Input::NUM_KEYS];

static ImVec2 operator*(const ImVec2 &a, float b)
{
	return ImVec2(a.x * b, a.y * b);
}

static int on_key_event(Input::KEY key, bool down)
{
	auto &io = ImGui::GetIO();
	io.AddKeyEvent(keymap[key], down);

	switch (keymap[key])
	{
	case ImGuiKey_LeftCtrl:
	case ImGuiKey_RightCtrl: io.AddKeyEvent(ImGuiMod_Ctrl, down); break;
	case ImGuiKey_LeftShift:
	case ImGuiKey_RightShift: io.AddKeyEvent(ImGuiMod_Shift, down); break;
	case ImGuiKey_LeftAlt:
	case ImGuiKey_RightAlt: io.AddKeyEvent(ImGuiMod_Alt, down); break;
	case ImGuiKey_LeftSuper:
	case ImGuiKey_RightSuper: io.AddKeyEvent(ImGuiMod_Super, down); break;
	default: break;
	}

	return 0;
}

static int on_mouse_button_event(Input::MOUSE_BUTTON button, bool down)
{
	auto &io = ImGui::GetIO();

	switch (button)
	{
	case Input::MOUSE_BUTTON_LEFT: io.AddMouseButtonEvent(ImGuiMouseButton_Left, down); break;
	case Input::MOUSE_BUTTON_RIGHT: io.AddMouseButtonEvent(ImGuiMouseButton_Right, down); break;
	case Input::MOUSE_BUTTON_MIDDLE: io.AddMouseButtonEvent(ImGuiMouseButton_Middle, down); break;
	default: break;
	}

	return 0;
}

static int on_key_down(Input::KEY key)
{
	return on_key_event(key, true);
}

static int on_key_up(Input::KEY key)
{
	return on_key_event(key, false);
}

static int on_mouse_button_down(Input::MOUSE_BUTTON button)
{
	return on_mouse_button_event(button, true);
}

static int on_mouse_button_up(Input::MOUSE_BUTTON button)
{
	return on_mouse_button_event(button, false);
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
	blob->setData(nullptr, 0);

	io.Fonts->SetTexID(font_texture.get());
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

	assert(imgui_mesh->getVertexSize() == sizeof(ImDrawVert)
		&& "Vertex size of MeshDynamic is not equal to size of ImDrawVert");
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
	{
		return;
	}

	auto draw_data = frame_draw_data;
	frame_draw_data = nullptr;

	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	auto render_target = Render::getTemporaryRenderTarget();
	render_target->bindColorTexture(0, Renderer::getTextureColor());

	// Render state
	RenderState::saveState();
	RenderState::clearStates();
	RenderState::setBlendFunc(RenderState::BLEND_SRC_ALPHA, RenderState::BLEND_ONE_MINUS_SRC_ALPHA,
		RenderState::BLEND_OP_ADD);
	RenderState::setPolygonCull(RenderState::CULL_NONE);
	RenderState::setDepthFunc(RenderState::DEPTH_NONE);
	RenderState::setViewport(static_cast<int>(draw_data->DisplayPos.x),
		static_cast<int>(draw_data->DisplayPos.y), static_cast<int>(draw_data->DisplaySize.x),
		static_cast<int>(draw_data->DisplaySize.y));

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

	ImGuiIO &io = ImGui::GetIO();

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
					{
						cmd->UserCallback(cmd_list, cmd);
					}
				}
				else
				{
					float width = (cmd->ClipRect.z - cmd->ClipRect.x) / draw_data->DisplaySize.x;
					float height = (cmd->ClipRect.w - cmd->ClipRect.y) / draw_data->DisplaySize.y;
					float x = (cmd->ClipRect.x - clip_off.x) / draw_data->DisplaySize.x;
					float y = 1.0f - height
						- (cmd->ClipRect.y - clip_off.y) / draw_data->DisplaySize.y;

					if (cmd->GetTexID() == 0 || cmd->GetTexID() == io.Fonts->TexRef.GetTexID())
					{
						RenderState::setTexture(RenderState::BIND_FRAGMENT, 0, font_texture);
					}
					else
					{
						auto texture = TexturePtr(reinterpret_cast<Texture *>(cmd->GetTexID()));
						RenderState::setTexture(RenderState::BIND_FRAGMENT, 0, texture);
					}

					RenderState::setScissorTest(x, y, width, height);
					RenderState::flushStates();

					imgui_mesh->renderInstancedSurface(MeshDynamic::MODE_TRIANGLES,
						cmd->VtxOffset + global_vtx_offset, cmd->IdxOffset + global_idx_offset,
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

	Input::getEventKeyDown().connect(connections, on_key_down);
	Input::getEventKeyUp().connect(connections, on_key_up);

	Input::getEventMouseDown().connect(connections, on_mouse_button_down);
	Input::getEventMouseUp().connect(connections, on_mouse_button_up);

	Input::getEventTextPress().connect(connections, on_unicode_key_pressed);

	Engine::get()->getEventBeginRender().connect(connections, before_render_callback);

	main_window->getViewport()->getEventEndScreen().connect(connections, draw_callback);

	ImGuiIO &io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	io.BackendPlatformName = "imgui_impl_unigine";
	io.BackendRendererName = "imgui_impl_unigine";

	// https://github.com/ocornut/imgui/issues/4921

	keymap[Input::KEY_ESC] = ImGuiKey_Escape;
	keymap[Input::KEY_F1] = ImGuiKey_F1;
	keymap[Input::KEY_F2] = ImGuiKey_F2;
	keymap[Input::KEY_F3] = ImGuiKey_F3;
	keymap[Input::KEY_F4] = ImGuiKey_F4;
	keymap[Input::KEY_F5] = ImGuiKey_F5;
	keymap[Input::KEY_F6] = ImGuiKey_F6;
	keymap[Input::KEY_F7] = ImGuiKey_F7;
	keymap[Input::KEY_F8] = ImGuiKey_F8;
	keymap[Input::KEY_F9] = ImGuiKey_F9;
	keymap[Input::KEY_F10] = ImGuiKey_F10;
	keymap[Input::KEY_F11] = ImGuiKey_F11;
	keymap[Input::KEY_F12] = ImGuiKey_F12;
	keymap[Input::KEY_PRINTSCREEN] = ImGuiKey_None;
	keymap[Input::KEY_SCROLL_LOCK] = ImGuiKey_None;
	keymap[Input::KEY_PAUSE] = ImGuiKey_None;
	keymap[Input::KEY_BACK_QUOTE] = ImGuiKey_None;
	keymap[Input::KEY_DIGIT_1] = ImGuiKey_1;
	keymap[Input::KEY_DIGIT_2] = ImGuiKey_2;
	keymap[Input::KEY_DIGIT_3] = ImGuiKey_3;
	keymap[Input::KEY_DIGIT_4] = ImGuiKey_4;
	keymap[Input::KEY_DIGIT_5] = ImGuiKey_5;
	keymap[Input::KEY_DIGIT_6] = ImGuiKey_6;
	keymap[Input::KEY_DIGIT_7] = ImGuiKey_7;
	keymap[Input::KEY_DIGIT_8] = ImGuiKey_8;
	keymap[Input::KEY_DIGIT_9] = ImGuiKey_9;
	keymap[Input::KEY_DIGIT_0] = ImGuiKey_0;
	keymap[Input::KEY_MINUS] = ImGuiKey_Minus;
	keymap[Input::KEY_EQUALS] = ImGuiKey_Equal;
	keymap[Input::KEY_BACKSPACE] = ImGuiKey_Backspace;
	keymap[Input::KEY_TAB] = ImGuiKey_Tab;
	keymap[Input::KEY_Q] = ImGuiKey_Q;
	keymap[Input::KEY_W] = ImGuiKey_W;
	keymap[Input::KEY_E] = ImGuiKey_E;
	keymap[Input::KEY_R] = ImGuiKey_R;
	keymap[Input::KEY_T] = ImGuiKey_T;
	keymap[Input::KEY_Y] = ImGuiKey_Y;
	keymap[Input::KEY_U] = ImGuiKey_U;
	keymap[Input::KEY_I] = ImGuiKey_I;
	keymap[Input::KEY_O] = ImGuiKey_O;
	keymap[Input::KEY_P] = ImGuiKey_P;
	keymap[Input::KEY_LEFT_BRACKET] = ImGuiKey_LeftBracket;
	keymap[Input::KEY_RIGHT_BRACKET] = ImGuiKey_RightBracket;
	keymap[Input::KEY_ENTER] = ImGuiKey_Enter;
	keymap[Input::KEY_CAPS_LOCK] = ImGuiKey_CapsLock;
	keymap[Input::KEY_A] = ImGuiKey_A;
	keymap[Input::KEY_S] = ImGuiKey_S;
	keymap[Input::KEY_D] = ImGuiKey_D;
	keymap[Input::KEY_F] = ImGuiKey_F;
	keymap[Input::KEY_G] = ImGuiKey_G;
	keymap[Input::KEY_H] = ImGuiKey_H;
	keymap[Input::KEY_J] = ImGuiKey_J;
	keymap[Input::KEY_K] = ImGuiKey_K;
	keymap[Input::KEY_L] = ImGuiKey_L;
	keymap[Input::KEY_SEMICOLON] = ImGuiKey_Semicolon;
	keymap[Input::KEY_QUOTE] = ImGuiKey_None;
	keymap[Input::KEY_BACK_SLASH] = ImGuiKey_Backslash;
	keymap[Input::KEY_LEFT_SHIFT] = ImGuiKey_LeftShift;
	keymap[Input::KEY_LESS] = ImGuiKey_None;
	keymap[Input::KEY_Z] = ImGuiKey_Z;
	keymap[Input::KEY_X] = ImGuiKey_X;
	keymap[Input::KEY_C] = ImGuiKey_C;
	keymap[Input::KEY_V] = ImGuiKey_V;
	keymap[Input::KEY_B] = ImGuiKey_B;
	keymap[Input::KEY_N] = ImGuiKey_N;
	keymap[Input::KEY_M] = ImGuiKey_M;
	keymap[Input::KEY_COMMA] = ImGuiKey_Comma;
	keymap[Input::KEY_DOT] = ImGuiKey_Comma;
	keymap[Input::KEY_SLASH] = ImGuiKey_None;
	keymap[Input::KEY_RIGHT_SHIFT] = ImGuiKey_RightShift;
	keymap[Input::KEY_LEFT_CTRL] = ImGuiKey_LeftCtrl;
	keymap[Input::KEY_LEFT_CMD] = ImGuiKey_LeftSuper;
	keymap[Input::KEY_LEFT_ALT] = ImGuiKey_LeftAlt;
	keymap[Input::KEY_SPACE] = ImGuiKey_Space;
	keymap[Input::KEY_RIGHT_ALT] = ImGuiKey_RightAlt;
	keymap[Input::KEY_RIGHT_CMD] = ImGuiKey_RightSuper;
	keymap[Input::KEY_MENU] = ImGuiKey_None;
	keymap[Input::KEY_RIGHT_CTRL] = ImGuiKey_RightCtrl;
	keymap[Input::KEY_INSERT] = ImGuiKey_Insert;
	keymap[Input::KEY_DELETE] = ImGuiKey_Delete;
	keymap[Input::KEY_HOME] = ImGuiKey_Home;
	keymap[Input::KEY_END] = ImGuiKey_End;
	keymap[Input::KEY_PGUP] = ImGuiKey_PageUp;
	keymap[Input::KEY_PGDOWN] = ImGuiKey_PageDown;
	keymap[Input::KEY_UP] = ImGuiKey_UpArrow;
	keymap[Input::KEY_LEFT] = ImGuiKey_LeftArrow;
	keymap[Input::KEY_DOWN] = ImGuiKey_DownArrow;
	keymap[Input::KEY_RIGHT] = ImGuiKey_RightArrow;
	keymap[Input::KEY_NUM_LOCK] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIVIDE] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_MULTIPLY] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_MINUS] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_7] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_8] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_9] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_PLUS] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_4] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_5] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_6] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_1] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_2] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_3] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_ENTER] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DIGIT_0] = ImGuiKey_None;
	keymap[Input::KEY_NUMPAD_DOT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_SHIFT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_CTRL] = ImGuiKey_None;
	keymap[Input::KEY_ANY_ALT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_CMD] = ImGuiKey_None;
	keymap[Input::KEY_ANY_UP] = ImGuiKey_None;
	keymap[Input::KEY_ANY_LEFT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_DOWN] = ImGuiKey_None;
	keymap[Input::KEY_ANY_RIGHT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_ENTER] = ImGuiKey_None;
	keymap[Input::KEY_ANY_DELETE] = ImGuiKey_None;
	keymap[Input::KEY_ANY_INSERT] = ImGuiKey_None;
	keymap[Input::KEY_ANY_HOME] = ImGuiKey_None;
	keymap[Input::KEY_ANY_END] = ImGuiKey_None;
	keymap[Input::KEY_ANY_PGUP] = ImGuiKey_None;
	keymap[Input::KEY_ANY_PGDOWN] = ImGuiKey_None;

	io.SetClipboardTextFn = set_clipboard_text;
	io.GetClipboardTextFn = get_clipboard_text;
	io.ClipboardUserData = nullptr;

	create_font_texture();
	create_imgui_mesh();
	create_imgui_material();

	{
		ImGui::StyleColorsDark();
		auto &style = ImGui::GetStyle();

		{
			style.FrameBorderSize = 1.f;
			style.TabBorderSize = 1.f;
		}

		{
			style.WindowRounding = 5.f;
			style.ChildRounding = 4.f;
			style.FrameRounding = 4.f;
			style.PopupRounding = 4.f;
			style.ScrollbarRounding = 12.f;
			style.GrabRounding = 4.f;
			style.LogSliderDeadzone = 4.f;
			style.TabRounding = 4.f;
		}
	}

	source_style = ImGui::GetStyle();
}

void ImGuiImpl::newFrame()
{
	EngineWindowPtr main_window = WindowManager::getMainWindow();
	if (!main_window)
	{
		Engine::get()->quit();
		return;
	}

	auto &io = ImGui::GetIO();

	io.DisplaySize = ImVec2(Math::toFloat(main_window->getClientRenderSize().x),
		Math::toFloat(main_window->getClientRenderSize().y));
	io.DeltaTime = Engine::get()->getIFps();

	if (Input::isMouseGrab() == false)
	{
		Unigine::ControlsApp::setEnabled(!io.WantCaptureKeyboard);
		if (io.WantCaptureKeyboard)
		{
			Unigine::ControlsApp::setMouseDX(0);
			Unigine::ControlsApp::setMouseDY(0);
		}

		if (io.WantSetMousePos)
		{
			Input::setMousePosition(
				Math::ivec2(Math::ftoi(io.MousePos.x), Math::ftoi(io.MousePos.y)));
		}

		const Math::ivec2 mouse_coord = Input::getMousePosition()
			- main_window->getClientPosition();

		io.MousePos = ImVec2(static_cast<float>(mouse_coord.x), static_cast<float>(mouse_coord.y));

		io.MouseWheel += static_cast<float>(Input::getMouseWheel());
		io.MouseWheelH += static_cast<float>(Input::getMouseWheelHorizontal());

		Input::setMouseHandle(io.WantCaptureMouse ? Input::MOUSE_HANDLE_USER : prev_mouse_handle);
	}

	float scale = main_window->getDpiScale();
	if (Math::compare(last_scale, scale) == 0)
	{
		auto &style = ImGui::GetStyle();
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
		style.TabCloseButtonMinWidthSelected = (source_style.TabCloseButtonMinWidthSelected
												   != FLT_MAX)
			? (source_style.TabCloseButtonMinWidthSelected * scale)
			: FLT_MAX;
		style.TabCloseButtonMinWidthUnselected = (source_style.TabCloseButtonMinWidthUnselected
													 != FLT_MAX)
			? (source_style.TabCloseButtonMinWidthUnselected * scale)
			: FLT_MAX;
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
