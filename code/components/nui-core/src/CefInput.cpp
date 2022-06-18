/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "CefOverlay.h"
#include "CrossLibraryInterfaces.h"

#include "CefImeHandler.h"
#include "NUIRenderHandler.h"

#include <HostSharedData.h>
#include <ReverseGameData.h>

#include <windowsx.h>
#include <console/Console.VariableHelpers.h>

using nui::HasFocus;

extern nui::GameInterface* g_nuiGi;

static bool g_hasFocus = false;
bool g_hasCursor = false;
bool g_keepInput = false;
static bool g_hasOverriddenFocus = false;
extern bool g_mainUIFlag;
POINT g_cursorPos;

static ConVar<bool> uiLoadingCursor("ui_loadingCursor", ConVar_None, false);

bool isKeyDown(WPARAM wparam)
{
	return (GetKeyState(wparam) & 0x8000) != 0;
}

#include <shared_mutex>

#ifdef USE_NUI_ROOTLESS
std::shared_mutex g_nuiFocusStackMutex;
std::list<std::string> g_nuiFocusStack;
#endif

static CefRefPtr<CefBrowser> GetFocusBrowser()
{
#ifdef USE_NUI_ROOTLESS
	std::shared_lock<std::shared_mutex> lock(g_nuiFocusStackMutex);

	for (const auto& entry : g_nuiFocusStack)
	{
		auto browser = nui::GetNUIWindowBrowser(entry);

		if (browser)
		{
			return browser;
		}
	}

	return {};
#else
	return nui::GetBrowser();
#endif
}

namespace nui
{
	CefBrowser* GetFocusBrowser()
	{
		return ::GetFocusBrowser().get();
	}

	extern fwRefContainer<NUIWindow> FindNUIWindow(fwString windowName);

	bool HasFocus()
	{
		return (g_hasFocus || g_hasOverriddenFocus);
	}

	bool HasFocusKeepInput()
	{
		return g_keepInput;
	}

	void GiveFocus(const std::string& frameName, bool hasFocus, bool hasCursor)
	{
		if (!HasFocus() && hasFocus)
		{
			g_nuiGi->SetGameMouseFocus(false);
		}
		else if (!hasFocus && HasFocus())
		{
			g_nuiGi->SetGameMouseFocus(true);
		}

		g_hasFocus = hasFocus;
		g_hasCursor = hasCursor;

#ifdef USE_NUI_ROOTLESS
		auto winName = fmt::sprintf("nui_%s", frameName);

		auto browser = nui::GetNUIWindowBrowser(winName);
		auto window = FindNUIWindow(winName);

		if (hasFocus)
		{
			// deferred-create the window if it's given focus, too
			if (window.GetRef())
			{
				if (!window->GetBrowser())
				{
					window->DeferredCreate();
				}
			}

			static std::string oldDD;
			std::unique_lock<std::shared_mutex> lock(g_nuiFocusStackMutex);

			// remove from focus stack so it can be moved on top
			for (auto it = g_nuiFocusStack.begin(); it != g_nuiFocusStack.end();)
			{
				if (*it == winName)
				{
					it = g_nuiFocusStack.erase(it);
				}
				else
				{
					++it;
				}
			}

			g_nuiFocusStack.push_front(winName);

			if (oldDD != g_nuiFocusStack.front())
			{
				if (browser)
				{
					auto rh = browser->GetHost()->GetClient()->GetRenderHandler();
					NUIRenderHandler* nrh = (NUIRenderHandler*)rh.get();

					RevokeDragDrop(g_nuiGi->GetHWND());
					
					HRESULT hr = RegisterDragDrop(g_nuiGi->GetHWND(), nrh->GetDropTarget());
					if (FAILED(hr))
					{
						trace("registering drag/drop failed. hr: %08x\n", hr);
					}
				}
			}
		}
		else
		{
			RevokeDragDrop(g_nuiGi->GetHWND());
		}

		if (browser)
		{
			browser->GetHost()->SetFocus(hasFocus);
		}
		else
		{
			if (window.GetRef())
			{
				window->PushLoadQueue([window, hasFocus]()
				{
					if (window->GetBrowser())
					{
						window->GetBrowser()->GetHost()->SetFocus(hasFocus);
					}
				});
			}
		}
#endif
	}

	void OverrideFocus(bool hasFocus)
	{
		if (!HasFocus() && hasFocus)
		{
			g_nuiGi->SetGameMouseFocus(false);
		}
		else if (!hasFocus && HasFocus())
		{
			g_nuiGi->SetGameMouseFocus(true);
		}

		g_hasOverriddenFocus = hasFocus;

		if (uiLoadingCursor.GetValue())
		{
			g_hasCursor = hasFocus;
		}
	}

