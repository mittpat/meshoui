#pragma once

namespace Meshoui
{
    class IMouse
    {
    public:
        virtual ~IMouse();
        IMouse();

        virtual void mouseButtonAction(void* window, int button, int action, int mods);
        virtual void scrollAction(void* window, double xoffset, double yoffset);
    };
    inline IMouse::IMouse() {}
    inline IMouse::~IMouse() {}
}
