package main

import (
	"encoding/json"
	"encoding/xml"
	"fmt"
	"os"
	"time"

	siri "github.com/nth-bailey/polyxml-transit-examples/generated/go"
)

type GtfsFeedMessage struct {
	Header struct {
		GtfsRealtimeVersion string `json:"gtfs_realtime_version"`
		Timestamp           int64  `json:"timestamp"`
	} `json:"header"`
	Entity []struct {
		ID      string `json:"id"`
		Vehicle struct {
			Trip struct {
				TripID      string `json:"trip_id"`
				RouteID     string `json:"route_id"`
				DirectionID int    `json:"direction_id"`
			} `json:"trip"`
			Position struct {
				Latitude  float64  `json:"latitude"`
				Longitude float64  `json:"longitude"`
				Bearing   *float64 `json:"bearing"`
				Altitude  *float64 `json:"altitude"`
			} `json:"position"`
			CurrentStopSequence *int32  `json:"current_stop_sequence"`
			CurrentStatus       string  `json:"current_status"`
			Timestamp           int64   `json:"timestamp"`
			OccupancyStatus     string  `json:"occupancy_status"`
			StopID              string  `json:"stop_id"`
			Vehicle             struct {
				ID string `json:"id"`
			} `json:"vehicle"`
		} `json:"vehicle"`
	} `json:"entity"`
}

func mapGtfsOccupancy(status string) siri.OccupancyEnum {
	switch status {
	case "EMPTY", "MANY_SEATS_AVAILABLE":
		return siri.OccupancyEnumManySeatsAvailable
	case "FEW_SEATS_AVAILABLE":
		return siri.OccupancyEnumFewSeatsAvailable
	case "STANDING_ROOM_ONLY":
		return siri.OccupancyEnumStandingAvailable
	case "CRUSHED_STANDING_ROOM_ONLY", "FULL":
		return siri.OccupancyEnumFull
	case "NOT_ACCEPTING_PASSENGERS":
		return siri.OccupancyEnumNotAcceptingPassengers
	default:
		return siri.OccupancyEnumManySeatsAvailable
	}
}

func strPtr(s string) *string       { return &s }
func boolPtr(b bool) *bool          { return &b }
func int32Ptr(i int32) *int32       { return &i }
func float64Ptr(f float64) *float64 { return &f }
func timePtr(t time.Time) *time.Time { return &t }

