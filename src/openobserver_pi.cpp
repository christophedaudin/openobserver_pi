/******************************************************************************
 * updated: 4-5-2012
 * Project:  OpenCPN
 * Purpose:  Open Observer Plugin
 * Author:   Jon Gough
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers
#include <wx/stdpaths.h>
#include <wx/timer.h>
#include <wx/event.h>
#include <wx/sysopt.h>
#include <wx/dir.h>
#include <wx/stdpaths.h>
#include <wx/filefn.h>
#include <wx/msgdlg.h>
#include <wx/listbook.h>
#include <wx/panel.h>
#include <wx/ffile.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>

#include <wx/aui/aui.h>
#include <wx/gdicmn.h>

#include "openobserver_pi.h"
#include "version.h"
#include "wxWTranslateCatalog.h"

#include "tpicons.h"
#include "ooObservations.h"
#include "ooControlDialogImpl.h"
#include "ooMiniDialogImpl.h"

#include "wx/jsonwriter.h"


#ifndef DECL_EXP
#ifdef __WXMSW__
#define DECL_EXP     __declspec(dllexport)
#else
#define DECL_EXP
#endif
#endif

#if !defined(NAN)
static const long long lNaN = 0xfff8000000000000;
#define NAN (*(double*)&lNaN)
#endif

openobserver_pi         *g_openobserver_pi;
wxString                *g_PrivateDataDir;

wxString                *g_pData;
wxString                *g_SData_Locn;
wxString                *g_pLayerDir;
wxString                *g_pListingDir;

wxString                *g_tplocale;
void                    *g_ppimgr;

int                     g_iLocaleDepth;

wxFont                  *g_pFontTitle;
wxFont                  *g_pFontData;
wxFont                  *g_pFontLabel;
wxFont                  *g_pFontSmall;

// Needed for ocpndc.cpp to compile. Normally would be in glChartCanvas.cpp
float g_GLMinSymbolLineWidth;

void appendOSDirSlash(wxString* pString)
{
    wxChar sep = wxFileName::GetPathSeparator();

    if (pString->Last() != sep)
        pString->Append(sep);
}

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new openobserver_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}


//---------------------------------------------------------------------------------------------------------
//
//    openobserver PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

openobserver_pi::openobserver_pi(void *ppimgr) 
    : opencpn_plugin_118(ppimgr), m_ooObservations(nullptr), m_ooControlDialogImpl(nullptr), m_ooMiniDialogImpl(nullptr)
{
    // Create the PlugIn icons
    g_ppimgr = ppimgr;
    g_openobserver_pi = this;

    wxString *l_pDir = new wxString(*GetpPrivateApplicationDataLocation());
    appendOSDirSlash(l_pDir);

    l_pDir->Append(_T("plugins"));
    appendOSDirSlash(l_pDir);
    if (!wxDir::Exists(*l_pDir))
        wxMkdir(*l_pDir);

    g_PrivateDataDir = new wxString(*l_pDir);
    g_PrivateDataDir->Append(_T("openobserver_pi"));
    appendOSDirSlash(g_PrivateDataDir);
    if (!wxDir::Exists(*g_PrivateDataDir))
        wxMkdir(*g_PrivateDataDir);

    g_pData = new wxString(*g_PrivateDataDir);
    g_pData->append(wxS("data"));
    appendOSDirSlash(g_pData);
    if (!wxDir::Exists(*g_pData))
        wxMkdir(*g_pData);

    g_pLayerDir = new wxString(*g_PrivateDataDir);
    g_pLayerDir->Append(wxT("Layers"));
    appendOSDirSlash(g_pLayerDir);
    if (!wxDir::Exists(*g_pLayerDir))
        wxMkdir(*g_pLayerDir);

    g_pListingDir = new wxString(*g_PrivateDataDir);
    g_pListingDir->Append(wxT("Listings"));
    appendOSDirSlash(g_pListingDir);
    if (!wxDir::Exists(*g_pListingDir))
        wxMkdir(*g_pListingDir);

    // Copy packaged listings
    const wxString exePath = GetOCPN_ExePath();
    const wxUniChar sep = wxFileName::GetPathSeparator();
    const wxString packagedListingsPath =
        wxString::Format(wxT("%s%c%s%c%s%c%s%c%s"),
            wxFileName(exePath).GetPath(),
            sep, "plugins", sep, "openobserver_pi", sep, "data", sep, "Listings");
    
    wxArrayString allPackagedListings;
    wxDir::GetAllFiles(packagedListingsPath, &allPackagedListings);
    for (auto f : allPackagedListings) {
        wxString filename = wxFileName(f).GetFullName();
        wxString targetPath = wxString::Format(wxT("%s%s"), *g_pListingDir, filename);
        if (!wxFile::Exists(targetPath)) {
            wxCopyFile(f, targetPath);
        }
    }

    ooObservations::SetNMEAFields(ReadNmeaXML());

    m_ptpicons = new tpicons();

    delete l_pDir;
}

openobserver_pi::~openobserver_pi()
{
    delete g_SData_Locn;
    g_SData_Locn = NULL;

    delete g_PrivateDataDir;
    g_PrivateDataDir = NULL;

    delete g_pData;
    g_pData = NULL;

    delete g_pLayerDir;
    g_pLayerDir = NULL;

    delete g_pListingDir;
    g_pListingDir = NULL;
}

int openobserver_pi::Init(void)
{
    g_tplocale = nullptr;
    m_bReadyForRequests = false;
    m_bShowMainDialog = true;
    m_ooControlDialogImpl = nullptr;
    m_ooMiniDialogImpl = nullptr;
    m_cursor_lat = 0.0;
    m_cursor_lon = 0.0;
    m_click_lat = 0.0;
    m_click_lon = 0.0;
    m_observationsIndex = -1;
    m_observationsChoiceCount = -1;

    // Adds local language support for the plugin to OCPN
    AddLocaleCatalog(PLUGIN_CATALOG_NAME);

    // Get a pointer to the opencpn display canvas, to use as a parent for windows created
    m_parent_window = GetOCPNCanvasWindow();
    m_pConfig = GetOCPNConfigObject();
    LoadConfig();

#ifdef PLUGIN_USE_SVG
    m_openobserver_button_id  = InsertPlugInToolSVG(_("Open Observer Plugin"), m_ptpicons->m_s_openobserver_grey_pi, m_ptpicons->m_s_openobserver_pi, m_ptpicons->m_s_openobserver_toggled_pi, wxITEM_CHECK,
                                                  _("Open Observer Plugin"), wxS(""), NULL, openobserver_POSITION, 0, this);
#else
    m_openobserver_button_id  = InsertPlugInTool(_("Open Observer Plugin"), &m_ptpicons->m_bm_openobserver_grey_pi, &m_ptpicons->m_bm_openobserver_pi, wxITEM_CHECK,
                                             _("Open Observer Plugin"), wxS(""), NULL, openobserver_POSITION, 0, this);
#endif

    //    In order to avoid an ASSERT on msw debug builds,
    //    we need to create a dummy menu to act as a surrogate parent of the created MenuItems
    //    The Items will be re-parented when added to the real context meenu
    wxMenu dummy_menu;
    wxMenuItem* pmi =
        new wxMenuItem(&dummy_menu, -1, _("Add OO observation here"));
    m_addObservationItem = AddCanvasContextMenuItem(pmi, this);
    SetCanvasContextMenuItemViz(m_addObservationItem, true);

    // Get item into font list in options/user interface
    AddPersistentFontKey( wxT("tp_Label") );
    AddPersistentFontKey( wxT("tp_Data") );
    g_pFontTitle = GetOCPNScaledFont_PlugIn( wxS("tp_Title") );
    g_pFontLabel = GetOCPNScaledFont_PlugIn( wxS("tp_Label") );
    g_pFontData = GetOCPNScaledFont_PlugIn( wxS("tp_Data") );
    g_pFontSmall = GetOCPNScaledFont_PlugIn( wxS("tp_Small") );
    wxColour l_fontcolour = GetFontColour_PlugIn( wxS("tp_Label") );
    l_fontcolour = GetFontColour_PlugIn( wxS("tp_Data") );

    m_ooObservations = new ooObservations();

    m_ooMiniDialogImpl = new ooMiniDialogImpl(m_parent_window);
    m_ooMiniDialogImpl->Fit();
    m_ooMiniDialogImpl->Layout();
    m_ooMiniDialogImpl->Hide();
    m_ooMiniDialogImpl->Move(m_miniDialogPosition.x, m_miniDialogPosition.y);
    m_ooMiniDialogImpl->SetSize(m_miniDialogPosition.width,
                                m_miniDialogPosition.height);

    m_ooControlDialogImpl = new ooControlDialogImpl(m_parent_window);
    m_ooControlDialogImpl->SetObservationsChoiceCount(m_observationsChoiceCount);
    m_ooControlDialogImpl->CreateObservationsTable(m_ooObservations);

    // restore backup observations and start backing up on a timer
    if (!m_ooControlDialogImpl->RestoreBackupObservations(m_observationsIndex)) {
        m_ooControlDialogImpl->NewProject();
        m_ooControlDialogImpl->UseProject();
    }

    m_ooControlDialogImpl->Fit();
    m_ooControlDialogImpl->Layout();
    m_ooControlDialogImpl->Hide();
    m_ooControlDialogImpl->Move(m_dialogPosition.x, m_dialogPosition.y);
    m_ooControlDialogImpl->SetSize(m_dialogPosition.width,
                                   m_dialogPosition.height);

    return (
        WANTS_CURSOR_LATLON       |
        WANTS_TOOLBAR_CALLBACK    |
        INSTALLS_TOOLBAR_TOOL     |
//        WANTS_CONFIG              |
        INSTALLS_TOOLBOX_PAGE     |
        INSTALLS_CONTEXTMENU_ITEMS  |
        WANTS_NMEA_EVENTS         |
        WANTS_NMEA_SENTENCES        |
        //    USES_AUI_MANAGER            |
//        WANTS_PREFERENCES         |
        WANTS_ONPAINT_VIEWPORT      |
        WANTS_PLUGIN_MESSAGING    |
        WANTS_LATE_INIT           |
        WANTS_MOUSE_EVENTS        |
        WANTS_KEYBOARD_EVENTS
    );
}

void openobserver_pi::LateInit(void)
{
    SendPluginMessage(wxS("OPENOBSERVER_PI_READY_FOR_REQUESTS"), wxS("TRUE"));
    m_bReadyForRequests = true;
    return;
}

bool openobserver_pi::DeInit(void)
{
    if (m_ooControlDialogImpl)
    {
        m_dialogPosition = m_ooControlDialogImpl->GetRect();
        
        m_ooControlDialogImpl->Close();
        delete m_ooControlDialogImpl;
        m_ooControlDialogImpl = nullptr;
    }
    if (m_ooMiniDialogImpl)
    {
        m_miniDialogPosition = m_ooMiniDialogImpl->GetRect();

        m_ooMiniDialogImpl->Close();
        delete m_ooMiniDialogImpl;
        m_ooMiniDialogImpl = nullptr;
    }
    if (m_pConfig) SaveConfig();

    return true;
}

int openobserver_pi::GetAPIVersionMajor()
{
      return OCPN_API_VERSION_MAJOR;
}

int openobserver_pi::GetAPIVersionMinor()
{
      return OCPN_API_VERSION_MINOR;
}

int openobserver_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int openobserver_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

int openobserver_pi::GetPlugInVersionPatch()
{
    return PLUGIN_VERSION_PATCH;
}

int openobserver_pi::GetPlugInVersionPost()
{
    return PLUGIN_VERSION_TWEAK;
}

wxString openobserver_pi::GetCommonName()
{
    return _T(PLUGIN_COMMON_NAME);
}

wxString openobserver_pi::GetShortDescription()
{
    return _(PLUGIN_SHORT_DESCRIPTION);
}

wxString openobserver_pi::GetLongDescription()
{
    return _(PLUGIN_LONG_DESCRIPTION);
}

int openobserver_pi::GetToolbarToolCount(void)
{
      return 1;
}

void openobserver_pi::OnToolbarToolCallback(int id)
{
    m_iCallerId = id;
    ToggleToolbarIcon();
}

void openobserver_pi::OnToolbarToolDownCallback(int id)
{
    return;
}

void openobserver_pi::OnToolbarToolUpCallback(int id)
{
    m_ptpicons->SetScaleFactor();
    return;
}

void openobserver_pi::OnContextMenuItemCallback(int id)
{
    if (id != m_addObservationItem) return;
    if (m_ooObservations == nullptr) return;

    if (m_ooObservations->IsObserving()) {
        wxMessageBox("Could not add observation ! An observation is already running.", "Error",
                   wxOK, wxGetActiveWindow());
        return;
    }

    m_ooObservations->AddObservation(m_cursor_lat, m_cursor_lon);
    m_ooObservations->AddMarks(0);
}

bool openobserver_pi::KeyboardEventHook( wxKeyEvent &event )
{
    bool bret = false;

    if( event.GetKeyCode() < 128 ) // ASCII
    {
        int key_char = event.GetKeyCode();

        if ( event.ControlDown() )
            key_char -= 64;

        switch( key_char ) {
            case WXK_CONTROL_W: // Ctrl W
                if ( event.ShiftDown() ) { // Shift-Ctrl-W
                    if(event.GetEventType() == wxEVT_KEY_DOWN) {
                        OnToolbarToolDownCallback( m_openobserver_button_id);
                    }
                    bret = true;
                } else bret = false;
                break;
        }
    }
    if(bret) RequestRefresh(m_parent_window);
    return bret;
}

bool openobserver_pi::MouseEventHook( wxMouseEvent &event )
{
    if (event.LeftDown()) {
        m_click_lat = m_cursor_lat;
        m_click_lon = m_cursor_lon;
    }

    return false;
}

void openobserver_pi::SetCursorLatLon(double lat, double lon)
{
    m_cursor_lat = lat;
    m_cursor_lon = lon;
}

void openobserver_pi::SetPositionFix(PlugIn_Position_Fix &pfix)
{
    if (m_ooObservations)
        m_ooObservations->SetPositionFix(pfix.FixTime, pfix.Lat, pfix.Lon);
    if (m_ooControlDialogImpl)
        m_ooControlDialogImpl->SetPositionFix(pfix.FixTime, pfix.Lat, pfix.Lon);
}

void openobserver_pi::SetNMEASentence(wxString& sentence)
{
    if (m_ooObservations)
        m_ooObservations->SetNmeaSentFix(sentence);
    if (m_ooControlDialogImpl)
        m_ooControlDialogImpl->SetNmeaSentence(sentence);
}

void openobserver_pi::SetCurrentViewPort(PlugIn_ViewPort& vp)
{
    if (m_ooControlDialogImpl)
        m_ooControlDialogImpl->SetViewScale(vp.view_scale_ppm);
}

wxBitmap *openobserver_pi::GetPlugInBitmap()
{
    return &m_ptpicons->m_bm_openobserver_pi;
}

void openobserver_pi::SetProject(const wxString& projectName, const wxColor& projectColor, int observationsIndex)
{
    m_observationsIndex = observationsIndex;
    
    wxString title = "Open Observer - " + projectName;
    m_ooControlDialogImpl->SetTitle(title);
    m_ooMiniDialogImpl->SetTitle(title);
    m_ooMiniDialogImpl->SetProjectInfo(projectName, projectColor);
}

void openobserver_pi::ToggleToolbarIcon()
{
    if (!m_ooControlDialogImpl || !m_ooMiniDialogImpl) return;

    if (m_ooControlDialogImpl->IsShown() || m_ooMiniDialogImpl->IsShown()) {
        SetToolbarItemState(m_openobserver_button_id, false);
        m_ooControlDialogImpl->Hide();
        m_ooMiniDialogImpl->Hide();
    } else {
        SetToolbarItemState(m_openobserver_button_id, true);
        if (m_bShowMainDialog) {
            m_ooMiniDialogImpl->Hide();
            m_ooControlDialogImpl->Show();
        } else {
            m_ooControlDialogImpl->Hide();
            m_ooMiniDialogImpl->Show();
        }
    }
}

void openobserver_pi::ToggleWindow()
{
    if (!m_ooControlDialogImpl || !m_ooMiniDialogImpl) return;

    if (m_ooControlDialogImpl->IsShown()) {
        m_ooControlDialogImpl->Hide();
        m_ooMiniDialogImpl->Show();
        m_bShowMainDialog = false;
    } else {
        m_ooMiniDialogImpl->Hide();
        m_ooControlDialogImpl->Show();
        m_bShowMainDialog = true;
    }
}

void openobserver_pi::SaveConfig()
{
    #ifndef __WXMSW__
    wxSetlocale(LC_NUMERIC, NULL);
    #if wxCHECK_VERSION(3,0,0)  && !defined(_WXMSW_)
    //#if wxCHECK_VERSION(3,0,0)
    wxSetlocale(LC_NUMERIC, "C");
    #else
    setlocale(LC_NUMERIC, "C");
    #endif
    #endif

    if (!m_pConfig) return;

    // section in the main OpenCPN setting file (Mac ~/Library/preferences/opencpn/opencpn.ini)
    m_pConfig->SetPath("/Settings/openobserver_pi");
    m_pConfig->DeleteEntry("ProjectFile");
    m_pConfig->Write("ObservationsIndex", m_observationsIndex);
    m_pConfig->Write("ObservationsChoiceCount", m_observationsChoiceCount);
    m_pConfig->Write("DialogX", m_dialogPosition.x);
    m_pConfig->Write("DialogY", m_dialogPosition.y);
    m_pConfig->Write("DialogWidth", m_dialogPosition.width);
    m_pConfig->Write("DialogHeight", m_dialogPosition.height);
    m_pConfig->Write("MiniDialogX", m_miniDialogPosition.x);
    m_pConfig->Write("MiniDialogY", m_miniDialogPosition.y);
    m_pConfig->Write("MiniDialogWidth", m_miniDialogPosition.width);
    m_pConfig->Write("MiniDialogHeight", m_miniDialogPosition.height);
}

void openobserver_pi::LoadConfig()
{
    #ifndef __WXMSW__
    wxSetlocale(LC_NUMERIC, NULL);
    #if wxCHECK_VERSION(3,0,0)
    wxSetlocale(LC_NUMERIC, "C");
    #else
    setlocale(LC_NUMERIC, "C");
    #endif
    #endif

    if (!m_pConfig) return;

    m_pConfig->SetPath("/Settings/openobserver_pi");
    m_pConfig->Read("ObservationsIndex", &m_observationsIndex, -1);
    m_pConfig->Read("ObservationsChoiceCount", &m_observationsChoiceCount, 3);
    m_pConfig->Read("DialogX", &m_dialogPosition.x, -1);
    m_pConfig->Read("DialogY", &m_dialogPosition.y, -1);
    m_pConfig->Read("DialogWidth", &m_dialogPosition.width, -1);
    m_pConfig->Read("DialogHeight", &m_dialogPosition.height, -1);
    m_pConfig->Read("MiniDialogX", &m_miniDialogPosition.x, -1);
    m_pConfig->Read("MiniDialogY", &m_miniDialogPosition.y, -1);
    m_pConfig->Read("MiniDialogWidth", &m_miniDialogPosition.width, -1);
    m_pConfig->Read("MiniDialogHeight", &m_miniDialogPosition.height, -1);
}

void openobserver_pi::WriteNmeaXML(const std::unordered_map<wxString, std::set<int>>& scannedNmeaFields)
{
    const wxString filePath =
      wxFileName(*g_PrivateDataDir, "NMEAFields.xml").GetFullPath();
    
    wxXmlDocument xmlDoc;
    
    wxXmlNode* root = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, "nmea_export");
    root->AddAttribute("creator", "Open Observer for OpenCPN");
    xmlDoc.SetRoot(root);

    // Sentence IDs
    for (const auto& sentencePair : scannedNmeaFields) {
        wxString sentenceId = sentencePair.first;
        if (sentenceId.IsEmpty()) continue;
        
        wxXmlNode* sentenceNode = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, "sentence");
        sentenceNode->AddAttribute("id", sentenceId);

        // Field indexes
        for (int index : sentencePair.second) {
            wxXmlNode* fieldNode = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, "field");
            fieldNode->AddAttribute("index", wxString::Format("%d", index));
            
            wxXmlNode* textNode =new wxXmlNode(
                wxXML_TEXT_NODE,
                "",
                wxString::Format(wxT("%s:%i"), sentenceId, index)
            );
            
            fieldNode->AddChild(textNode);
            sentenceNode->AddChild(fieldNode);
        }
        
        root->AddChild(sentenceNode);
    }
    
    xmlDoc.Save(filePath);
}

// Read NMEA XML file and return list of NMEA Fields
std::vector<NMEAField> openobserver_pi::ReadNmeaXML()
{
    const wxString filePath =
      wxFileName(*g_PrivateDataDir, "NMEAFields.xml").GetFullPath();

    std::vector<NMEAField> res;

    wxXmlDocument xmlDoc;
    if (!xmlDoc.Load(filePath)) {
        wxLogError("Cannont read NMEA XML : %s", filePath);
        return res;
    }

    wxXmlNode* root = xmlDoc.GetRoot();
    if (!root || root->GetName() != "nmea_export") {
        wxLogError("Invalid NMEA XML file : %s", filePath);
        return res;
    }

    for (wxXmlNode* sentenceNode = root->GetChildren(); sentenceNode != NULL; sentenceNode = sentenceNode->GetNext()) {
        if (sentenceNode->GetName() != "sentence") continue;

        wxString sentenceId = sentenceNode->GetAttribute("id", "");
        if (sentenceId.IsEmpty()) continue;

        for (wxXmlNode* fieldNode = sentenceNode->GetChildren(); fieldNode != NULL; fieldNode = fieldNode->GetNext()) {
            if (fieldNode->GetName() != "field") continue;

            wxString indexStr = fieldNode->GetAttribute("index", "");
            long fieldIndex = 0;
            if (!indexStr.ToLong(&fieldIndex)) continue;

            wxString description;
            if (fieldNode->GetChildren()) {
                description = fieldNode->GetChildren()->GetContent();
            }
            if (description.IsEmpty()) description = wxString::Format(wxT("%s:%i"), sentenceId, fieldIndex);

            res.push_back({sentenceId, (int)fieldIndex, description, ""});
        }
    }

    return res;
}

void openobserver_pi::RefreshListings()
{
    ooObservations::ClearListings();
    wxArrayString allListings;
    wxDir::GetAllFiles(*g_pListingDir, &allListings);
    for (auto f : allListings) {
        wxArrayString items, icons;
        if (ooObservations::ReadListingFromXML(f, items, icons)) {
            wxString filename = wxFileName(f).GetName();
            ooObservations::AddListing(filename, items);
            if (icons.GetCount() > 0) ooObservations::AddIcons(filename, icons);
        }
    }
}
