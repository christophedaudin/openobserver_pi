/***************************************************************************
* Project:  OpenCPN
* Purpose:  Open Observer Plugin Observations
* Author:   Alex Mansfield
*
* Project:  OpenCPN
*
***************************************************************************
*   Copyright (C) 2013 by David S. Register                               *
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
**************************************************************************/

#pragma once

#include <wx/file.h>
#include <wx/grid.h>
#include <wx/stopwatch.h>
#include <unordered_map>

///////////////////////////////////////////////////////////////////////////////
/// Class ooObservations
///////////////////////////////////////////////////////////////////////////////
class ooObservations : public wxGridStringTable
{
public:
    ooObservations();
    ~ooObservations();

    wxGridSizesInfo GetColSizes() const;
    void SetColSizes(const wxGridSizesInfo &sizeInfo);

    const wxArrayString& GetColFieldTypes() const;
    void SetColFieldTypes(const wxArrayString &colFieldTypes);

    void SetPositionFix(time_t fixTime, double lat, double lon);

    void StartObservation();
    void StopObservation();

    bool IsObserving() const;

    long GetObservationDuration();

    void AddMarks();
    void DeleteMarks();

    void SaveToCSV(wxFile *file);
    void SaveToXML(wxFile *file);
    bool ReadFromXML(wxString& filename);

    static wxArrayString GetObservationFieldTypes();
    static void AddListing(const wxString& listing, const wxArrayString& items);
    static bool GetListing(const wxString& listing, wxArrayString* items);
    static bool ReadListingFromXML(const wxString& filename, wxArrayString* result);


private:
    wxGridSizesInfo m_col_sizes;
    wxArrayString m_col_field_types;
    static std::unordered_map<wxString, wxArrayString> m_listings;

    time_t m_position_fix_time;
    double m_position_fix_lat;
    double m_position_fix_lon;

    bool m_IsObserving;
    wxStopWatch m_ObservationDurationStopWatch;
};
