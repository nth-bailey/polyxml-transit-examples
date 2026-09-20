#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <filesystem>
#include <cassert>
#include "siri_core.hpp"

namespace fs = std::filesystem;
using namespace polyxml::generated;

// Statically verify C++20 XmlModel concept
static_assert(XmlModel<SiriType>, "SiriType must satisfy C++20 XmlModel concept");
static_assert(XmlModel<MonitoredVehicleJourneyStructure>, "MonitoredVehicleJourneyStructure must satisfy C++20 XmlModel concept");

struct GtfsVehicleTelemetry {
    std::string entity_id;
    std::string trip_id;
    std::string route_id;
    int direction_id = 0;
    std::string vehicle_id;
    std::string vehicle_label;
    double latitude = 0.0;
    double longitude = 0.0;
    double bearing = 0.0;
    double speed = 0.0;
    double altitude = 2.5;
    std::string stop_id;
    int current_stop_sequence = 7;
    std::string current_status;
    std::string occupancy_status;
};

std::string extract_string_field(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

double extract_double_field(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return std::stod(match[1].str());
    }
    return 0.0;
}

int extract_int_field(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return std::stoi(match[1].str());
    }
    return 0;
}

std::string find_data_file() {
    const std::vector<std::string> candidates = {
        "data/gtfs_realtime_vehicle.json",
        "../../data/gtfs_realtime_vehicle.json",
        "../data/gtfs_realtime_vehicle.json"
    };
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return fs::absolute(p).string();
        }
    }
    return "";
}

OccupancyEnum map_occupancy(const std::string& occ) {
    if (occ == "EMPTY" || occ == "MANY_SEATS_AVAILABLE") return OccupancyEnum::ManySeatsAvailable;
    if (occ == "FEW_SEATS_AVAILABLE") return OccupancyEnum::FewSeatsAvailable;
    if (occ == "STANDING_ROOM_ONLY") return OccupancyEnum::StandingAvailable;
    if (occ == "CRUSHED_STANDING_ROOM_ONLY" || occ == "FULL") return OccupancyEnum::Full;
    if (occ == "NOT_ACCEPTING_PASSENGERS") return OccupancyEnum::NotAcceptingPassengers;
    return OccupancyEnum::ManySeatsAvailable;
}

std::string serialize_siri_xml(const SiriType& siri) {
    std::ostringstream oss;
    oss << "<Siri xmlns=\"http://www.siri.org.uk/siri\"";
    if (siri.version) oss << " version=\"" << *siri.version << "\"";
    oss << ">\n";
    oss << "  <ServiceDelivery>\n";
    oss << "    <ResponseTimestamp>" << siri.service_delivery.response_timestamp << "</ResponseTimestamp>\n";
    oss << "    <ProducerRef>" << siri.service_delivery.producer_ref << "</ProducerRef>\n";
    oss << "    <VehicleMonitoringDelivery>\n";
    const auto& deliv = siri.service_delivery.vehicle_monitoring_delivery;
    oss << "      <ResponseTimestamp>" << deliv.response_timestamp << "</ResponseTimestamp>\n";
    for (const auto& act : deliv.vehicle_activity) {
        oss << "      <VehicleActivity>\n";
        oss << "        <RecordedAtTime>" << act.recorded_at_time << "</RecordedAtTime>\n";
        if (act.valid_until_time) oss << "        <ValidUntilTime>" << *act.valid_until_time << "</ValidUntilTime>\n";
        oss << "        <VehicleMonitoringRef>" << act.vehicle_monitoring_ref << "</VehicleMonitoringRef>\n";
        oss << "        <MonitoredVehicleJourney>\n";
        const auto& mvj = act.monitored_vehicle_journey;
        oss << "          <LineRef>" << mvj.line_ref << "</LineRef>\n";
        oss << "          <DirectionRef>" << mvj.direction_ref << "</DirectionRef>\n";
        if (mvj.framed_vehicle_journey_ref) {
            oss << "          <FramedVehicleJourneyRef>\n";
            oss << "            <DataFrameRef>" << mvj.framed_vehicle_journey_ref->data_frame_ref << "</DataFrameRef>\n";
            oss << "            <DatedVehicleJourneyRef>" << mvj.framed_vehicle_journey_ref->dated_vehicle_journey_ref << "</DatedVehicleJourneyRef>\n";
            oss << "          </FramedVehicleJourneyRef>\n";
        }
        if (mvj.published_line_name) oss << "          <PublishedLineName>" << *mvj.published_line_name << "</PublishedLineName>\n";
        if (mvj.operator_ref) oss << "          <OperatorRef>" << *mvj.operator_ref << "</OperatorRef>\n";
        if (mvj.origin_ref) oss << "          <OriginRef>" << *mvj.origin_ref << "</OriginRef>\n";
        if (mvj.destination_ref) oss << "          <DestinationRef>" << *mvj.destination_ref << "</DestinationRef>\n";
        if (mvj.destination_name) oss << "          <DestinationName>" << *mvj.destination_name << "</DestinationName>\n";
        oss << "          <VehicleLocation>\n";
        oss << "            <Longitude>" << mvj.vehicle_location.longitude << "</Longitude>\n";
        oss << "            <Latitude>" << mvj.vehicle_location.latitude << "</Latitude>\n";
        if (mvj.vehicle_location.altitude) oss << "            <Altitude>" << *mvj.vehicle_location.altitude << "</Altitude>\n";
        oss << "          </VehicleLocation>\n";
        if (mvj.bearing) oss << "          <Bearing>" << *mvj.bearing << "</Bearing>\n";
        if (mvj.progress_rate) oss << "          <ProgressRate>" << to_string(*mvj.progress_rate) << "</ProgressRate>\n";
        if (mvj.occupancy) oss << "          <Occupancy>" << to_string(*mvj.occupancy) << "</Occupancy>\n";
        if (mvj.delay) oss << "          <Delay>" << *mvj.delay << "</Delay>\n";
        oss << "          <VehicleRef>" << mvj.vehicle_ref << "</VehicleRef>\n";
        if (mvj.monitored_call) {
            oss << "          <MonitoredCall>\n";
            oss << "            <StopPointRef>" << mvj.monitored_call->stop_point_ref << "</StopPointRef>\n";
            if (mvj.monitored_call->visit_number) oss << "            <VisitNumber>" << *mvj.monitored_call->visit_number << "</VisitNumber>\n";
            if (mvj.monitored_call->stop_point_name) oss << "            <StopPointName>" << *mvj.monitored_call->stop_point_name << "</StopPointName>\n";
            if (mvj.monitored_call->vehicle_at_stop) oss << "            <VehicleAtStop>" << (*mvj.monitored_call->vehicle_at_stop ? "true" : "false") << "</VehicleAtStop>\n";
            if (mvj.monitored_call->aimed_arrival_time) oss << "            <AimedArrivalTime>" << *mvj.monitored_call->aimed_arrival_time << "</AimedArrivalTime>\n";
            if (mvj.monitored_call->expected_arrival_time) oss << "            <ExpectedArrivalTime>" << *mvj.monitored_call->expected_arrival_time << "</ExpectedArrivalTime>\n";
            oss << "          </MonitoredCall>\n";
        }
        oss << "        </MonitoredVehicleJourney>\n";
        oss << "      </VehicleActivity>\n";
    }
    oss << "    </VehicleMonitoringDelivery>\n";
    oss << "  </ServiceDelivery>\n";
    oss << "</Siri>\n";
    return oss.str();
}

