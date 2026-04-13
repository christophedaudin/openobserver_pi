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
#include <wx/xml/xml.h>
#include <unordered_map>
#include <vector>

#define XML_FILE_VERSION_PROJECT      1
#define XML_FILE_VERSION_OBSERVATIONS 2
#define XML_FILE_VERSION_LISTING      1

#define DEFAULT_PROJECT_ICON          "Info-Fish-Whale"
#define DEFAULT_PROJECT_COLOUR        wxColor(50,50,50)

typedef struct _NMEAField {
    wxString m_sentenceId;
    int m_fieldIndex;
    wxString m_description;
    wxString m_value;
} NMEAField;

class ooProject {
public:
  ooProject()
      : m_name("Empty"),
        m_color(DEFAULT_PROJECT_COLOUR),
        m_mark_icon(DEFAULT_PROJECT_ICON) {}
  ooProject(const wxString& name, const wxGridSizesInfo& colSizes,
            const wxArrayString& colFieldTypes, const wxArrayString& colLabels, const wxColor& color, const wxString& markIcon)
      : m_name(name),
        m_col_sizes(colSizes),
        m_col_field_types(colFieldTypes),
        m_col_labels(colLabels),
        m_color(color),
        m_mark_icon(markIcon)
  {
    OnFieldTypeChanged();
  }

  bool ReadFromXML(const wxXmlNode* node);
  wxXmlNode* SaveToXML(wxXmlNode* parent = NULL);

  int GetColCount() const { return m_col_labels.GetCount(); }

  const wxString& GetName() const { return m_name; }
  void SetName(const wxString& name) { m_name = name; }

  const wxGridSizesInfo& GetColSizes() const { return m_col_sizes; }
  void SetColSizes(const wxGridSizesInfo& sizeInfo) { m_col_sizes = sizeInfo; }

  const wxArrayString& GetColFieldTypes() const { return m_col_field_types; }
  void SetColFieldTypes(const wxArrayString& colFieldTypes) {
    m_col_field_types = colFieldTypes;
    OnFieldTypeChanged();
  }

  const wxArrayString& GetColLabels() const { return m_col_labels; }
  void SetColLabels(const wxArrayString& colLabels) { m_col_labels = colLabels; }

  const wxColour& GetColor() const { return m_color; }
  const wxString& GetMarkIcon() const { return m_mark_icon; }

  bool IsUpdatable(const ooProject& other) const;

  int FindFieldTypeColumn(const wxString& field_type) const;
  int GetLatCol() const { return m_lat_col; }
  int GetLonCol() const { return m_lon_col; }
  int GetMarkCol() const { return m_mark_col; }

protected:
  void OnFieldTypeChanged();
  wxString m_name;
  wxGridSizesInfo m_col_sizes;
  wxArrayString m_col_field_types;
  wxArrayString m_col_labels;
  wxColor m_color;
  wxString m_mark_icon;
  int m_lat_col, m_lon_col, m_mark_col;
};

///////////////////////////////////////////////////////////////////////////////
/// Class ooObservations
///////////////////////////////////////////////////////////////////////////////
class ooObservations : public wxGridStringTable
{
public:
    ooObservations();
    ~ooObservations();

    void SetColSizes(const wxGridSizesInfo& colSize);
    const wxGridSizesInfo& GetColSizes() const;
    const wxArrayString& GetColFieldTypes() const;

    void SetPositionFix(time_t fixTime, double lat, double lon);
    void SetNmeaSentFix(wxString sentenceNmea);

    void StartObservation();
    void StopObservation();
    void AddObservation(double lat, double lon);

    bool IsObserving() const;

    long GetObservationDuration();

    void AddMarks(int row = -1); // -1 for all rows
    void DeleteMarks(int row = -1); // -1 for all rows
    int UpdateObservationsFromMarks();

    void SetProject(const ooProject& project);
    const ooProject& GetProject() const;

    void SaveToCSV(wxFile *file);
    bool ReadFromCSV(const wxString& filename, wxString& err);
    void SaveToXML(wxFile *file);
    bool ReadFromXML(const wxString& filename, const ooProject& defaultProject);
    bool SaveToGeoJSON(wxOutputStream& out);

    wxString GetRowDescription(int row);

    enum {UTC_TIME_DATE, UTC_TIME_TIME, UTC_TIMESTAMP};
    wxString GetUtcTimeFromNMEA(int dateFormat) const;

    double HaversineDistance(double lat1, double lon1, double lat2,double lon2);
    double DegToRad(double deg);
    static wxArrayString GetObservationFieldTypes();
    static void AddListing(const wxString& listing, const wxArrayString& items);
    static bool GetListing(const wxString& listing, wxArrayString& items);
    static const std::unordered_map<wxString, wxArrayString>& GetListings();
    static wxArrayString GetListingKeys();
    static void ClearListings();
    static bool ReadListingFromXML(const wxString& filename,
                                   wxArrayString& result,
                                   wxArrayString& icons);
    static bool ReadFromXML(const wxString& filename, int& fileVersion,
                            ooProject& project,
                            wxXmlDocument& xmlDoc, wxXmlNode*& root,
                            const ooProject& defaultProject);
    static void AddIcons(const wxString& listing, const wxArrayString& icons);
    static void SetNMEAFields(const std::vector<NMEAField>& fields);
    static const std::vector<NMEAField>& GetNMEAFields();

    // TODO: those should not be static, should not be in this class
    static wxString GetNMEAField(const wxString& sentence, const wxString& sentenceID,
                                 int fieldIndex);
    static std::vector<wxString> SplitNMEAFields(const wxString& sentence, wxChar sep = ',');

private:
    ooProject m_project;
    static std::unordered_map<wxString, wxArrayString> m_listings;
    static std::unordered_map<wxString, wxArrayString> m_listingsIcons;
    static std::vector<NMEAField> m_nmeaFields;

    time_t m_position_fix_time;
    double m_position_fix_lat;
    double m_position_fix_lon;

    wxString m_sentenceNMEA;
    double m_apparentWindSpeed = 0;
    double m_apparentWindAngle = 0;
    double m_trueWindSpeed = 0;
    double m_trueWindDirection = 0;

    double m_SOG = 0;
    double m_COG = 0;
    wxDateTime m_utcTime;

    double StartLongSave;
    double StartLatSave;

    bool m_IsObserving;
    wxStopWatch m_ObservationDurationStopWatch;
};
