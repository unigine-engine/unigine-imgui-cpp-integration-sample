#ifndef __APP_SYSTEM_LOGIC_H__
#define __APP_SYSTEM_LOGIC_H__

#include <UnigineLogic.h>

class AppSystemLogic : public Unigine::SystemLogic
{
public:
	AppSystemLogic() {}
	virtual ~AppSystemLogic() {}

	Unigine::TexturePtr	custom_texture;

	int init() override;
	int update() override;
	int shutdown() override;
};

#endif // __APP_SYSTEM_LOGIC_H__