std::string serialize_siri_json(const SiriType& siri) {
    std::ostringstream oss;
    oss << "{\n";
    if (siri.version) oss << "  \"version\": \"" << *siri.version << "\",\n";
    oss << "  \"ServiceDelivery\": {\n";
    oss << "    \"ResponseTimestamp\": \"" << siri.service_delivery.response_timestamp << "\",\n";
    oss << "    \"ProducerRef\": \"" << siri.service_delivery.producer_ref << "\",\n";
    oss << "    \"VehicleMonitoringDelivery\": {\n";
    const auto& deliv = siri.service_delivery.vehicle_monitoring_delivery;
    oss << "      \"ResponseTimestamp\": \"" << deliv.response_timestamp << "\",\n";
    oss << "      \"VehicleActivity\": [\n";
    for (size_t i = 0; i < deliv.vehicle_activity.size(); ++i) {
        const auto& act = deliv.vehicle_activity[i];
        const auto& mvj = act.monitored_vehicle_journey;
        oss << "        {\n";
        oss << "          \"RecordedAtTime\": \"" << act.recorded_at_time << "\",\n";
        oss << "          \"VehicleMonitoringRef\": \"" << act.vehicle_monitoring_ref << "\",\n";
        oss << "          \"MonitoredVehicleJourney\": {\n";
        oss << "            \"LineRef\": \"" << mvj.line_ref << "\",\n";
        oss << "            \"VehicleRef\": \"" << mvj.vehicle_ref << "\",\n";
        oss << "            \"VehicleLocation\": {\n";
        oss << "              \"Latitude\": " << mvj.vehicle_location.latitude << ",\n";
        oss << "              \"Longitude\": " << mvj.vehicle_location.longitude << "\n";
        oss << "            }\n";
        oss << "          }\n";
        oss << "        }" << (i + 1 < deliv.vehicle_activity.size() ? "," : "") << "\n";
    }
    oss << "      ]\n";
    oss << "    }\n";
    oss << "  }\n";
    oss << "}\n";
    return oss.str();
}

