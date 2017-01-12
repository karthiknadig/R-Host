/* ****************************************************************************
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 *
 *
 * This file is part of Microsoft R Host.
 *
 * Microsoft R Host is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Microsoft R Host is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Microsoft R Host.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***************************************************************************/

#include "stdafx.h"
#include "host.h"

#ifdef __cplusplus
extern "C" {
#endif 

    // Taken from  graphapp.h
#ifndef objptr
    typedef struct { int kind; } gui_obj;
    typedef gui_obj * objptr;
#endif

    typedef objptr window;       /* on-screen window */
    typedef objptr control;      /* buttons, text-fields, scrollbars */
    typedef objptr progressbar;  /* progress bar */
    typedef objptr label;        /* text label */

    typedef struct rect rect;
    struct rect {
        int x, y;               /* top-left point inside rect */
        int width, height;      /* width and height of rect */
    };

    struct progress_data {
        std::string package;
        int max;
        int min;
        int value;
        int incr;
    };

    extern window __cdecl GA_newwindow(const char *name, rect r, long flags);
    extern label __cdecl GA_newlabel(const char *text, rect r, int alignment);
    extern progressbar __cdecl GA_newprogressbar(rect r, int pmin, int pmax, int incr, int smooth);
    extern window __cdecl GA_parentwindow(control c);
    extern void __cdecl GA_show(window w);
    extern void __cdecl GA_hide(window w);
    extern void __cdecl GA_setprogressbar(progressbar obj, int n);
    extern void __cdecl GA_stepprogressbar(progressbar obj, int n);
    extern void __cdecl GA_setprogressbarrange(progressbar obj, int pbmin, int pbmax);
    extern void __cdecl GA_settext(control c, const char *newtext);
    extern void __cdecl GA_delobj(objptr obj);
#ifdef __cplusplus
}
#endif 

std::unordered_map<progressbar, window> _pb_to_window;
std::unordered_map<window, progress_data> _window_to_data;
std::unordered_map<label, window> _label_to_window;

void AddLabel(label lb) {
    window w = GA_parentwindow(lb);
    // make sure label belongs to a known window,
    // specifically the "Download progress" window.
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _label_to_window[lb] = w;
    }
}

void RemoveLabel(label lb) {
    _label_to_window.erase(lb);
}

void AddProgressBar(progressbar pb, int min, int max, int incr) {
    window w = GA_parentwindow(pb);
    // make sure progress bar belongs to a known window,
    // specifically the "Download progress" window.
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _pb_to_window[pb] = w;
        _window_to_data[w].min = min;
        _window_to_data[w].max = max;
        _window_to_data[w].incr = incr;
    }
}

void RemoveProgressBar(progressbar pb) {
    _pb_to_window.erase(pb);
}

void AddWindow(window w, progress_data &data) {
    _window_to_data[w] = data;
}

void RemoveWindow(window w) {
    std::unordered_map<label, window>::iterator lb_iter = std::find_if(
        _label_to_window.begin(),
        _label_to_window.end(),
        [w](const std::pair<label, window>& p) {return p.second == w; });
    if (lb_iter != _label_to_window.end()) {
        RemoveLabel(lb_iter->first);
    }

    std::unordered_map<progressbar, window>::iterator pb_iter = std::find_if(
        _pb_to_window.begin(),
        _pb_to_window.end(),
        [w](const std::pair<label, window>& p) {return p.second == w; });
    if (pb_iter != _pb_to_window.end()) {
        RemoveProgressBar(pb_iter->first);
    }

    _window_to_data.erase(w);
}

void Cleanup() {
    _window_to_data.clear();
    _label_to_window.clear();
    _pb_to_window.clear();
}

void SetProgressBar(progressbar pb, int n) {
    window w = GA_parentwindow(pb);
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _window_to_data[w].value = n;
    }
}

void SetProgressBarRange(progressbar pb, int min, int max) {
    window w = GA_parentwindow(pb);
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _window_to_data[w].min = min;
        _window_to_data[w].max = max;
    }
}

bool IsLabel(control c) {
    return _label_to_window.find((label)c) != _label_to_window.end();
}

bool IsWindow(control c) {
    return _window_to_data.find((window)c) != _window_to_data.end();
}

void UpdatePackage(label lb, std::string text) {
    window w = _label_to_window[lb];
    if (_window_to_data.find(w) != _window_to_data.end()) {
        // uri format is : http://<path>/<package-name>_<version>.<extension>
        // extract package file name
        size_t ps = text.find_last_of("/");
        size_t pe = text.find_last_of("_");
        if (ps == std::string::npos || pe == std::string::npos) {
            _window_to_data[w].package = "<unknown>";
        } else {
            _window_to_data[w].package = text.substr(ps + 1, pe - ps - 1);
        }
    }
}

