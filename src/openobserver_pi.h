/******************************************************************************
 * $Id: openobserver_pi.h,v 1.0 2015/01/28 01:54:37 jongough Exp $
 *
 * Project:  OpenCPN
 * Purpose:  OpenCPN General Drawing Plugin
 * Author:   Jon Gough
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
 *   $EMAIL$   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 */

#pragma once

#ifdef __WXMSW__
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <sstream>
#  define DEBUGSL(x) do { \
std::ostringstream oss; \
oss << x; \
time_t now = time(0); \
tm* localtm = localtime(&now); \
char *stime = asctime(localtm); \
stime[strlen(stime) - 1 ] = 0; \
std::string s1(oss.str()); \
std::string s = stime; \
s += " :: "; \
s += s1; \
s += "\n"; \
std::wstring stemp = std::wstring(s.begin(), s.end()); \
LPCWSTR sw = stemp.c_str(); \
OutputDebugString(sw); } while (0)

#  define DEBUGST(x) do { \
std::string s(""); \
std::ostringstream oss; \
oss << x; \
time_t now = time(0); \
tm* localtm = localtime(&now); \
char *stime = asctime(localtm); \
stime[strlen(stime) - 1 ] = 0; \
do { \
std::string s1(oss.str()); \
s += stime; \
s += " :: "; \
s += s1; } while (0);

#  define DEBUGCONT(x) do { \
std::ostringstream oss; \
oss << x; \
std::string s1(oss.str()); \
s += s1 ; } while (0);

#  define DEBUGEND(x) do { \
std::string s1(""); \
std::ostringstream oss; \
oss << x; \
s1 = oss.str(); \
s += s1; } while (0); \
s += "\n" ; \
std::wstring stemp = std::wstring(s.begin(), s.end()); \
LPCWSTR sw = stemp.c_str(); \
OutputDebugString(sw); } while (0)
#else
#  define DEBUGSL(x) do {} while (0)
#  define DEBUGST(x) do {} while (0)
#  define DEBUGCONT(x) do {} while (0)
#  define DEBUGEND(x) do {} while (0)
#endif
#else
#ifdef DEBUG_BUILD
#  define DEBUGSL(x) do { \
time_t now = time(0); \
tm* localtm = localtime(&now); \
char *stime = asctime(localtm); \
stime[strlen(stime) - 1 ] = 0; \
std::cout << stime << " :: "; \
std::cout << x << std::endl ;} while (0)

#  define DEBUGST(x) do { \
time_t now = time(0); \
tm* localtm = localtime(&now); \
char *stime = asctime(localtm); \
stime[strlen(stime) - 1 ] = 0; \
std::cout << stime << " :: " ; \
std::cout << x; } while (0)

#  define DEBUGCONT(x) do { \
std::cout << x ; } while (0)

#  define DEBUGEND(x) do { \
std::cout << x  << std::endl ; } while (0)
#else
#  define DEBUGSL(x) do {} while (0)
#  define DEBUGST(x) do {} while (0)
#  define DEBUGCONT(x) do {} while (0)
#  define DEBUGEND(x) do {} while (0)
#endif
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef __OCPN__ANDROID__
#include "qopengl.h"                  // this gives us the qt runtime gles2.h
#endif

#include "wxWTranslateCatalog.h"

#include "ocpn_plugin.h"

#include <wx/string.h>
#include <wx/settings.h>
#include <wx/statline.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>
#include <wx/splitter.h>
#include <wx/fileconf.h>
#include <wx/dynarray.h>
#include <set>
#include <unordered_map>

#include "ooObservations.h"
#include "globals.h"

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

class openobserver_pi : public opencpn_plugin_118
{
public:

    openobserver_pi(void *ppimgr);
    ~openobserver_pi();

    //    The required PlugIn Methods
    int Init(void);
    bool DeInit(void);

    int GetPlugInVersionMajor();
    int GetPlugInVersionMinor();
    int GetPlugInVersionPatch();
    int GetPlugInVersionPost();

    int GetAPIVersionMajor();
    int GetAPIVersionMinor();
    wxBitmap *GetPlugInBitmap();
    wxString GetCommonName();
    wxString GetShortDescription();
    wxString GetLongDescription();

    int GetToolbarToolCount(void);
    void OnToolbarToolCallback(int id);
    void OnToolbarToolDownCallback(int id);
    void OnToolbarToolUpCallback(int id);
    void OnContextMenuItemCallback(int id);

    void LateInit(void);
    bool KeyboardEventHook( wxKeyEvent &event );
    bool MouseEventHook( wxMouseEvent &event );
    void SetCursorLatLon(double lat, double lon);
    void SetPositionFix(PlugIn_Position_Fix &pfix);
    void SetNMEASentence(wxString& sentence);
    void SetCurrentViewPort(PlugIn_ViewPort& vp);
    
    void SetProject(const wxString& projectName,
                    const wxColor& projectColor,
                    int observationsIndex);

    void ToggleToolbarIcon();
    void ToggleWindow();
    bool IsLateInitDone() const;

    ooObservations *m_ooObservations;

    static void WriteNmeaXML(const std::unordered_map<wxString, std::set<int>>& scannedNmeaFields);
    static std::vector<NMEAField> ReadNmeaXML();
    static void RefreshListings();
  private:
    void    SaveConfig();
    void    LoadConfig();

    void    MenuPrepend(wxMenu *menu, int id, wxString label);
    void    MenuAppend(wxMenu *menu, int id, wxString label);

    wxWindow            *m_parent_window;
    wxFileConfig        *m_pConfig;

    tpicons *m_ptpicons;
    ooControlDialogImpl *m_ooControlDialogImpl;
    ooMiniDialogImpl *m_ooMiniDialogImpl;

    bool    m_bReadyForRequests;
    int     m_iCallerId;
    bool    m_bShowMainDialog;
    int     m_openobserver_button_id;
    int     m_observationsIndex, m_observationsChoiceCount;
    wxRect  m_dialogPosition, m_miniDialogPosition;
    int     m_addObservationItem;

    double  m_cursor_lat;
    double  m_cursor_lon;
    double  m_click_lat;
    double  m_click_lon;
};