func main() {
	fmt.Println("================================================================================")
	fmt.Println("🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (Go)")
	fmt.Println("================================================================================")

	data, err := os.ReadFile("data/gtfs_realtime_vehicle.json")
	if err != nil {
		data, err = os.ReadFile("../../data/gtfs_realtime_vehicle.json")
		if err != nil {
			panic(fmt.Sprintf("Failed to read GTFS data: %v", err))
		}
	}

	var feed GtfsFeedMessage
	if err := json.Unmarshal(data, &feed); err != nil {
		panic(fmt.Sprintf("Failed to parse GTFS JSON: %v", err))
	}

	entity := feed.Entity[0]
	vp := entity.Vehicle
	fmt.Printf("Ingesting Live Transit Telemetry: Entity %s (Vehicle: %s)\n\n", entity.ID, vp.Vehicle.ID)

	now := time.Now().UTC()
	validUntil := now.Add(5 * time.Minute)
	arrival := now.Add(2*time.Minute + 30*time.Second)

	progressRate := siri.ProgressRateEnumNormalProgress
	occupancy := mapGtfsOccupancy(vp.OccupancyStatus)
	atStop := vp.CurrentStatus == "STOPPED_AT"

	siriMsg := siri.SiriType{
		XMLName: xml.Name{Space: "http://www.siri.org.uk/siri", Local: "Siri"},
		Version: strPtr("2.0"),
		ServiceDelivery: siri.ServiceDeliveryStructure{
			ResponseTimestamp: now,
			ProducerRef:       "NDOV_LOKET_NL",
			VehicleMonitoringDelivery: siri.VehicleMonitoringDeliveryStructure{
				ResponseTimestamp: now,
				VehicleActivity: []siri.VehicleActivityStructure{
					{
						RecordedAtTime:       now,
						ValidUntilTime:       &validUntil,
						VehicleMonitoringRef: entity.ID,
						MonitoredVehicleJourney: siri.MonitoredVehicleJourneyStructure{
							LineRef:         vp.Trip.RouteID,
							DirectionRef:    fmt.Sprintf("%d", vp.Trip.DirectionID),
							PublishedLineName: strPtr("Tram 4 - Centraal Station"),
							OperatorRef:     strPtr("GVB_AMSTERDAM"),
							OriginRef:       strPtr("NL:S:30000099"),
							DestinationRef:  strPtr("NL:S:30000001"),
							DestinationName: strPtr("Amsterdam Centraal Station"),
							FramedVehicleJourneyRef: &siri.FramedVehicleJourneyRefStructure{
								DataFrameRef:           "2026-09-20",
								DatedVehicleJourneyRef: vp.Trip.TripID,
							},
							VehicleLocation: siri.LocationStructure{
								Longitude: vp.Position.Longitude,
								Latitude:  vp.Position.Latitude,
								Altitude:  vp.Position.Altitude,
							},
							Bearing:      vp.Position.Bearing,
							ProgressRate: &progressRate,
							Occupancy:    &occupancy,
							Delay:        strPtr("PT0S"),
							VehicleRef:   vp.Vehicle.ID,
							MonitoredCall: &siri.MonitoredCallStructure{
								StopPointRef:        vp.StopID,
								VisitNumber:         vp.CurrentStopSequence,
								StopPointName:       strPtr("Centraal Station"),
								VehicleAtStop:       boolPtr(atStop),
								AimedArrivalTime:    timePtr(arrival),
								ExpectedArrivalTime: timePtr(arrival),
							},
						},
					},
				},
			},
		},
	}

	// 1. Dual-tag XML Serialization
	t0 := time.Now()
	xmlBytes, err := xml.MarshalIndent(siriMsg, "", "  ")
	if err != nil {
		panic(fmt.Sprintf("Failed to marshal XML: %v", err))
	}
	tXml := time.Since(t0)

	fmt.Printf("[1] Generated CEN SIRI v2.0 XML Message (latency: %v):\n", tXml)
	previewXml := string(xmlBytes)
	if len(previewXml) > 400 {
		previewXml = previewXml[:400]
	}
	fmt.Printf("%s\n...\n\n", previewXml)

	// 2. Dual-tag JSON Serialization on SAME Struct
	t1 := time.Now()
	jsonBytes, err := json.MarshalIndent(siriMsg, "", "  ")
	if err != nil {
		panic(fmt.Sprintf("Failed to marshal JSON: %v", err))
	}
	tJson := time.Since(t1)

	fmt.Printf("[2] Generated Native JSON on Same Model (latency: %v):\n", tJson)
	previewJson := string(jsonBytes)
	if len(previewJson) > 400 {
		previewJson = previewJson[:400]
	}
	fmt.Printf("%s\n...\n\n", previewJson)

	// 3. Roundtrip Inherent JSON Deserialization into Siri
	t2 := time.Now()
	var restoredSiri siri.SiriType
	if err := json.Unmarshal(jsonBytes, &restoredSiri); err != nil {
		panic(fmt.Sprintf("Failed to unmarshal JSON into Siri: %v", err))
	}
	tRestore := time.Since(t2)

	restoredDelivery := restoredSiri.ServiceDelivery.VehicleMonitoringDelivery
	restoredMvj := restoredDelivery.VehicleActivity[0].MonitoredVehicleJourney

	fmt.Printf("[3] Inherent JSON Deserialization into Siri (latency: %v):\n", tRestore)
	fmt.Printf("    Restored LineRef: %s\n", restoredMvj.LineRef)
	fmt.Printf("    Restored VehicleRef: %s\n", restoredMvj.VehicleRef)
	fmt.Printf("    Restored Coordinates: (%.4f, %.4f)\n", restoredMvj.VehicleLocation.Latitude, restoredMvj.VehicleLocation.Longitude)
	if restoredMvj.Occupancy != nil {
		fmt.Printf("    Restored Occupancy: %s\n", *restoredMvj.Occupancy)
	}

	fmt.Println("\n✅ Go GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully!")
}