void NotifyProgressChanged(std::string package, std::string event, double min, double max, double value) {
    rhost::host::send_notification("!PkdDload", package, event, min, max, value);
}

void NotifyProgressStart(window w) {
    if (_window_to_data.find(w) != _window_to_data.end()) {
        NotifyProgressChanged(_window_to_data[w].package, "Start", 
            static_cast<double>(_window_to_data[w].min),
            static_cast<double>(_window_to_data[w].max),
            static_cast<double>(_window_to_data[w].min));
    }
}

void NotifyProgressEnd(window w) {
    if (_window_to_data.find(w) != _window_to_data.end()) {
        NotifyProgressChanged(_window_to_data[w].package, "End",
            static_cast<double>(_window_to_data[w].min),
            static_cast<double>(_window_to_data[w].max),
            static_cast<double>(_window_to_data[w].max));
    }
}

void NotifyProgressStep(window w) {
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _window_to_data[w].value = _window_to_data[w].value + _window_to_data[w].incr;
        NotifyProgressChanged(_window_to_data[w].package, "Step",
            static_cast<double>(_window_to_data[w].min),
            static_cast<double>(_window_to_data[w].max),
            static_cast<double>(_window_to_data[w].value));
    }
}

void NotifyProgressSet(window w, int value) {
    if (_window_to_data.find(w) != _window_to_data.end()) {
        _window_to_data[w].value = value;
        NotifyProgressChanged(_window_to_data[w].package, "Set",
            static_cast<double>(_window_to_data[w].min),
            static_cast<double>(_window_to_data[w].max),
            static_cast<double>(_window_to_data[w].value));
    }
}

#ifdef __cplusplus
extern "C" {
#endif

    // GA_newwindow
    decltype(GA_newwindow) *pGA_newwindow = nullptr;
    window __cdecl DetourGA_newwindow(const char *name, rect r, long flags) {
        window w = pGA_newwindow(name, r, flags);
        std::string strname(name);
        if (w && (strname == "Download progress")) {
            AddWindow(w, progress_data());
        }
        return w;
    }

    // GA_newlabel
    decltype(GA_newlabel) *pGA_newlabel = nullptr;
    label __cdecl DetourGA_newlabel(const char *text, rect r, int alignment) {
        label lb = pGA_newlabel(text, r, alignment);
        if (lb) {
            AddLabel(lb);
        }
        return lb;
    }

    // GA_newprogressbar
    decltype(GA_newprogressbar) *pGA_newprogressbar = nullptr;
    progressbar __cdecl DetourGA_newprogressbar(rect r, int pmin, int pmax, int incr, int smooth) {
        progressbar pb = pGA_newprogressbar(r, pmin, pmax, incr, smooth);
        if (pb) {
            AddProgressBar(pb, pmin, pmax, incr);
        }
        return pb;
    }

    // GA_setprogressbar
    decltype(GA_setprogressbar) *pGA_setprogressbar = nullptr;
    void __cdecl DetourGA_setprogressbar(progressbar obj, int n) {
        pGA_setprogressbar(obj, n);
        if (obj) {
            SetProgressBar(obj, n);
            NotifyProgressSet(GA_parentwindow(obj), n);
        }
    }

    // GA_setprogressbarrange
    decltype(GA_setprogressbarrange) *pGA_setprogressbarrange = nullptr;
    void __cdecl DetourGA_setprogressbarrange(progressbar obj, int pbmin, int pbmax) {
        pGA_setprogressbarrange(obj, pbmin, pbmax);
        if (obj) {
            SetProgressBarRange(obj, pbmin, pbmax);
        }
    }

    // GA_stepprogressbar
    decltype(GA_stepprogressbar) *pGA_stepprogressbar = nullptr;
    void __cdecl DetourGA_stepprogressbar(progressbar obj, int n) {
        pGA_stepprogressbar(obj, n);
        if (obj) {
            NotifyProgressStep(GA_parentwindow(obj));
        }
    }

    // GA_settext
    decltype(GA_settext) *pGA_settext = nullptr;
    void __cdecl DetourGA_settext(control c, const char *newtext) {
        pGA_settext(c, newtext);
        std::string text(newtext);
        if (IsLabel(c)) {
            UpdatePackage(c, text);
        }
    }

    // GA_show
    decltype(GA_show) *pGA_show = nullptr;
    void __cdecl DetourGA_show(window w) {
        pGA_show(w);
        NotifyProgressStart(w);
    }

    // GA_hide
    decltype(GA_hide) *pGA_hide = nullptr;
    void __cdecl DetourGA_hide(window w) {
        pGA_hide(w);
        NotifyProgressEnd(w);
    }

    // GA_delobj
    decltype(GA_delobj) *pGA_delobj = nullptr;
    void __cdecl DetourGA_delobj(objptr obj) {
        pGA_delobj(obj);
        if (IsWindow(obj)) {
            RemoveWindow(obj);
        }
    }

#ifdef __cplusplus
}
#endif

