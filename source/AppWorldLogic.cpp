#include "AppWorldLogic.h"
#include "UnigineString.h"

// World logic, it takes effect only when the world is loaded.
// These methods are called right after corresponding world script's (UnigineScript) methods.

AppWorldLogic::AppWorldLogic()
{}

AppWorldLogic::~AppWorldLogic()
{}

int AppWorldLogic::init()
{
	// Write here code to be called on world initialization: initialize resources for your world scene during the world start.
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
// start of the main loop
////////////////////////////////////////////////////////////////////////////////

int AppWorldLogic::update()
{
	// Write here code to be called before updating each render frame: specify all graphics-related functions you want to be called every frame while your application executes.
	return 1;
}

int AppWorldLogic::postUpdate()
{
	// The engine calls this function after updating each render frame: correct behavior after the state of the node has been updated.
	return 1;
}

int AppWorldLogic::updatePhysics()
{
	// Write here code to be called before updating each physics frame: control physics in your application and put non-rendering calculations.
	// The engine calls updatePhysics() with the fixed rate (60 times per second by default) regardless of the FPS value.
	// WARNING: do not create, delete or change transformations of nodes here, because rendering is already in progress.
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
// end of the main loop
////////////////////////////////////////////////////////////////////////////////

int AppWorldLogic::shutdown()
{
	// Write here code to be called on world shutdown: delete resources that were created during world script execution to avoid memory leaks.
	return 1;
}
