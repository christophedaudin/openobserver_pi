/**************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Open Observer Plugin Observations
 * Author:   Alex Mansfield
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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

#include "../opencpn-libs/nmea0183/src/nmea0183.h"

#include "ooObservations.h"

#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/xml/xml.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/msgdlg.h>

#include "tpUtils.h"
#include "ocpn_plugin.h"
#include <openobserver_pi.h>

extern openobserver_pi* g_openobserver_pi;

void ComputeTrueWind(double sog, double cog, double apparentWindSpeed,
                     double apparentWindAngle, double& trueWindSpeed,
                     double& trueWindDirection);

    std::vector<NMEAField> ooObservations::m_nmeaFields;

bool ooProject::ReadFromXML(const wxXmlNode* project)
{
    if (project == NULL ||
        project->GetName() != "project") {
        return false;
    }

    m_col_labels.clear();
    m_col_field_types.clear();
    m_col_sizes = wxGridSizesInfo();
    m_color = DEFAULT_PROJECT_COLOUR;

    m_name = project->GetAttribute("name");
    wxArrayString colorStr = wxSplit(project->GetAttribute("color"), ',');
    int r, g, b;
    if (colorStr.GetCount() == 3 &&
        colorStr[0].ToInt(&r) &&
        colorStr[1].ToInt(&g) &&
        colorStr[2].ToInt(&b)) {
        m_color = wxColor(r, g, b);
    }
    m_mark_icon = project->GetAttribute("mark_icon", DEFAULT_PROJECT_ICON);

    for (wxXmlNode* field = project->GetChildren(); field != NULL ; field = field->GetNext()) {
        int c = -1;
        if (!field->GetAttribute("id").ToInt(&c) || c < 0) continue;
        m_col_labels.SetCount(c + 1);
        m_col_field_types.SetCount(c + 1);

        int col_size = -1;
        if (field->GetAttribute("col_size").ToInt(&col_size) && col_size >= 0) {
          m_col_sizes.m_customSizes[c] = col_size;
        }
        m_col_labels[c] = field->GetAttribute("label");
        m_col_field_types[c] = field->GetAttribute("field_type");

        if (m_col_labels[c].IsEmpty()) {
            m_col_labels[c] = wxString("Data");
        }
        if (m_col_field_types[c].IsEmpty()) {
            m_col_field_types[c] = wxString("Text");
        }
    }

    OnFieldTypeChanged();

    return true;
}

wxXmlNode* ooProject::SaveToXML(wxXmlNode* parent)
{
    const int C = m_col_field_types.size();

    wxXmlNode* project = new wxXmlNode(parent, wxXML_ELEMENT_NODE, "project");
    project->AddAttribute("name", m_name);
    project->AddAttribute(
        "color",
        wxString::Format("%i,%i,%i", (int)m_color.Red(), (int)m_color.Green(), (int)m_color.Blue())
    );
    project->AddAttribute("mark_icon", m_mark_icon);
    
    for (int c = 0; c < C; ++c) {
        wxXmlNode* field = new wxXmlNode(project, wxXML_ELEMENT_NODE, "field");
        field->AddAttribute("id", wxString::Format(wxT("%i"), c));
        field->AddAttribute("label", m_col_labels[c]);
        field->AddAttribute("field_type", m_col_field_types[c]);
        field->AddAttribute(
            "col_size", wxString::Format(wxT("%i"), m_col_sizes.GetSize(c)));
    }

    return project;
}

bool IsTextOrListing(const wxString& fieldType, const wxArrayString& listings)
{
    if (fieldType.IsSameAs("Text"))               return true;
    if (listings.Index(fieldType) != wxNOT_FOUND) return true;
    return false;
}

bool ooProject::IsUpdatable(const ooProject& other) const
{
    bool res = (this->GetColCount() == other.GetColCount());
    int c = 0;
    const int C = GetColCount();
    const wxArrayString listings = ooObservations::GetListingKeys();
    while (res && c < C) {
        const bool otherIsTextOrListing =
            IsTextOrListing(other.GetColFieldTypes()[c], listings);
        const bool thisIsTextOrListing =
            IsTextOrListing(this->GetColFieldTypes()[c], listings);
    
        if (otherIsTextOrListing || thisIsTextOrListing) {
            res = (otherIsTextOrListing && thisIsTextOrListing);
        } else {
            res = (other.GetColFieldTypes()[c].IsSameAs(
                   this->GetColFieldTypes()[c]));
        }
        c++;
    }
    return res;
}

int ooProject::FindFieldTypeColumn(const wxString& field_type) const
{
    const int C = m_col_field_types.size();
    return m_col_field_types.Index(field_type);
}

void ooProject::OnFieldTypeChanged()
{
    m_lat_col = FindFieldTypeColumn("Start Latitude");
    m_lon_col = FindFieldTypeColumn("Start Longitude");
    m_mark_col = FindFieldTypeColumn("Mark GUID");
}

std::unordered_map<wxString, wxArrayString> ooObservations::m_listings;
std::unordered_map<wxString, wxArrayString> ooObservations::m_listingsIcons;

ooObservations::ooObservations() : wxGridStringTable(0, 0), m_IsObserving(false)
{
}

ooObservations::~ooObservations()
{
}

const wxGridSizesInfo& ooObservations::GetColSizes() const
{
    return m_project.GetColSizes();
}

void ooObservations::SetColSizes(const wxGridSizesInfo& colSize)
{
    m_project.SetColSizes(colSize);
}

const wxArrayString& ooObservations::GetColFieldTypes() const
{
    return m_project.GetColFieldTypes();
}

void ooObservations::SetPositionFix(time_t fixTime, double lat, double lon)
{
    m_position_fix_time = fixTime;
    m_position_fix_lat = lat;
    m_position_fix_lon = lon;
}

void ooObservations::SetNmeaSentFix(wxString sentenceNmea)
{
    m_sentenceNMEA = sentenceNmea;
    NMEA0183 nmea;
    nmea << sentenceNmea;

    if (nmea.Parse())
    {
        if (nmea.LastSentenceIDParsed == "MWV") {
            m_apparentWindSpeed = nmea.Mwv.WindSpeed;
            m_apparentWindAngle = nmea.Mwv.WindAngle;
            if (m_SOG != 0 && m_COG != 0) {
                ComputeTrueWind(m_SOG, m_COG, m_apparentWindSpeed, m_apparentWindAngle, m_trueWindSpeed,
                                m_trueWindDirection);
            }
        }
        else if (nmea.LastSentenceIDParsed == "RMC") {
            if (nmea.Rmc.IsDataValid == NTrue) {
                wxString dt = nmea.Rmc.Date + nmea.Rmc.UTCTime;
                m_utcTime.ParseFormat(dt.c_str(), _T("%d%m%y%H%M%S"));
            }
            if (nmea.Rmc.TrackMadeGoodDegreesTrue >= 0 && nmea.Rmc.TrackMadeGoodDegreesTrue <= 360)
            {
                m_COG = nmea.Rmc.TrackMadeGoodDegreesTrue;
            }
             m_SOG = nmea.Rmc.SpeedOverGroundKnots;
             
             //wxLogMessage("SOG: %.2f kn  COG: %.2f°", m_SOG, m_COG);

             wxString sog = GetNMEAField(m_sentenceNMEA, "RMC", 7);  // SOG
             wxString cog = GetNMEAField(m_sentenceNMEA, "RMC", 8);  // COG
             //wxLogMessage("SOG=%s COG=%s", sog, cog);
        }
        else if (nmea.LastSentenceIDReceived == _T("ZDA")) {
            wxString dt;
            dt.Printf(_T("%4d%02d%02d"), nmea.Zda.Year, nmea.Zda.Month,
                      nmea.Zda.Day);
            dt.Append(nmea.Zda.UTCTime);
            m_utcTime.ParseFormat(dt.c_str(), _T("%d%m%y%H%M%S"));
        }
        for (auto& item : ooObservations::m_nmeaFields) {
            if (nmea.LastSentenceIDParsed.IsSameAs(item.m_sentenceId)) {
                item.m_value = GetNMEAField(m_sentenceNMEA,
                                            item.m_sentenceId,
                                            item.m_fieldIndex);
            }
        }


    }
}

// Computes TrueWindSpeed and TrueWindAngle using the Apparent Wind and the GPS SOG.
static void ComputeTrueWind(double sog, double cog,
                                     double apparentWindSpeed,
                                     double apparentWindAngle,
                                     double& trueWindSpeed,
                                     double& trueWindDirection)
{
    double awa_rad = apparentWindAngle * M_PI / 180.0;
    
    double Vx = apparentWindSpeed * cos(awa_rad);
    double Vy = apparentWindSpeed * sin(awa_rad);
    
    double Vtx = Vx - sog;
    double Vty = Vy;
    
    trueWindSpeed = sqrt(Vtx * Vtx + Vty * Vty);
    trueWindDirection = (int)(cog + atan2(Vty, Vtx) * 180.0 / M_PI) % 360;
    if (trueWindDirection < 0) trueWindDirection += 360.0;
}

std::vector<wxString> ooObservations::SplitNMEAFields(const wxString& sentence,
                                                      wxChar sep)
{
    std::vector<wxString> fields;
    size_t start = 0, end = 0;
    
    while ((end = sentence.find(sep, start)) != wxString::npos) {
        fields.push_back(sentence.Mid(start, end - start));
        start = end + 1;
    }
    fields.push_back(sentence.Mid(start));
    return fields;
}

// Get an NMEA field by ID and index
wxString ooObservations::GetNMEAField(const wxString& sentence,
                                      const wxString& sentenceID,
                                      int fieldIndex)
{
    if (sentence.IsEmpty()) return "";
    
    if (!sentence.Mid(3, sentenceID.Length()).IsSameAs(sentenceID)) return "";
    
    std::vector<wxString> fields = SplitNMEAFields(sentence);
    
    if (fieldIndex < 0 || fieldIndex >= (int)fields.size()) return "";
    
    return fields[fieldIndex];
}


////////////VPE NMEA//////////////

wxString ooObservations::GetUtcTimeFromNMEA(int dateFormat) const
{
  wxDateTime::Tm tm = m_utcTime.GetTm();
  wxString res;
  switch (dateFormat) {
    case UTC_TIME_DATE:
      res = wxString::Format("%04d/%02d/%02d", (int)tm.year,
                             (int)tm.mon + 1, (int)tm.mday);
      break;
    case UTC_TIME_TIME:
      res = wxString::Format("%02d:%02d:%02d",
                             (int)tm.hour, (int)tm.min, (int)tm.sec);
      break;
    case UTC_TIMESTAMP:
      res = wxString::Format("%04d-%02d-%02dT%02d:%02d:%02dZ", (int)tm.year,
                             (int)tm.mon + 1, (int)tm.mday, (int)tm.hour,
                             (int)tm.min, (int)tm.sec);
      break;
  }

  return res;
}

void ooObservations::StartObservation()
{
    if (m_IsObserving) return;

    // start duration stopwatch
    m_ObservationDurationStopWatch.Start(0);

    // get date and time
    //char dateString[16];
    //strftime(dateString, 16, "%F", gmtime(&m_position_fix_time));
    //char timeString[16];
    //strftime(timeString, 16, "%T", gmtime(&m_position_fix_time));
    //char utcTimestampString[64];
    //strftime(utcTimestampString, 64, "%FT%TZ", gmtime(&m_position_fix_time));
    wxString dateString         = GetUtcTimeFromNMEA(UTC_TIME_DATE);
    wxString timeString         = GetUtcTimeFromNMEA(UTC_TIME_TIME);
    wxString utcTimestampString = GetUtcTimeFromNMEA(UTC_TIMESTAMP);
    
    // create new observation in table and fill in fields
    InsertRows(0, 1);
    const int C = GetNumberCols();
    if (m_project.GetColCount() == C)
    {
        for (int c=0; c<C; ++c)
        {
            wxString field_type = m_project.GetColFieldTypes()[c];
            if (field_type.IsSameAs("Start Date"))
                SetValue(0, c, dateString);
            else if (field_type.IsSameAs("Start Time"))
                SetValue(0, c, timeString);
            else if (field_type.IsSameAs("Start Timestamp UTC"))
                SetValue(0, c, utcTimestampString);
            else if (field_type.IsSameAs("Start Latitude"))
              {
               
                SetValue(0, c, toSDMM_PlugIn(1, m_position_fix_lat));
                StartLatSave = m_position_fix_lat;
              }
            else if (field_type.IsSameAs("Start Longitude"))
              {
                 SetValue(0, c, toSDMM_PlugIn(2, m_position_fix_lon));
                 StartLongSave = m_position_fix_lon;
              }
            else if (field_type.IsSameAs("NMEA AWS"))
                SetValue(0, c, wxString::Format("%d", (int)round(m_apparentWindSpeed)));
            else if (field_type.IsSameAs("NMEA AWA"))
                SetValue(0, c, wxString::Format("%d", (int)round(m_apparentWindAngle)));
            else if (field_type.IsSameAs("NMEA TWS"))
                SetValue(0, c, wxString::Format("%d", (int)round(m_trueWindSpeed)));
            else if (field_type.IsSameAs("NMEA TWD"))
                SetValue(0, c, wxString::Format("%d", (int)round(m_trueWindDirection)));
            else if (field_type.IsSameAs("NMEA COG"))
                SetValue(0, c, wxString::Format("%d", (int)round(m_COG)));
            else if (field_type.IsSameAs("NMEA SOG"))
                SetValue(0, c, wxString::Format("%.1f", m_SOG));
            else if (field_type.IsSameAs("Distance"))
              SetValue(0, c, "...");
            else {
                for (const auto& item : ooObservations::m_nmeaFields) {
                    if (field_type.IsSameAs(item.m_description)) {
                        SetValue(0, c, item.m_value);
                        break;
                    }
                }
            }
        }
    } else {
        wxLogError("m_col_field_types.GetCount() does not match number of observation columns");
    }

    m_IsObserving = true;
}

void ooObservations::StopObservation()
{
    if (!m_IsObserving) return;

    m_ObservationDurationStopWatch.Pause();

    // get date and time
    // char dateString[16];
    // strftime(dateString, 16, "%F", gmtime(&m_position_fix_time));
    // char timeString[16];
    // strftime(timeString, 16, "%T", gmtime(&m_position_fix_time));
    // char utcTimestampString[64];
    // strftime(utcTimestampString, 64, "%FT%TZ", gmtime(&m_position_fix_time));
    wxString dateString = GetUtcTimeFromNMEA(UTC_TIME_DATE);
    wxString timeString = GetUtcTimeFromNMEA(UTC_TIME_TIME);
    wxString utcTimestampString = GetUtcTimeFromNMEA(UTC_TIMESTAMP);
    
    // get duration
    const long duration_ms = GetObservationDuration();
    const unsigned int hours = duration_ms / 3600000;
    const unsigned int minutes = (duration_ms % 3600000) / 60000;
    const unsigned int seconds = (duration_ms % 60000) / 1000;

    char durationString[16];
    sprintf(durationString, "%02u:%02u:%02u", hours, minutes, seconds);

    // fill in fields
    const int C = GetNumberCols();
    if (m_project.GetColCount() == C)
    {
        for (int c=0; c<C; ++c)
        {
            wxString field_type = m_project.GetColFieldTypes()[c];
            if (field_type.IsSameAs("End Date"))
                SetValue(0, c, dateString);
            else if (field_type.IsSameAs("End Time"))
                SetValue(0, c, timeString);
            else if (field_type.IsSameAs("End Timestamp UTC"))
                SetValue(0, c, utcTimestampString);
            else if (field_type.IsSameAs("End Latitude"))
                SetValue(0, c, toSDMM_PlugIn(1, m_position_fix_lat));
            else if (field_type.IsSameAs("End Longitude"))
                SetValue(0, c, toSDMM_PlugIn(2, m_position_fix_lon));
          else if (field_type.IsSameAs("Distance")) {
                double dist = HaversineDistance(StartLatSave, StartLongSave, m_position_fix_lat, m_position_fix_lon);
                SetValue(0, c,wxString::Format("%.3f", dist));
          }
            else if (field_type.IsSameAs("Observation Duration"))
                SetValue(0, c, durationString);
        }
    } else {
        wxLogError("m_col_field_types.GetCount() does not match number of observation columns");
    }

    m_IsObserving = false;
}

void ooObservations::AddObservation(double lat, double lon)
{
    if (m_IsObserving) return;
    
    double ex_position_fix_lon = m_position_fix_lon,
           ex_position_fix_lat = m_position_fix_lat;
    m_position_fix_lon = lon;
    m_position_fix_lat = lat;
    StartObservation();
    StopObservation();
    m_position_fix_lon = ex_position_fix_lon,
    m_position_fix_lat = ex_position_fix_lat;
}

const double R_EARTH_KM = 6371.0;  // rayon Terre en km
double ooObservations::DegToRad(double deg) { return deg * M_PI / 180.0; }
constexpr double KM_TO_NAUTICAL_MILES = 1.0 / 1.8520;

// Returns the distance in NM between two GPS positions
double ooObservations::HaversineDistance(double lat1, double lon1, double lat2,
                                         double lon2) {
  double dLat = DegToRad(lat2 - lat1);
  double dLon = DegToRad(lon2 - lon1);

  double radLat1 = DegToRad(lat1);
  double radLat2 = DegToRad(lat2);
  
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(radLat1) * cos(radLat2) * sin(dLon / 2) * sin(dLon / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  double distance = R_EARTH_KM * c * KM_TO_NAUTICAL_MILES;
  return distance;
}

bool ooObservations::IsObserving() const
{
    return m_IsObserving;
}

long ooObservations::GetObservationDuration()
{
    if (IsObserving())
        return m_ObservationDurationStopWatch.Time();
    else
        return 0;
}

wxString ooObservations::GetRowDescription(int row)
{
    wxString res;
    const int C = m_project.GetColCount();

    for (int c = 0; c < C; ++c) {
        wxString field_type = m_project.GetColFieldTypes()[c];
        if (field_type.IsSameAs("Mark GUID") ||
            field_type.IsSameAs("Start Date") ||
            field_type.IsSameAs("Start Time") ||
            field_type.IsSameAs("Start Timestamp UTC") ||
            field_type.IsSameAs("End Date") ||
            field_type.IsSameAs("End Time") ||
            field_type.IsSameAs("End Timestamp UTC") ||
            field_type.IsSameAs("Start Latitude") ||
            field_type.IsSameAs("Start Longitude") ||
            field_type.IsSameAs("End Latitude") ||
            field_type.IsSameAs("End Longitude") ||
            field_type.IsSameAs("Observation Duration") ||
            field_type.IsSameAs("Distance"))
            continue;
        const wxString cell = GetValue(row, c);
        if (cell.Length() == 0) continue;
        res += wxString::Format(
            wxT("%s: %s\n"),
            m_project.GetColLabels()[c],
            cell
        );
    }

    return res;
}

void ooObservations::AddMarks(int targetRow)
{
    // Get column indices
    int markGUIDCol = -1;
    int dateCol = -1;
    int timeCol = -1;
    int tstpCol = -1;
    int latCol = -1;
    int lonCol = -1;
    int nameCol = -1;
    int descriptionCol = -1;
    int iconCol = -1;
    wxString iconFieldType;
    const int C = m_project.GetColCount();

    for (int c=0; c<C; ++c)
    {
        wxString field_type = m_project.GetColFieldTypes()[c];
        if (field_type.IsSameAs("Mark GUID")) {
            if (markGUIDCol < 0) markGUIDCol = c;
        } else if (field_type.IsSameAs("Start Date")) {
            if (dateCol < 0) dateCol = c;
        } else if (field_type.IsSameAs("Start Time")) {
            if (timeCol < 0) timeCol = c;
        } else if (field_type.IsSameAs("Start Timestamp UTC")) {
            if (tstpCol < 0) tstpCol = c;
        } else if (field_type.IsSameAs("Start Latitude")) {
            if (latCol < 0) latCol = c;
        } else if (field_type.IsSameAs("Start Longitude")) {
            if (lonCol < 0) lonCol = c;
        } else if (m_listingsIcons.find(field_type) != m_listingsIcons.end()) {
            if (iconCol < 0) iconCol = c;
            iconFieldType = field_type;
        } else if (field_type.IsSameAs("Text")) {
            // use first text column as name and second as description
            if (nameCol < 0) nameCol = c;
            else if (descriptionCol < 0) descriptionCol = c;
        }
    }

    if (markGUIDCol < 0) {
        wxLogError("Could not find a column for storing the Mark GUID");
        return;
    }

    if ((latCol < 0) || (lonCol < 0)) {
        wxLogError("Could not find columns storing the latitude and longitude, so unable to make marks");
        return;
    }

    const int R = GetNumberRows();
    for (int r=0; r<R; ++r)
    {
        if (targetRow != -1 && targetRow != r) continue;

        if (GetValue(r, markGUIDCol).IsEmpty())
        {
            const double lat = fromDMM_Plugin(GetValue(r, latCol));
            const double lon = fromDMM_Plugin(GetValue(r, lonCol));

            wxDateTime datetime;
            if (dateCol>=0) datetime.ParseISODate(GetValue(r, dateCol));
            if (timeCol>=0) datetime.ParseISOTime(GetValue(r, timeCol));
            if (tstpCol>=0) datetime.ParseISOCombined(GetValue(r, tstpCol));

            wxString description = GetRowDescription(r);
            wxString name = wxString::Format(wxT("%s (OO)"), datetime.FormatISOCombined());

            wxString guid = GetNewGUID();

            wxString icon;
            if (iconCol != -1) {
                wxString iconValue = GetValue(r, iconCol);
                int iconIndex = m_listings[iconFieldType].Index(iconValue);
                if (iconIndex != wxNOT_FOUND) icon = m_listingsIcons[iconFieldType][iconIndex];
            }
            if (icon.IsEmpty()) icon = m_project.GetMarkIcon();
            
            // add waypoint
            PlugIn_Waypoint wp(lat, lon, icon, name, guid);
            wp.m_MarkDescription = description;
            wp.m_CreateTime = datetime;
            AddSingleWaypoint(&wp);

            // store guid in table
            SetValue(r, markGUIDCol, guid);
        }
    }
}

void ooObservations::DeleteMarks(int targetRow)
{
    // Get column indices
    int markGUIDCol = -1;
    
    const int C = GetNumberCols();
    if (m_project.GetColCount() == C)
    {
        for (int c=0; c<C; ++c)
        {
            wxString field_type = m_project.GetColFieldTypes()[c];
            if (field_type.IsSameAs("Mark GUID")) {
                if (markGUIDCol < 0) markGUIDCol = c;
            }
        }
    } else {
        wxLogError("m_col_field_types.GetCount() does not match number of observation columns");
        return;
    }

    if (markGUIDCol < 0) {
        wxLogError("Could not find a column for storing the Mark GUID");
        return;
    }

    const int R = GetNumberRows();
    for (int r=0; r<R; ++r)
    {
        if (targetRow != -1 && targetRow != r) continue;

        wxString guid = GetValue(r, markGUIDCol);
        if (guid.IsEmpty()) continue;

        // delete waypoint
        DeleteSingleWaypoint(guid);

        // remove guid from table
        guid.Clear();
        SetValue(r, markGUIDCol, guid);
    }
}

int ooObservations::UpdateObservationsFromMarks()
{
    // !!! Work-around !!!
    // Since OpenCPN 5.14.0, calling GetSingleWaypoint() during plugin
    // initialization crashes so we'll skip this step if plugin initizalization is not done.
    // (which does not matter, because there are no usecases where this function has anything
    // to do during initialization)
    if (!g_openobserver_pi->IsLateInitDone()) return 0; 

    int res = 0;
    const int R = GetRowsCount();
    const int markCol = GetProject().GetMarkCol();
    const int latCol = GetProject().GetLatCol();
    const int lonCol = GetProject().GetLonCol();

    if (latCol == wxNOT_FOUND ||
        lonCol == wxNOT_FOUND ||
        markCol == wxNOT_FOUND)
      return 0;

    for (int r = R - 1; r >= 0; r--) {
        wxString guid = GetValue(r, markCol);
        if (guid.IsEmpty()) continue;
        const wxString latStr = GetValue(r, latCol);
        const wxString lonStr = GetValue(r, lonCol);
        
        const double lat = fromDMM_Plugin(latStr);
        const double lon = fromDMM_Plugin(lonStr);
        
        PlugIn_Waypoint wp;
        if (GetSingleWaypoint(guid, &wp)) {
            double aLat = wxRound(wp.m_lat * 1000);
            double bLat = wxRound(lat * 1000);
            
            double aLon = wxRound(wp.m_lon * 1000);
            double bLon = wxRound(lon * 1000);
            
            if (wxRound(wp.m_lat*1000) != wxRound(lat*1000) ||
                wxRound(wp.m_lon*1000) != wxRound(lon*1000)) {
                // Update needed
                SetValue(r, latCol, toSDMM_PlugIn(1, wp.m_lat));
                SetValue(r, lonCol, toSDMM_PlugIn(2, wp.m_lon));
                res++;
            }
        } else {
            // The waypoint has been deleted
            const int response = wxMessageBox(
              wxString::Format(wxT("Warning: mark of observation #%i has been deleted. Do you want "
              "to delete the corresponding observation?"), r + 1),
                "Delete observation?", wxYES | wxNO, GetOCPNCanvasWindow());
            if (response == wxYES) {
                res++;
                DeleteRows(r);
            } else {
                SetValue(r, markCol, wxEmptyString);
            }
        }
    }
    return res;
}

void ooObservations::SaveToCSV(wxFile *file)
{
    const int C = GetNumberCols();
    const int R = GetNumberRows();
    
    for (int c=0; c<C; ++c)
    {
        file->Write("\"" + GetColLabelValue(c) + "\"");

        if (c<(C - 1))
            file->Write(",");
    }
    file->Write("\n");

    for (int r=0; r<R; ++r)
    {
        for (int c=0; c<C; ++c)
        {
            file->Write("\"" + GetValue(r, c) + "\"");

            if (c<(C - 1))
                file->Write(",");
        }
        file->Write("\n");
    }

    file->Close();
}

static wxString FormatImportedCsvCell(const wxString& value)
{
    wxString res = value;
    if (value.Length() >= 2 &&
        value.GetChar(0) == '"' &&
        value.GetChar(value.Length() - 1) == '"') {
        int length = res.Length();
        res = res.Right(length - 1).Left(length - 2);
    }
    return res;
}

bool ooObservations::ReadFromCSV(const wxString& filename, wxString& err)
{
    wxFileInputStream file_input(filename);
    wxTextInputStream input(file_input);
    const int C = m_project.GetColCount();

    wxString line;
    int r = GetRowsCount();
    bool bSkip = true;
    while ((line = input.ReadLine()).Length() > 0)
    {
        if (bSkip) {
            bSkip = false;
            continue;
        }
        wxArrayString cells = wxSplit(line, ',');
        const int cellCount = cells.GetCount();
        if (cellCount != C)
        {
            err = wxString::Format(wxT("CSV contains %i columns, but the current project has %i"), cellCount, C);
            return false;
        }
        AppendRows(1);
        int c = 0;
        for (auto cell: cells)
        {
            SetValue(r, c, FormatImportedCsvCell(cell));
            c++;
        }
        r++;
    }

    return true;
}

void ooObservations::SaveToXML(wxFile *file)
{
    const int C = GetNumberCols();
    const int R = GetNumberRows();

    wxXmlDocument xmlDoc;
    wxXmlNode* observations = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, "observations");
    observations->AddAttribute("creator", "Open Observer for OpenCPN");
    observations->AddAttribute("file_version", wxString::FromDouble(XML_FILE_VERSION_OBSERVATIONS));
    xmlDoc.SetRoot(observations);

    // xml item 1: save project
    m_project.SaveToXML(observations);

    // xml item 2: save data
    wxXmlNode* data = new wxXmlNode(observations, wxXML_ELEMENT_NODE, "data");

    for (int r=0; r<R; ++r) {
        wxXmlNode* observation = new wxXmlNode(data, wxXML_ELEMENT_NODE, "observation");
        observation->AddAttribute("id", wxString::Format(wxT("%i"), r));

        for (int c=0; c<C; ++c) {
            wxXmlNode* field = new wxXmlNode(observation, wxXML_ELEMENT_NODE, "field");
            field->AddAttribute("id", wxString::Format(wxT("%i"), c));
            field->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "",  GetValue(r, c)));
        }
    }
    
    // write the output to a wxString
    wxStringOutputStream stream;
    xmlDoc.Save(stream);

    // write the string to the file
    file->Write(stream.GetString());    
}

bool ooObservations::ReadListingFromXML(const wxString& filename, wxArrayString& result, wxArrayString& icons)
{
    wxXmlDocument xmlDoc;
    int fileVersion = -1;
    if (filename.IsEmpty() || (!xmlDoc.Load(filename)) ||
        (xmlDoc.GetRoot()->GetName() != "listing") ||
        !xmlDoc.GetRoot()->GetAttribute("file_version").ToInt(&fileVersion) ||
         fileVersion > XML_FILE_VERSION_LISTING) {
        return false;
    }

    bool bHasIcon = false;
    wxXmlNode* item = xmlDoc.GetRoot()->GetChildren();
    while (item) {
        int r = -1;
        wxString label = item->GetAttribute("label");
        wxString icon = item->GetAttribute("icon");
        if (label.length() > 0) {
            result.Add(label);
            icons.Add(icon);
        }
        bHasIcon |= (icon.length() > 0);
        
        item = item->GetNext();
    }

    if (!bHasIcon) icons.clear();

    return true;
}

wxXmlNode* FindChild(const wxXmlNode* parent, const wxString& name)
{
    wxXmlNode * child = parent->GetChildren();
    while (child) {
        if (child->GetName() == name) return child;
        child = child->GetNext();
    }
    return NULL;
}

bool ooObservations::ReadFromXML(const wxString& filename, int& fileVersion,
                                 ooProject& project, wxXmlDocument& xmlDoc, wxXmlNode*& root,
                                 const ooProject& defaultProject) {
    if (filename.IsEmpty() || (!xmlDoc.Load(filename))) {
        return false;
    }
    root = xmlDoc.GetRoot();
    fileVersion = -1;

    wxXmlNode* xmlProject = NULL;
    if (root->GetName() == "observations") {
        if (!root->GetAttribute("file_version").ToInt(&fileVersion) ||
            fileVersion > XML_FILE_VERSION_OBSERVATIONS) {
            return false;
        }
        xmlProject = FindChild(root, "project");
    } else if (root->GetName() == "project") {
        if (!root->GetAttribute("file_version").ToInt(&fileVersion) ||
            fileVersion > XML_FILE_VERSION_PROJECT) {
            return false;
        }
        xmlProject = root;
    } else {
        return false;
    }

    if (!project.ReadFromXML(xmlProject)) project = defaultProject;

    return true;
}

bool ooObservations::ReadFromXML(const wxString& filename, const ooProject& defaultProject)
{
    int fileVersion = 0;
    wxXmlNode * root = nullptr;
    wxXmlDocument xmlDoc;
    ooProject project;
    if (!ooObservations::ReadFromXML(filename, fileVersion, project, xmlDoc, root,
                                     defaultProject))
        return false;

    // Make sure the table is cleared before loading any new data
    // (loading blank data would let the old data still present)
    DeleteRows(0, GetNumberRows());

    SetProject(project);

    const int C = GetNumberCols();
    wxXmlNode* data = (root->GetName() == "project" ? NULL // Loading old project file without data
                                                    : fileVersion == 1 ? root
                                                    : FindChild(root, "data"));
    wxXmlNode* observation = (data ? data->GetChildren() : NULL);
    while (observation)
    {
        int r = -1;
        if (observation->GetAttribute("id").ToInt(&r) && (r>=0)) {

            // expand number of rows as needed to accommodate observations
            if (r >= GetNumberRows()) {
                AppendRows(r - GetNumberRows() + 1);
            }

            wxXmlNode *field = observation->GetChildren();
            while (field) {
                if (field->GetChildren()) {
                    int c = -1;
                    if (field->GetAttribute("id").ToInt(&c) && (c>=0) && (c<C)) {
                        SetValue(r, c, field->GetChildren()->GetContent());
                    }
                }

                field = field->GetNext();
            }
        }

        observation = observation->GetNext();
    }

    return true;
}

#include <wx/jsonwriter.h>
bool ooObservations::SaveToGeoJSON(wxOutputStream& out)
{
    const int R = GetRowsCount();
    const int C = GetColsCount();
    const int latCol = m_project.GetLatCol();
    const int lonCol = m_project.GetLonCol();

    wxJSONValue root;
    root["type"] = wxString("FeatureCollection");
    
    wxJSONValue features;
    for (int r = 0; r < R; r++) {
        const double lat = fromDMM_Plugin(GetValue(r, latCol));
        const double lon = fromDMM_Plugin(GetValue(r, lonCol));
        const wxString des = GetRowDescription(r);

        features[r]["type"] = wxString("Feature");
        features[r]["geometry"]["type"] = wxString("Point");
        features[r]["geometry"]["coordinates"][0] = lon;
        features[r]["geometry"]["coordinates"][1] = lat;

        for (int c = 0; c < C; c++) {
            const wxString field_type = m_project.GetColFieldTypes()[c];
            if (field_type.IsSameAs("Mark GUID") ||
                field_type.IsSameAs("End Date") ||
                field_type.IsSameAs("End Time") ||
                field_type.IsSameAs("End Timestamp UTC") ||
                field_type.IsSameAs("Start Latitude") ||
                field_type.IsSameAs("Start Longitude") ||
                field_type.IsSameAs("End Latitude") ||
                field_type.IsSameAs("End Longitude") ||
                field_type.IsSameAs("Observation Duration") ||
                field_type.IsSameAs("Distance"))
                continue;
            const wxString field_label = m_project.GetColLabels()[c];

            features[r]["properties"][field_label] =
                  wxString(GetValue(r,c));
        }
    }
    
    root["features"] = features;

    wxJSONWriter().Write(root, out);

    return true;
}

void ooObservations::SetProject(const ooProject& project)
{
    bool bMustReset = !m_project.IsUpdatable(project);

    m_project = project;
    const int C = m_project.GetColCount();

    if (bMustReset) {
        // stop any observation, if one is running
        StopObservation();
        
        // delete table
        if (GetNumberRows() > 0)
            DeleteRows(0, GetNumberRows());
        
        if (GetNumberCols() > 0)
            DeleteCols(0, GetNumberCols());
        
        // add columns
        InsertCols(0, C);
    }

    // set the column labels
    for (int c = 0; c < C; ++c) {
        SetColLabelValue(c, m_project.GetColLabels()[c]);
    }
}

const ooProject& ooObservations::GetProject() const
{
    return m_project;
}

wxArrayString ooObservations::GetObservationFieldTypes()
{
    wxArrayString observationFieldTypes;
    observationFieldTypes.Add("Start Date");
    observationFieldTypes.Add("Start Time");
    observationFieldTypes.Add("Start Timestamp UTC");
    observationFieldTypes.Add("Start Latitude");
    observationFieldTypes.Add("Start Longitude");
    observationFieldTypes.Add("Distance");
    observationFieldTypes.Add("End Date");
    observationFieldTypes.Add("End Time");
    observationFieldTypes.Add("End Timestamp UTC");
    observationFieldTypes.Add("End Latitude");
    observationFieldTypes.Add("End Longitude");
    observationFieldTypes.Add("Observation Duration");
    observationFieldTypes.Add("Mark GUID");
    observationFieldTypes.Add("Text");
    observationFieldTypes.Add("NMEA AWS");
    observationFieldTypes.Add("NMEA AWA");
    observationFieldTypes.Add("NMEA TWS");
    observationFieldTypes.Add("NMEA TWD");
    observationFieldTypes.Add("NMEA COG");
    observationFieldTypes.Add("NMEA SOG");

    for (auto it : m_listings)
    {
      observationFieldTypes.Add(it.first);
    }
    for (auto it : ooObservations::m_nmeaFields) {
        observationFieldTypes.Add(it.m_description);
    }
    return observationFieldTypes;
}

void ooObservations::ClearListings()
{
    m_listings.clear();
    m_listingsIcons.clear();
}

void ooObservations::AddListing(const wxString& listing, const wxArrayString& items)
{
    m_listings[listing] = items;
}

bool ooObservations::GetListing(const wxString& listing, wxArrayString& items)
{
    if (m_listings.find(listing) == m_listings.end()) return false;

    items = m_listings.at(listing);
    return true;
}

const std::unordered_map<wxString, wxArrayString>& ooObservations::GetListings()
{
    return m_listings;
}

wxArrayString ooObservations::GetListingKeys()
{
    wxArrayString res;
    for (auto it : m_listings) {
        res.Add(it.first);
    }
    return res;
}

void ooObservations::AddIcons(const wxString& listing, const wxArrayString& icons)
{
    m_listingsIcons[listing] = icons;
}

void ooObservations::SetNMEAFields(const std::vector<NMEAField>& fields)
{
    m_nmeaFields = fields;
}

const std::vector<NMEAField>& ooObservations::GetNMEAFields()
{
    return m_nmeaFields;
}