int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << "🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (C++20)" << std::endl;
    std::cout << "================================================================================" << std::endl;

    std::string path = find_data_file();
    if (path.empty()) {
        std::cerr << "Error: Could not find gtfs_realtime_vehicle.json" << std::endl;
        return 1;
    }

    std::ifstream file(path);
    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    GtfsVehicleTelemetry telemetry;
    telemetry.entity_id = extract_string_field(json_content, "id");
    telemetry.trip_id = extract_string_field(json_content, "trip_id");
    telemetry.route_id = extract_string_field(json_content, "route_id");
    telemetry.direction_id = extract_int_field(json_content, "direction_id");
    telemetry.vehicle_id = extract_string_field(json_content, "id");
    telemetry.latitude = extract_double_field(json_content, "latitude");
    telemetry.longitude = extract_double_field(json_content, "longitude");
    telemetry.bearing = extract_double_field(json_content, "bearing");
    telemetry.speed = extract_double_field(json_content, "speed");
    telemetry.stop_id = extract_string_field(json_content, "stop_id");
    telemetry.current_stop_sequence = extract_int_field(json_content, "current_stop_sequence");
    telemetry.current_status = extract_string_field(json_content, "current_status");
    telemetry.occupancy_status = extract_string_field(json_content, "occupancy_status");

    std::cout << "Ingesting Live Transit Telemetry: Entity " << telemetry.entity_id 
              << " (Route: " << telemetry.route_id << ", Stop: " << telemetry.stop_id << ")\n" << std::endl;

    // Construct C++20 SIRI model
    SiriType siri;
    siri.version = "2.0";
    siri.service_delivery.response_timestamp = "2026-09-20T14:00:00Z";
    siri.service_delivery.producer_ref = "NDOV_LOKET_NL";
    siri.service_delivery.vehicle_monitoring_delivery.response_timestamp = "2026-09-20T14:00:00Z";

    VehicleActivityStructure activity;
    activity.recorded_at_time = "2026-09-20T14:00:00Z";
    activity.valid_until_time = "2026-09-20T14:05:00Z";
    activity.vehicle_monitoring_ref = telemetry.entity_id;

    auto& mvj = activity.monitored_vehicle_journey;
    mvj.line_ref = telemetry.route_id;
    mvj.direction_ref = std::to_string(telemetry.direction_id);
    mvj.published_line_name = "Tram 4 - Centraal Station";
    mvj.operator_ref = "GVB_AMSTERDAM";
    mvj.origin_ref = "NL:S:30000099";
    mvj.destination_ref = "NL:S:30000001";
    mvj.destination_name = "Amsterdam Centraal Station";

    FramedVehicleJourneyRefStructure framed;
    framed.data_frame_ref = "2026-09-20";
    framed.dated_vehicle_journey_ref = telemetry.trip_id;
    mvj.framed_vehicle_journey_ref = framed;

    mvj.vehicle_location.longitude = telemetry.longitude;
    mvj.vehicle_location.latitude = telemetry.latitude;
    mvj.vehicle_location.altitude = 2.5;

    mvj.bearing = telemetry.bearing;
    mvj.progress_rate = ProgressRateEnum::NormalProgress;
    mvj.occupancy = map_occupancy(telemetry.occupancy_status);
    mvj.delay = "PT0S";
    mvj.vehicle_ref = "GVB_TRAM_2042";

    MonitoredCallStructure call;
    call.stop_point_ref = telemetry.stop_id;
    call.visit_number = telemetry.current_stop_sequence;
    call.stop_point_name = "Centraal Station";
    call.vehicle_at_stop = (telemetry.current_status == "STOPPED_AT");
    call.aimed_arrival_time = "2026-09-20T14:02:30Z";
    call.expected_arrival_time = "2026-09-20T14:02:30Z";
    mvj.monitored_call = call;

    siri.service_delivery.vehicle_monitoring_delivery.vehicle_activity.push_back(activity);

    // Concept and validation checks
    assert(siri.validate());
    SiriType siri_clone = siri;
    assert(siri == siri_clone);

    // 1. XML Serialization
    auto t0 = std::chrono::high_resolution_clock::now();
    std::string siri_xml = serialize_siri_xml(siri);
    auto t1 = std::chrono::high_resolution_clock::now();
    double xml_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::cout << "[1] Generated CEN SIRI v2.0 XML Message (latency: " << xml_us << "µs):" << std::endl;
    std::cout << siri_xml.substr(0, std::min<size_t>(siri_xml.size(), 400)) << "\n...\n" << std::endl;

    // 2. JSON Serialization
    auto t2 = std::chrono::high_resolution_clock::now();
    std::string siri_json = serialize_siri_json(siri);
    auto t3 = std::chrono::high_resolution_clock::now();
    double json_us = std::chrono::duration<double, std::micro>(t3 - t2).count();

    std::cout << "[2] Generated Native JSON on Same Model (latency: " << json_us << "µs):" << std::endl;
    std::cout << siri_json.substr(0, std::min<size_t>(siri_json.size(), 400)) << "\n...\n" << std::endl;

    // 3. Model Inspection
    std::cout << "[3] C++20 Value Type Inspection:" << std::endl;
    std::cout << "    LineRef: " << mvj.line_ref << std::endl;
    std::cout << "    VehicleRef: " << mvj.vehicle_ref << std::endl;
    std::cout << "    Coordinates: (" << mvj.vehicle_location.latitude << ", " << mvj.vehicle_location.longitude << ")" << std::endl;
    if (mvj.occupancy) {
        std::cout << "    Occupancy: " << to_string(*mvj.occupancy) << std::endl;
    }
    std::cout << "    C++20 Equality operator== check: PASS" << std::endl;

    std::cout << "\n✅ C++20 GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully!" << std::endl;
    return 0;
}
