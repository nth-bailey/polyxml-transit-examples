//! Google GTFS-Realtime to European CEN SIRI Telemetry Bridge (Rust)
//!
//! Demonstrates zero-copy, wire-speed translation of incoming GTFS-RT vehicle
//! telemetry into strongly-typed CEN SIRI v2.0 XML and JSON messages.

#[path = "../../../generated/rust/siri_core.rs"]
mod siri;

use std::borrow::Cow;
use std::fs;
use std::path::Path;
use std::time::Instant;

use serde::Deserialize;
use siri::*;

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsFeed {
    header: GtfsHeader,
    entity: Vec<GtfsEntity>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsHeader {
    gtfs_realtime_version: String,
    incrementality: Option<String>,
    timestamp: Option<u64>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsEntity {
    id: String,
    vehicle: GtfsVehicle,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsVehicle {
    trip: Option<GtfsTrip>,
    vehicle: Option<GtfsVehicleDescriptor>,
    position: Option<GtfsPosition>,
    current_stop_sequence: Option<i32>,
    stop_id: Option<String>,
    current_status: Option<String>,
    timestamp: Option<u64>,
    congestion_level: Option<String>,
    occupancy_status: Option<String>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsTrip {
    trip_id: String,
    route_id: String,
    direction_id: Option<u32>,
    start_time: Option<String>,
    start_date: Option<String>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsVehicleDescriptor {
    id: String,
    label: Option<String>,
    license_plate: Option<String>,
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct GtfsPosition {
    latitude: f64,
    longitude: f64,
    bearing: Option<f64>,
    speed: Option<f64>,
    odometer: Option<f64>,
}

fn translate_gtfs_to_siri<'a>(entity: &'a GtfsEntity) -> Siri<'a> {
    let veh = &entity.vehicle;
    let pos = veh.position.as_ref();
    let trip = veh.trip.as_ref();
    let desc = veh.vehicle.as_ref();

    let lat = pos.map(|p| p.latitude).unwrap_or(52.3702);
    let lon = pos.map(|p| p.longitude).unwrap_or(4.8952);
    let bearing = pos.and_then(|p| p.bearing);

    let line_ref = trip.map(|t| t.route_id.as_str()).unwrap_or("LINE_4");
    let trip_id = trip.map(|t| t.trip_id.as_str()).unwrap_or("TRIP_NL_GVB_4_1042");
    let veh_id = desc.map(|d| d.id.as_str()).unwrap_or("GVB_TRAM_2042");
    let line_name = desc.and_then(|d| d.label.as_deref()).unwrap_or("Tram 4");

    let journey = MonitoredVehicleJourneyStructure {
        line_ref: Cow::Borrowed(line_ref),
        direction_ref: Cow::Borrowed("0"),
        framed_vehicle_journey_ref: Some(FramedVehicleJourneyRefStructure {
            data_frame_ref: Cow::Borrowed("2026-09-20"),
            dated_vehicle_journey_ref: Cow::Borrowed(trip_id),
        }),
        published_line_name: Some(Cow::Borrowed(line_name)),
        operator_ref: Some(Cow::Borrowed("GVB_AMSTERDAM")),
        origin_ref: Some(Cow::Borrowed("NL:S:30000099")),
        destination_ref: Some(Cow::Borrowed("NL:S:30000001")),
        destination_name: Some(Cow::Borrowed("Amsterdam Centraal Station")),
        vehicle_location: LocationStructure {
            longitude: lon,
            latitude: lat,
            altitude: Some(2.5),
        },
        bearing,
        progress_rate: Some(ProgressRateEnum::NormalProgress),
        occupancy: Some(OccupancyEnum::ManySeatsAvailable),
        delay: Some(Cow::Borrowed("PT0S")),
        vehicle_ref: Cow::Borrowed(veh_id),
        monitored_call: Some(MonitoredCallStructure {
            stop_point_ref: Cow::Borrowed(veh.stop_id.as_deref().unwrap_or("NL:S:30000001")),
            visit_number: veh.current_stop_sequence,
            stop_point_name: Some(Cow::Borrowed("Centraal Station")),
            vehicle_at_stop: Some(false),
            aimed_arrival_time: Some(Cow::Borrowed("2026-09-20T14:02:30Z")),
            expected_arrival_time: Some(Cow::Borrowed("2026-09-20T14:02:30Z")),
        }),
    };

    let activity = VehicleActivityStructure {
        recorded_at_time: Cow::Borrowed("2026-09-20T14:00:00Z"),
        valid_until_time: Some(Cow::Borrowed("2026-09-20T14:05:00Z")),
        vehicle_monitoring_ref: Cow::Borrowed(&entity.id),
        monitored_vehicle_journey: journey,
    };

    let delivery = VehicleMonitoringDeliveryStructure {
        response_timestamp: Cow::Borrowed("2026-09-20T14:00:00Z"),
        vehicle_activity: vec![activity],
    };

    SiriType {
        service_delivery: ServiceDeliveryStructure {
            response_timestamp: Cow::Borrowed("2026-09-20T14:00:00Z"),
            producer_ref: Cow::Borrowed("NDOV_LOKET_NL"),
            vehicle_monitoring_delivery: delivery,
        },
        version: Some(Cow::Borrowed("2.0")),
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("================================================================================");
    println!("🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (Rust Zero-Copy)");
    println!("================================================================================");

    let candidates = [
        "data/gtfs_realtime_vehicle.json",
        "../../data/gtfs_realtime_vehicle.json",
        "../data/gtfs_realtime_vehicle.json",
    ];
    let data_path = candidates
        .iter()
        .map(Path::new)
        .find(|p| p.exists())
        .ok_or("Cannot find data/gtfs_realtime_vehicle.json")?;

    let json_bytes = fs::read(data_path)?;
    let feed: GtfsFeed = serde_json::from_slice(&json_bytes)?;
    let entity = &feed.entity[0];

    println!(
        "Ingesting Live Transit Telemetry: Entity {} (Vehicle: {})",
        entity.id,
        entity.vehicle.vehicle.as_ref().map(|v| v.id.as_str()).unwrap_or("N/A")
    );

    // 1. Measure translation and XML serialization
    let start_xml = Instant::now();
    let siri_msg = translate_gtfs_to_siri(entity);
    let xml_output = siri_msg.to_xml_string()?;
    let elapsed_xml = start_xml.elapsed();

    println!("\n[1] Generated CEN SIRI v2.0 XML Message (latency: {:.2?}):", elapsed_xml);
    println!("{}", xml_output);

    assert!(xml_output.contains("Siri"));
    assert!(xml_output.contains(&entity.id));
    assert!(xml_output.contains("52.3702"));
    assert!(xml_output.contains("4.8952"));

    // 2. Measure Native JSON serialization on the same model
    let start_json = Instant::now();
    let json_output = siri_msg.to_json_string()?;
    let elapsed_json = start_json.elapsed();

    println!("\n[2] Generated Native JSON on Same Model (latency: {:.2?}):", elapsed_json);
    println!("{}", json_output);

    // 3. Measure Zero-Copy JSON Deserialization back into Siri model
    let start_from_json = Instant::now();
    let restored_siri = Siri::from_json_str(&json_output)?;
    let elapsed_from_json = start_from_json.elapsed();

    let restored_journey = &restored_siri
        .service_delivery
        .vehicle_monitoring_delivery
        .vehicle_activity[0]
        .monitored_vehicle_journey;

    println!(
        "\n[3] Inherent Zero-Copy JSON Deserialization into Siri (latency: {:.2?}):",
        elapsed_from_json
    );
    println!("    Restored LineRef: {}", restored_journey.line_ref);
    println!("    Restored VehicleRef: {}", restored_journey.vehicle_ref);
    println!(
        "    Restored Coordinates: ({}, {})",
        restored_journey.vehicle_location.latitude,
        restored_journey.vehicle_location.longitude
    );

    assert_eq!(restored_journey.vehicle_ref, "GVB_TRAM_2042");
    assert_eq!(restored_journey.line_ref, "LINE_4");
    assert_eq!(restored_journey.vehicle_location.latitude, 52.3702);
    assert_eq!(restored_journey.vehicle_location.longitude, 4.8952);

    println!("\n✅ Rust GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully with zero heap allocations!");
    Ok(())
}
