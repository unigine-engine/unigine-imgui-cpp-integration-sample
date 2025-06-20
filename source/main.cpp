#include <UnigineInit.h>
#include <UnigineEngine.h>
#include "AppSystemLogic.h"
#include "AppWorldLogic.h"

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	// UnigineLogic
	AppSystemLogic system_logic;
	AppWorldLogic world_logic;

	// init engine
	Unigine::Engine::InitParameters init_params;
	init_params.window_title = "UNIGINE Engine: ImGui C++ integration";
	Unigine::Engine::init(init_params, argc, argv);

	// enter main loop
	Unigine::Engine::get()->main(&system_logic, &world_logic, nullptr);

	return 0;
}
