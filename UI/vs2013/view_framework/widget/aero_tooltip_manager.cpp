
#include "aero_tooltip_manager.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>


#include "base/rtl.h"

#include "message_framework/message_loop.h"

#include "gfx/point.h"

namespace view
{

    ///////////////////////////////////////////////////////////////////////////////
    // AeroTooltipManager, public:

    AeroTooltipManager::AeroTooltipManager(Widget* widget)
        : TooltipManagerWin(widget),
        initial_delay_(0) {}

    AeroTooltipManager::~AeroTooltipManager()
    {
        if(initial_timer_)
        {
            initial_timer_->Disown();
        }
    }

    void AeroTooltipManager::OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param)
    {
        if(initial_timer_)
        {
            initial_timer_->Disown();
        }

        if(u_msg==WM_MOUSEMOVE || u_msg==WM_NCMOUSEMOVE)
        {
            gfx::Point mouse_pos(l_param);
            if(u_msg == WM_NCMOUSEMOVE)
            {
                // NC message coordinates are in screen coordinates.
                POINT temp = mouse_pos.ToPOINT();
                ::MapWindowPoints(HWND_DESKTOP, GetParent(), &temp, 1);
                mouse_pos.SetPoint(temp.x, temp.y);
            }
            if(last_mouse_pos_ != mouse_pos)
            {
                last_mouse_pos_ = mouse_pos;
                HideKeyboardTooltip();
                UpdateTooltip(mouse_pos);
            }

            // Delay opening of the tooltip just in case the user moves their
            // mouse to another control. We defer this from Init because we get
            // zero if we query it too soon.
            if(!initial_delay_)
            {
                initial_delay_ = static_cast<int>(::SendMessage(tooltip_hwnd_,
                    TTM_GETDELAYTIME, TTDT_INITIAL, 0));
            }
            initial_timer_ = new InitialTimer(this);
            initial_timer_->Start(initial_delay_);
        }
        else
        {
            // Hide the tooltip and cancel any timers.
            ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
            ::SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE, false, (LPARAM)&toolinfo_);
            return;
        }
    }

    void AeroTooltipManager::OnMouseLeave()
    {
        last_mouse_pos_.SetPoint(-1, -1);
        UpdateTooltip();
    }

    ///////////////////////////////////////////////////////////////////////////////
    // AeroTooltipManager, private:

    void AeroTooltipManager::Init()
    {
        // Create the tooltip control.
        tooltip_hwnd_ = CreateWindowEx(
            WS_EX_TRANSPARENT|base::GetExtendedTooltipStyles(),
            TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0,
            GetParent(), NULL, NULL, NULL);

        base::AdjustUIFontForWindow(tooltip_hwnd_);

        // Add one tool that is used for all tooltips.
        toolinfo_.cbSize = sizeof(toolinfo_);

        // We use tracking tooltips on Vista to allow us to manually control the
        // visibility of the tooltip.
        toolinfo_.uFlags = TTF_TRANSPARENT | TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
        toolinfo_.hwnd = GetParent();
        toolinfo_.uId = (UINT_PTR)GetParent();

        // Setting this tells windows to call GetParent() back (using a WM_NOTIFY
        // message) for the actual tooltip contents.
        toolinfo_.lpszText = LPSTR_TEXTCALLBACK;
        SetRectEmpty(&toolinfo_.rect);
        ::SendMessage(tooltip_hwnd_, TTM_ADDTOOL, 0, (LPARAM)&toolinfo_);
    }

    void AeroTooltipManager::OnTimer()
    {
        initial_timer_ = NULL;

        POINT pt = last_mouse_pos_.ToPOINT();
        ::ClientToScreen(GetParent(), &pt);

        // Set the position and visibility.
        if(!tooltip_showing_)
        {
            ::SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
            ::SendMessage(tooltip_hwnd_, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x, pt.y));
            ::SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE, true, (LPARAM)&toolinfo_);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    // AeroTooltipManager::InitialTimer

    AeroTooltipManager::InitialTimer::InitialTimer(AeroTooltipManager* manager)
        : manager_(manager) {}

    void AeroTooltipManager::InitialTimer::Start(int time)
    {
		MessageLoop::current()->PostDelayedTask(std::bind(&InitialTimer::Execute, scoped_refptr<InitialTimer>(this)), time);
    }

    void AeroTooltipManager::InitialTimer::Disown()
    {
        manager_ = NULL;
    }

    void AeroTooltipManager::InitialTimer::Execute()
    {			
        if(manager_)
        {

            manager_->OnTimer();
        }
    }

} //namespace view