	void KeepInput(bool keepInput)
	{
		if (keepInput && HasFocus())
		{
			g_nuiGi->SetGameMouseFocus(true);
		}
		else if (!keepInput && HasFocus())
		{
			g_nuiGi->SetGameMouseFocus(false);
		}

		g_keepInput = keepInput;
	}

	void ProcessInput()
	{
	}
}

int GetCefKeyboardModifiers(WPARAM wparam, LPARAM lparam)
{
	int modifiers = 0;
	if (isKeyDown(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (isKeyDown(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (isKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam)
	{
		case VK_RETURN:
			if ((lparam >> 16) & KF_EXTENDED)
				modifiers |= EVENTFLAG_IS_KEY_PAD;
			break;
		case VK_INSERT:
		case VK_DELETE:
		case VK_HOME:
		case VK_END:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_UP:
		case VK_DOWN:
		case VK_LEFT:
		case VK_RIGHT:
			if (!((lparam >> 16) & KF_EXTENDED))
				modifiers |= EVENTFLAG_IS_KEY_PAD;
			break;
		case VK_NUMLOCK:
		case VK_NUMPAD0:
		case VK_NUMPAD1:
		case VK_NUMPAD2:
		case VK_NUMPAD3:
		case VK_NUMPAD4:
		case VK_NUMPAD5:
		case VK_NUMPAD6:
		case VK_NUMPAD7:
		case VK_NUMPAD8:
		case VK_NUMPAD9:
		case VK_DIVIDE:
		case VK_MULTIPLY:
		case VK_SUBTRACT:
		case VK_ADD:
		case VK_DECIMAL:
		case VK_CLEAR:
			modifiers |= EVENTFLAG_IS_KEY_PAD;
			break;
		case VK_SHIFT:
			if (isKeyDown(VK_LSHIFT))
				modifiers |= EVENTFLAG_IS_LEFT;
			else if (isKeyDown(VK_RSHIFT))
				modifiers |= EVENTFLAG_IS_RIGHT;
			break;
		case VK_CONTROL:
			if (isKeyDown(VK_LCONTROL))
				modifiers |= EVENTFLAG_IS_LEFT;
			else if (isKeyDown(VK_RCONTROL))
				modifiers |= EVENTFLAG_IS_RIGHT;
			break;
		case VK_MENU:
			if (isKeyDown(VK_LMENU))
				modifiers |= EVENTFLAG_IS_LEFT;
			else if (isKeyDown(VK_RMENU))
				modifiers |= EVENTFLAG_IS_RIGHT;
			break;
		case VK_LWIN:
			modifiers |= EVENTFLAG_IS_LEFT;
			break;
		case VK_RWIN:
			modifiers |= EVENTFLAG_IS_RIGHT;
			break;
	}
	return modifiers;
}

OsrImeHandlerWin* g_imeHandler;

int GetCefMouseModifiers(WPARAM wparam) {
	int modifiers = 0;
	if (wparam & MK_CONTROL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (wparam & MK_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (isKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (wparam & MK_LBUTTON)
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (wparam & MK_MBUTTON)
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (wparam & MK_RBUTTON)
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	return modifiers;
}

int GetCefMouseModifiers() {
	int modifiers = 0;
	if (isKeyDown(VK_CONTROL) || isKeyDown(VK_LCONTROL) || isKeyDown(VK_RCONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (isKeyDown(VK_SHIFT) || isKeyDown(VK_LSHIFT) || isKeyDown(VK_RSHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (isKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (isKeyDown(VK_LBUTTON))
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (isKeyDown(VK_MBUTTON))
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (isKeyDown(VK_RBUTTON))
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	return modifiers;
}

static void AlterKeyEventForAltGr(CefKeyEvent& event, WPARAM wParam)
{
	// mimic alt-gr check behaviour from
	// src/ui/events/win/events_win_utils.cc: GetModifiersFromKeyState
	if ((event.type == KEYEVENT_CHAR) && isKeyDown(VK_RMENU))
	{
		// reverse AltGr detection taken from PlatformKeyMap::UsesAltGraph
		// instead of checking all combination for ctrl-alt, just check current char
		HKL current_layout = ::GetKeyboardLayout(0);

		// https://docs.microsoft.com/en-gb/windows/win32/api/winuser/nf-winuser-vkkeyscanexw
		// ... high-order byte contains the shift state,
		// which can be a combination of the following flag bits.
		// 2 Either CTRL key is pressed.
		// 4 Either ALT key is pressed.
		SHORT scan_res = ::VkKeyScanExW(wParam, current_layout);
		if (((scan_res >> 8) & 0xFF) == (2 | 4))
		{ // ctrl-alt pressed
			event.modifiers &= ~(EVENTFLAG_CONTROL_DOWN | EVENTFLAG_ALT_DOWN);
			event.modifiers |= EVENTFLAG_ALTGR_DOWN;
		}
	}
}

static HookFunction initFunction([] ()
{
	g_nuiGi->QueryMayLockCursor.Connect([](int& argPtr)
	{
		if (HasFocus() && !g_keepInput)
		{
			argPtr = 0;
		}
	});

	static bool g_lastFocus = false;

	static struct NuiInputTarget : InputTarget
	{
		void KeyEvent(UINT vKey, UINT scanCode, bool down)
		{
			static HostSharedData<ReverseGameData> rgd("CfxReverseGameData");

			auto browser = GetFocusBrowser();
			if (!browser)
			{
				return;
			}

			CefKeyEvent keyEvent;

			keyEvent.windows_key_code = vKey;
			keyEvent.native_key_code = scanCode;
			keyEvent.modifiers = GetCefKeyboardModifiers(vKey, scanCode);

			if (down)
			{
				keyEvent.type = KEYEVENT_RAWKEYDOWN;

				if (vKey == VK_RETURN && launch::IsSDKGuest())
				{
					keyEvent.character = '\r';
					keyEvent.unmodified_character = '\r';
					
					browser->GetHost()->SendKeyEvent(keyEvent);

					keyEvent.type = KEYEVENT_CHAR;
				}

				if (rgd->inputChar)
				{
					keyEvent.character = rgd->inputChar;
					keyEvent.unmodified_character = rgd->inputChar;

					browser->GetHost()->SendKeyEvent(keyEvent);

					keyEvent.windows_key_code = rgd->inputChar;
					keyEvent.type = KEYEVENT_CHAR;
				}
			}
			else
			{
				keyEvent.type = KEYEVENT_KEYUP;
			}

			browser->GetHost()->SendKeyEvent(keyEvent);
		}

		void MouseEvent(int button, int x, int y, bool down)
		{
			static int lastClickX;
			static int lastClickY;
			static int lastClickCount;
			static LONG lastClickTime;
			static CefBrowserHost::MouseButtonType lastClickButton;
			static bool mouseTracking;

			LONG currentTime = 0;
			bool cancelPreviousClick = false;

			lastX = x;
			lastY = y;

			if (down)
			{
				currentTime = GetMessageTime();
				cancelPreviousClick =
					(abs(lastClickX - x) > (GetSystemMetrics(SM_CXDOUBLECLK) / 2))
					|| (abs(lastClickY - y) > (GetSystemMetrics(SM_CYDOUBLECLK) / 2))
					|| ((currentTime - lastClickTime) > GetDoubleClickTime());
				if (cancelPreviousClick &&
					(button == -1 /* move */)) {
					lastClickCount = 0;
					lastClickX = 0;
					lastClickY = 0;
					lastClickTime = 0;
				}
			}

			if (button == -1)
			{
				auto browser = GetFocusBrowser();

				if (browser)
				{
					CefMouseEvent mouse_event;
					mouse_event.x = x;
					mouse_event.y = y;
					mouse_event.modifiers = GetCefMouseModifiers();
					browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
				}
			}
			else if (down)
			{
				CefBrowserHost::MouseButtonType btnType =
					((button == 0) ? MBT_LEFT : ((button == 1) ? MBT_RIGHT : MBT_MIDDLE));
				if (!cancelPreviousClick && (btnType == lastClickButton))
				{
					++lastClickCount;
				}
				else
				{
					lastClickCount = 1;
					lastClickX = x;
					lastClickY = y;
				}

				lastClickTime = currentTime;
				lastClickButton = btnType;

				CefMouseEvent mouse_event;
				mouse_event.x = x;
				mouse_event.y = y;
				mouse_event.modifiers = GetCefMouseModifiers();

				auto browser = GetFocusBrowser();

				if (browser)
				{
					browser->GetHost()->SendMouseClickEvent(mouse_event, btnType, false, lastClickCount);
				}
			}
			else
			{
				CefBrowserHost::MouseButtonType btnType =
					((button == 0) ? MBT_LEFT : ((button == 1) ? MBT_RIGHT : MBT_MIDDLE));

				auto browser = GetFocusBrowser();

				if (browser)
				{
					CefMouseEvent mouse_event;
					mouse_event.x = x;
					mouse_event.y = y;
					mouse_event.modifiers = GetCefMouseModifiers();
					browser->GetHost()->SendMouseClickEvent(mouse_event, btnType, true, lastClickCount);
				}
			}
		}

		virtual void KeyDown(UINT vKey, UINT scanCode) override
		{
			KeyEvent(vKey, scanCode, true);
		}

		virtual void KeyUp(UINT vKey, UINT scanCode) override
		{
			KeyEvent(vKey, scanCode, false);
		}

		virtual void MouseDown(int buttonIdx, int x, int y) override
		{
			MouseEvent(buttonIdx, x, y, true);
		}

		virtual void MouseUp(int buttonIdx, int x, int y) override
		{
			MouseEvent(buttonIdx, x, y, false);
		}

		virtual void MouseWheel(int deltaY) override
		{
			auto browser = GetFocusBrowser();

			if (browser) {
				int delta = deltaY * 120;

				CefMouseEvent mouse_event;
				mouse_event.x = lastX;
				mouse_event.y = lastY;
				mouse_event.modifiers = GetCefMouseModifiers();

				browser->GetHost()->SendMouseWheelEvent(mouse_event,
					isKeyDown(VK_SHIFT) ? delta : 0,
					!isKeyDown(VK_SHIFT) ? delta : 0);
			}
		}

		virtual void MouseMove(int x, int y) override
		{
			MouseEvent(-1, x, y, true);
		}

		int lastX;
		int lastY;

	} inputTarget;

	g_nuiGi->QueryInputTarget.Connect([](std::vector<InputTarget*>& targets)
	{
		auto browser = GetFocusBrowser();

		if (browser)
		{
			if (HasFocus() != g_lastFocus)
			{
				browser->GetHost()->SendFocusEvent(HasFocus());
			}

			g_lastFocus = HasFocus();
		}

		if (HasFocus())
		{
			targets.push_back(&inputTarget);

			return false;
		}

		return true;
	});

	g_nuiGi->OnWndProc.Connect([](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& pass, LRESULT& lresult)
	{
		if (!pass)
		{
			return;
		}

		if (nui::HasMainUI())
		{
			if (msg == WM_CLOSE)
			{
				ExitProcess(0);
				pass = false;

				return;
			}
		}

		// send a focus event to CEF if focus changed
		auto browser = GetFocusBrowser();

		if (browser)
		{
			if (HasFocus() != g_lastFocus)
			{
				browser->GetHost()->SendFocusEvent(HasFocus());
			}

			g_lastFocus = HasFocus();
		}

		if (HasFocus())
		{
			if (!g_imeHandler)
			{
				g_imeHandler = new OsrImeHandlerWin(g_nuiGi->GetHWND());
			}

			static bool mouseTracking;

			switch (msg)
			{
			case WM_XBUTTONUP: {
				auto button = GET_XBUTTON_WPARAM(wParam);
				auto browser = GetFocusBrowser();

				// we have a MainUI check here so that behavior for *existing* code doesn't change
				if (browser && nui::HasMainUI())
				{
					if (button == XBUTTON1 && browser->CanGoBack())
					{
						browser->GoBack();
					}
					else if (button == XBUTTON2 && browser->CanGoForward())
					{
						browser->GoForward();
					}
				}
			} break;
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_LBUTTONDBLCLK: 
			case WM_RBUTTONDBLCLK: 
			case WM_MBUTTONDBLCLK: {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				int btnType =
					((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) ? 0 : (
						(msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) ? 1 : 2));
				
				inputTarget.MouseEvent(btnType, x, y, true);

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
			} break;

			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP: {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				int btnType =
					((msg == WM_LBUTTONUP) ? 0 : (
					(msg == WM_RBUTTONUP) ? 1 : 2));

				inputTarget.MouseEvent(btnType, x, y, false);

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}

				break;
			}
			case WM_MOUSEMOVE: {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				if (!mouseTracking) {
					// Start tracking mouse leave. Required for the WM_MOUSELEAVE event to
					// be generated.
					TRACKMOUSEEVENT tme;
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_LEAVE;
					tme.hwndTrack = hWnd;
					TrackMouseEvent(&tme);
					mouseTracking = true;
				}

				inputTarget.MouseEvent(-1, x, y, true);

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
				break;
			}

			case WM_MOUSELEAVE: {
				if (mouseTracking) {
					// Stop tracking mouse leave.
					TRACKMOUSEEVENT tme;
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_LEAVE & TME_CANCEL;
					tme.hwndTrack = hWnd;
					TrackMouseEvent(&tme);
					mouseTracking = false;
				}

				auto browser = GetFocusBrowser();

				if (browser) {
					// Determine the cursor position in screen coordinates.
					POINT p;
					::GetCursorPos(&p);
					::ScreenToClient(hWnd, &p);

					CefMouseEvent mouse_event;
					mouse_event.x = p.x;
					mouse_event.y = p.y;
					mouse_event.modifiers = GetCefMouseModifiers(wParam);
					browser->GetHost()->SendMouseMoveEvent(mouse_event, true);
				}

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
			} break;

			case WM_MOUSEWHEEL: {
				inputTarget.MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / 120);

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
				break;
			}
			}

			if (msg == WM_KEYUP || msg == WM_KEYDOWN)
			{
				inputTarget.KeyEvent(wParam, lParam, (msg == WM_KEYDOWN));

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
			}
			else if (msg == WM_CHAR)
			{
				CefKeyEvent keyEvent;

				keyEvent.windows_key_code = wParam;
				keyEvent.native_key_code = lParam;
				keyEvent.modifiers = GetCefKeyboardModifiers(wParam, lParam);

				if (msg != WM_CHAR)
				{
					keyEvent.type = (msg == WM_KEYDOWN) ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
				}
				else
				{
					keyEvent.type = KEYEVENT_CHAR;
				}

				auto browser = GetFocusBrowser();

				if (browser)
				{
					AlterKeyEventForAltGr(keyEvent, wParam);
					browser->GetHost()->SendKeyEvent(keyEvent);
				}

				pass = false;
				lresult = FALSE;
				return;
			}
			else if (msg == WM_INPUT && g_hasCursor && !g_keepInput)
			{
				pass = false;
				lresult = TRUE;
				return;
			}
			else if (msg == WM_IME_STARTCOMPOSITION)
			{
				if (g_imeHandler)
				{
					g_imeHandler->CreateImeWindow();
					g_imeHandler->MoveImeWindow();
					g_imeHandler->ResetComposition();
				}

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
				return;
			}
			else if (msg == WM_IME_SETCONTEXT)
			{
				// We handle the IME Composition Window ourselves (but let the IME Candidates
				// Window be handled by IME through DefWindowProc()), so clear the
				// ISC_SHOWUICOMPOSITIONWINDOW flag:
				lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
				::DefWindowProc(hWnd, msg, wParam, lParam);

				// Create Caret Window if required
				if (g_imeHandler) {
					g_imeHandler->CreateImeWindow();
					g_imeHandler->MoveImeWindow();
				}

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}
				return;
			}
			else if (msg == WM_IME_COMPOSITION)
			{
				auto browser = GetFocusBrowser();

				if (browser && g_imeHandler) {
					CefString cTextStr;
					if (g_imeHandler->GetResult(lParam, cTextStr)) {
						// Send the text to the browser. The |replacement_range| and
						// |relative_cursor_pos| params are not used on Windows, so provide
						// default invalid values.
						browser->GetHost()->ImeCommitText(cTextStr,
							CefRange(UINT32_MAX, UINT32_MAX), 0);
						g_imeHandler->ResetComposition();
						// Continue reading the composition string - Japanese IMEs send both
						// GCS_RESULTSTR and GCS_COMPSTR.
					}

					std::vector<CefCompositionUnderline> underlines;
					int composition_start = 0;

					if (g_imeHandler->GetComposition(lParam, cTextStr, underlines,
						composition_start)) {
						// Send the composition string to the browser. The |replacement_range|
						// param is not used on Windows, so provide a default invalid value.
						browser->GetHost()->ImeSetComposition(
							cTextStr, underlines, CefRange(UINT32_MAX, UINT32_MAX),
							CefRange(composition_start,
								static_cast<int>(composition_start + cTextStr.length())));

						// Update the Candidate Window position. The cursor is at the end so
						// subtract 1. This is safe because IMM32 does not support non-zero-width
						// in a composition. Also,  negative values are safely ignored in
						// MoveImeWindow
						g_imeHandler->UpdateCaretPosition(composition_start - 1);
					}
					else {
						browser->GetHost()->ImeCancelComposition();
						g_imeHandler->ResetComposition();
						g_imeHandler->DestroyImeWindow();
					}
				}

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}

				return;
			}
			else if (msg == WM_IME_ENDCOMPOSITION)
			{
				browser->GetHost()->ImeCancelComposition();
				g_imeHandler->ResetComposition();
				g_imeHandler->DestroyImeWindow();

				if (!g_keepInput)
				{
					pass = false;
					lresult = FALSE;
				}

				return;
			}
			else if ((msg == WM_IME_KEYLAST || msg == WM_IME_KEYDOWN || msg == WM_IME_KEYUP) && !g_keepInput)
			{
				pass = false;
				lresult = false;

				return;
			}
		}
	});
});