namespace rhost {
    namespace detours {
        // Converts host response back to MessageBox codes
        int ToMessageBoxCodes(int result, UINT mbType) {
            if (mbType == MB_YESNO) {
                return result == -1 ? IDNO : IDYES;
            } else if (mbType == MB_YESNOCANCEL) {
                return result == 0 ? IDCANCEL : (result == -1 ? IDNO : IDYES);
            }
            return result == 0 ? IDCANCEL : IDOK;
        }

        // Displays host message box. Host provides title and the parent window.
        // Communication with the host is in UTF-8 and single-byte characters
        // are converted to UTF-8 JSON in the rhost::host::ShowMessageBox().
        int HostMessageBox(LPCSTR lpText, UINT uType) {
            UINT mbType = uType & 0x0F;
            if (mbType == MB_OKCANCEL) {
                return ToMessageBoxCodes(rhost::host::OkCancel(lpText), mbType);
            } else if (mbType == MB_YESNO) {
                return ToMessageBoxCodes(rhost::host::YesNo(lpText), mbType);
            } else if (mbType == MB_YESNOCANCEL) {
                return ToMessageBoxCodes(rhost::host::YesNoCancel(lpText), mbType);
            }
            rhost::host::ShowMessage(lpText);
            return IDOK;
        }

        // MessageBoxW
        decltype(MessageBoxW) *pMessageBoxW = NULL;
        int WINAPI DetourMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
            char ch[1000];
            wcstombs(ch, lpText, _countof(ch));
            return HostMessageBox(ch, uType);
        }

        // MessageBoxA
        decltype(MessageBoxW) *pMessageBoxA = NULL;
        int WINAPI DetourMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
            return HostMessageBox(lpText, uType);
        }

        void init_ui_detours() {
            MH_Initialize();
            MH_CreateHook(&MessageBoxW, &DetourMessageBoxW, reinterpret_cast<LPVOID*>(&pMessageBoxW));
            MH_CreateHook(&MessageBoxA, &DetourMessageBoxA, reinterpret_cast<LPVOID*>(&pMessageBoxA));

            MH_CreateHookApi(L"rgraphapp", "GA_delobj", &DetourGA_delobj, reinterpret_cast<LPVOID*>(&pGA_delobj));
            MH_CreateHookApi(L"rgraphapp", "GA_hide", &DetourGA_hide, reinterpret_cast<LPVOID*>(&pGA_hide));
            MH_CreateHookApi(L"rgraphapp", "GA_newlabel", &DetourGA_newlabel, reinterpret_cast<LPVOID*>(&pGA_newlabel));
            MH_CreateHookApi(L"rgraphapp", "GA_newprogressbar", &DetourGA_newprogressbar, reinterpret_cast<LPVOID*>(&pGA_newprogressbar));
            MH_CreateHookApi(L"rgraphapp", "GA_newwindow", &DetourGA_newwindow, reinterpret_cast<LPVOID*>(&pGA_newwindow));
            MH_CreateHookApi(L"rgraphapp", "GA_setprogressbar", &DetourGA_setprogressbar, reinterpret_cast<LPVOID*>(&pGA_setprogressbar));
            MH_CreateHookApi(L"rgraphapp", "GA_setprogressbarrange", &DetourGA_setprogressbarrange, reinterpret_cast<LPVOID*>(&pGA_setprogressbarrange));
            MH_CreateHookApi(L"rgraphapp", "GA_settext", &DetourGA_settext, reinterpret_cast<LPVOID*>(&pGA_settext));
            MH_CreateHookApi(L"rgraphapp", "GA_show", &DetourGA_show, reinterpret_cast<LPVOID*>(&pGA_show));
            MH_CreateHookApi(L"rgraphapp", "GA_stepprogressbar", &DetourGA_stepprogressbar, reinterpret_cast<LPVOID*>(&pGA_stepprogressbar));

            MH_EnableHook(MH_ALL_HOOKS);
        }

        void terminate_ui_detours() {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
        }
    }
}