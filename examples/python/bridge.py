#!/usr/bin/env python3
"""
PolyXML Transit Showcase: Google GTFS-Realtime ↔ European CEN SIRI v2.0 Adapter (Python)
Translates GTFS-Realtime JSON vehicle position telemetry to CEN SIRI XML and JSON.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

# Add generated directory to path
repo_root = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(repo_root / "generated" / "python"))

import polyxml
from siri_core import (
    FramedVehicleJourneyRefStructure,
    LocationStructure,
    MonitoredCallStructure,
    MonitoredVehicleJourneyStructure,
    OccupancyEnum,
    ProgressRateEnum,
    ServiceDeliveryStructure,
    SiriType,
    VehicleActivityStructure,
    VehicleMonitoringDeliveryStructure,
)


def map_gtfs_occupancy(status: str | None) -> OccupancyEnum:
    mapping = {
        "EMPTY": OccupancyEnum.MANY_SEATS_AVAILABLE,
        "MANY_SEATS_AVAILABLE": OccupancyEnum.MANY_SEATS_AVAILABLE,
        "FEW_SEATS_AVAILABLE": OccupancyEnum.FEW_SEATS_AVAILABLE,
        "STANDING_ROOM_ONLY": OccupancyEnum.STANDING_AVAILABLE,
        "CRUSHED_STANDING_ROOM_ONLY": OccupancyEnum.FULL,
        "FULL": OccupancyEnum.FULL,
        "NOT_ACCEPTING_PASSENGERS": OccupancyEnum.NOT_ACCEPTING_PASSENGERS,
    }
    return mapping.get(status or "", OccupancyEnum.MANY_SEATS_AVAILABLE)


def main():
    print("=" * 80)
    print("🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (Python)")
    print("=" * 80)

    gtfs_file = repo_root / "data" / "gtfs_realtime_vehicle.json"
    with open(gtfs_file, "r", encoding="utf-8") as f:
        gtfs_data = json.load(f)

    entity = gtfs_data["entity"][0]
    vp = entity["vehicle"]
    trip = vp["trip"]
    pos = vp["position"]
    veh = vp["vehicle"]

    print(f"Ingesting Live Transit Telemetry: Entity {entity['id']} (Vehicle: {veh['id']})\n")

    # Construct CEN SIRI data structure using generated PolyXML dataclasses
    t0 = time.perf_counter_ns()
    siri = SiriType(
        version="2.0",
        service_delivery=ServiceDeliveryStructure(
            response_timestamp="2026-09-20T14:00:00Z",
            producer_ref="NDOV_LOKET_NL",
            vehicle_monitoring_delivery=VehicleMonitoringDeliveryStructure(
                response_timestamp="2026-09-20T14:00:00Z",
                vehicle_activity=[
                    VehicleActivityStructure(
                        recorded_at_time="2026-09-20T14:00:00Z",
                        valid_until_time="2026-09-20T14:05:00Z",
                        vehicle_monitoring_ref=entity["id"],
                        monitored_vehicle_journey=MonitoredVehicleJourneyStructure(
                            line_ref=trip.get("route_id") or trip.get("routeId", "LINE_4"),
                            direction_ref=str(trip.get("direction_id") or trip.get("directionId", 0)),
                            framed_vehicle_journey_ref=FramedVehicleJourneyRefStructure(
                                data_frame_ref="2026-09-20",
                                dated_vehicle_journey_ref=trip.get("trip_id") or trip.get("tripId", "UNKNOWN"),
                            ),
                            published_line_name="Tram 4 - Centraal Station",
                            operator_ref="GVB_AMSTERDAM",
                            origin_ref="NL:S:30000099",
                            destination_ref="NL:S:30000001",
                            destination_name="Amsterdam Centraal Station",
                            vehicle_location=LocationStructure(
                                longitude=pos["longitude"],
                                latitude=pos["latitude"],
                                altitude=pos.get("altitude", 2.5),
                            ),
                            bearing=pos.get("bearing", 142.5),
                            progress_rate=ProgressRateEnum.NORMAL_PROGRESS,
                            occupancy=map_gtfs_occupancy(vp.get("occupancy_status") or vp.get("occupancyStatus")),
                            delay="PT0S",
                            vehicle_ref=veh["id"],
                            monitored_call=MonitoredCallStructure(
                                stop_point_ref=vp.get("stop_id") or vp.get("stopId", "NL:S:30000001"),
                                visit_number=vp.get("current_stop_sequence") or vp.get("currentStopSequence", 7),
                                stop_point_name="Centraal Station",
                                vehicle_at_stop=((vp.get("current_status") or vp.get("currentStatus")) == "STOPPED_AT"),
                                aimed_arrival_time="2026-09-20T14:02:30Z",
                                expected_arrival_time="2026-09-20T14:02:30Z",
                            ),
                        ),
                    )
                ],
            ),
        ),
    )
    t_construct = (time.perf_counter_ns() - t0) / 1000.0

    # 1. XML Serialization via PolyXML native engine
    t1 = time.perf_counter_ns()
    xml_bytes = siri.to_xml(indent=2)
    t_xml = (time.perf_counter_ns() - t1) / 1000.0
    xml_str = xml_bytes.decode("utf-8")

    print(f"[1] Generated CEN SIRI v2.0 XML Message (latency: {t_xml:.2f}µs):")
    print(xml_str.strip()[:400] + "\n...\n")

    # 2. JSON Serialization on same model
    t2 = time.perf_counter_ns()
    json_bytes = siri.to_json(indent=2)
    t_json = (time.perf_counter_ns() - t2) / 1000.0
    json_str = json_bytes.decode("utf-8")

    print(f"[2] Generated Native JSON on Same Model (latency: {t_json:.2f}µs):")
    print(json_str.strip()[:400] + "\n...\n")

    # 3. Roundtrip Inherent JSON Deserialization
    t3 = time.perf_counter_ns()
    restored_siri = SiriType.from_json(json_bytes)
    t_restore = (time.perf_counter_ns() - t3) / 1000.0

    delivery = restored_siri.service_delivery.vehicle_monitoring_delivery
    mvj = delivery.vehicle_activity[0].monitored_vehicle_journey

    print(f"[3] Inherent JSON Deserialization into Siri (latency: {t_restore:.2f}µs):")
    print(f"    Restored LineRef: {mvj.line_ref}")
    print(f"    Restored VehicleRef: {mvj.vehicle_ref}")
    print(f"    Restored Coordinates: ({mvj.vehicle_location.latitude}, {mvj.vehicle_location.longitude})")
    print(f"    Restored Occupancy: {mvj.occupancy}")

    # 4. Schemaless PolyXML C-Engine Transcoding Demo
    t4 = time.perf_counter_ns()
    transcoded_json = polyxml.xml_to_json(xml_bytes, indent=2)
    t_transcode = (time.perf_counter_ns() - t4) / 1000.0
    print(f"\n[4] PolyXML C-Engine Schemaless XML->JSON Transcoder (latency: {t_transcode:.2f}µs)")
    print(f"    Transcoded payload bytes: {len(transcoded_json)}")

    print("\n✅ Python GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully!")


if __name__ == "__main__":
    main()
