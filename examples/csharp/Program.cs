using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Xml;
using System.Xml.Serialization;
using Transit.Siri;

namespace GtfsSiriBridge;

public record GtfsTrip(
    [property: JsonPropertyName("trip_id")] string TripId,
    [property: JsonPropertyName("route_id")] string RouteId,
    [property: JsonPropertyName("direction_id")] int DirectionId
);

public record GtfsPosition(
    [property: JsonPropertyName("latitude")] double Latitude,
    [property: JsonPropertyName("longitude")] double Longitude,
    [property: JsonPropertyName("bearing")] double? Bearing,
    [property: JsonPropertyName("speed")] double? Speed,
    [property: JsonPropertyName("odometer")] double? Odometer
);

public record GtfsVehicleDescriptor(
    [property: JsonPropertyName("id")] string Id,
    [property: JsonPropertyName("label")] string? Label,
    [property: JsonPropertyName("license_plate")] string? LicensePlate
);

public record GtfsVehiclePosition(
    [property: JsonPropertyName("trip")] GtfsTrip Trip,
    [property: JsonPropertyName("position")] GtfsPosition Position,
    [property: JsonPropertyName("current_stop_sequence")] int? CurrentStopSequence,
    [property: JsonPropertyName("current_status")] string? CurrentStatus,
    [property: JsonPropertyName("stop_id")] string? StopId,
    [property: JsonPropertyName("occupancy_status")] string? OccupancyStatus,
    [property: JsonPropertyName("vehicle")] GtfsVehicleDescriptor Vehicle
);

public record GtfsEntity(
    [property: JsonPropertyName("id")] string Id,
    [property: JsonPropertyName("vehicle")] GtfsVehiclePosition Vehicle
);

public record GtfsFeed(
    [property: JsonPropertyName("entity")] List<GtfsEntity> Entity
);

public static class Program
{
    private static string FindDataFile()
    {
        string[] candidates = {
            "data/gtfs_realtime_vehicle.json",
            "../../data/gtfs_realtime_vehicle.json",
            "../data/gtfs_realtime_vehicle.json"
        };
        foreach (var c in candidates)
        {
            if (File.Exists(c))
            {
                return Path.GetFullPath(c);
            }
        }
        throw new FileNotFoundException("Could not locate data/gtfs_realtime_vehicle.json");
    }

    private static OccupancyEnum MapOccupancy(string? status) => status switch
    {
        "EMPTY" or "MANY_SEATS_AVAILABLE" => OccupancyEnum.ManySeatsAvailable,
        "FEW_SEATS_AVAILABLE" => OccupancyEnum.FewSeatsAvailable,
        "STANDING_ROOM_ONLY" => OccupancyEnum.StandingAvailable,
        "CRUSHED_STANDING_ROOM_ONLY" or "FULL" => OccupancyEnum.Full,
        "NOT_ACCEPTING_PASSENGERS" => OccupancyEnum.NotAcceptingPassengers,
        _ => OccupancyEnum.ManySeatsAvailable
    };

