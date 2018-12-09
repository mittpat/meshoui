#pragma once

namespace Meshoui
{
    class IMouse
    {
    public:
        virtual ~IMouse();
        IMouse();

        virtual void cursorPositionAction(void* window, double xpos, double ypos, bool overlay);
        virtual void mouseButtonAction(void* window, int button, int action, int mods, bool overlay);
        virtual void scrollAction(void* window, double xoffset, double yoffset, bool overlay);
        virtual void mouseLost();
    };
    inline IMouse::IMouse() {}
    inline IMouse::~IMouse() {}
}