    public static void Main(string[] args)
    {
        Console.WriteLine("================================================================================");
        Console.WriteLine("🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (C# 12 / .NET 8)");
        Console.WriteLine("================================================================================");

        var dataPath = FindDataFile();
        var jsonBytes = File.ReadAllBytes(dataPath);
        var feed = JsonSerializer.Deserialize<GtfsFeed>(jsonBytes)!;
        var entity = feed.Entity[0];
        var vp = entity.Vehicle;

        Console.WriteLine($"Ingesting Live Transit Telemetry: Entity {entity.Id} (Route: {vp.Trip.RouteId}, Stop: {vp.StopId})\n");

        var now = DateTimeOffset.Parse("2026-09-20T14:00:00Z");
        var validUntil = DateTimeOffset.Parse("2026-09-20T14:05:00Z");
        var arrival = DateTimeOffset.Parse("2026-09-20T14:02:30Z");

        var siri = new SiriType(
            new ServiceDeliveryStructure(
                now,
                "NDOV_LOKET_NL",
                new VehicleMonitoringDeliveryStructure(
                    now,
                    new List<VehicleActivityStructure>
                    {
                        new(
                            now,
                            validUntil,
                            entity.Id,
                            new MonitoredVehicleJourneyStructure(
                                vp.Trip.RouteId,
                                vp.Trip.DirectionId.ToString(),
                                new FramedVehicleJourneyRefStructure("2026-09-20", vp.Trip.TripId),
                                "Tram 4 - Centraal Station",
                                "GVB_AMSTERDAM",
                                "NL:S:30000099",
                                "NL:S:30000001",
                                "Amsterdam Centraal Station",
                                new LocationStructure(vp.Position.Longitude, vp.Position.Latitude, 2.5),
                                vp.Position.Bearing ?? 142.5,
                                ProgressRateEnum.NormalProgress,
                                MapOccupancy(vp.OccupancyStatus),
                                "PT0S",
                                "GVB_TRAM_2042",
                                new MonitoredCallStructure(
                                    vp.StopId ?? "NL:S:30000001",
                                    vp.CurrentStopSequence ?? 7,
                                    "Centraal Station",
                                    vp.CurrentStatus == "STOPPED_AT",
                                    arrival,
                                    arrival
                                )
                            )
                        )
                    }
                )
            ),
            "2.0"
        );

        // 1. XML Serialization via XmlSerializer
        var swXml = Stopwatch.StartNew();
        var xmlSerializer = new XmlSerializer(typeof(SiriType), "http://www.siri.org.uk/siri");
        var xmlSettings = new XmlWriterSettings
        {
            Indent = true,
            OmitXmlDeclaration = true
        };

        using var stringWriter = new StringWriter();
        using (var xmlWriter = XmlWriter.Create(stringWriter, xmlSettings))
        {
            var namespaces = new XmlSerializerNamespaces();
            namespaces.Add("", "http://www.siri.org.uk/siri");
            xmlSerializer.Serialize(xmlWriter, siri, namespaces);
        }
        swXml.Stop();

        var xmlOutput = stringWriter.ToString();
        Console.WriteLine($"[1] Generated CEN SIRI v2.0 XML Message (latency: {swXml.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine(xmlOutput.Length > 400 ? xmlOutput.Substring(0, 400) + "\n...\n" : xmlOutput);

        // 2. Native JSON Serialization on same record
        var swJson = Stopwatch.StartNew();
        var jsonOptions = new JsonSerializerOptions
        {
            WriteIndented = true
        };
        var jsonOutput = JsonSerializer.Serialize(siri, jsonOptions);
        swJson.Stop();

        Console.WriteLine($"[2] Generated Native JSON on Same Model (latency: {swJson.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine(jsonOutput.Length > 400 ? jsonOutput.Substring(0, 400) + "\n...\n" : jsonOutput);

        // 3. Inherent JSON Deserialization into SiriType record
        var swFromJson = Stopwatch.StartNew();
        var restoredSiri = JsonSerializer.Deserialize<SiriType>(jsonOutput, jsonOptions);
        swFromJson.Stop();

        var restoredMvj = restoredSiri?.ServiceDelivery.VehicleMonitoringDelivery.VehicleActivity[0].MonitoredVehicleJourney;
        Console.WriteLine($"[3] Inherent JSON Deserialization into Siri (latency: {swFromJson.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine($"    Restored LineRef: {restoredMvj?.LineRef}");
        Console.WriteLine($"    Restored VehicleRef: {restoredMvj?.VehicleRef}");
        Console.WriteLine($"    Restored Coordinates: ({restoredMvj?.VehicleLocation.Latitude}, {restoredMvj?.VehicleLocation.Longitude})");
        Console.WriteLine($"    Restored Occupancy: {restoredMvj?.Occupancy}");

        Console.WriteLine("\n✅ C# 12 GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully!");
    }
}
