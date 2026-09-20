import fs from "node:fs";
import path from "node:path";
import {
  OccupancyEnum,
  ProgressRateEnum,
  SiriTypeSchema,
  type SiriType,
} from "../../generated/typescript/siri_core.ts";

interface GtfsFeed {
  header: {
    gtfs_realtime_version: string;
    timestamp: number;
  };
  entity: Array<{
    id: string;
    vehicle: {
      trip: {
        trip_id: string;
        route_id: string;
        direction_id: number;
      };
      vehicle: {
        id: string;
        label: string;
      };
      position: {
        latitude: number;
        longitude: number;
        bearing?: number;
        altitude?: number;
      };
      current_stop_sequence?: number;
      current_status?: string;
      stop_id?: string;
      occupancy_status?: string;
    };
  }>;
}

function findDataFile(): string {
  const candidates = [
    "data/gtfs_realtime_vehicle.json",
    "../../data/gtfs_realtime_vehicle.json",
    "../data/gtfs_realtime_vehicle.json",
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) {
      return path.resolve(c);
    }
  }
  throw new Error("Could not locate data/gtfs_realtime_vehicle.json");
}

function mapOccupancy(status?: string): OccupancyEnum {
  switch (status) {
    case "EMPTY":
    case "MANY_SEATS_AVAILABLE":
      return OccupancyEnum.ManySeatsAvailable;
    case "FEW_SEATS_AVAILABLE":
      return OccupancyEnum.FewSeatsAvailable;
    case "STANDING_ROOM_ONLY":
      return OccupancyEnum.StandingAvailable;
    case "CRUSHED_STANDING_ROOM_ONLY":
    case "FULL":
      return OccupancyEnum.Full;
    case "NOT_ACCEPTING_PASSENGERS":
      return OccupancyEnum.NotAcceptingPassengers;
    default:
      return OccupancyEnum.ManySeatsAvailable;
  }
}

function serializeXml(siri: SiriType): string {
  const v = siri.version ? ` version="${siri.version}"` : "";
  let xml = `<Siri xmlns="http://www.siri.org.uk/siri"${v}>\n`;
  xml += "  <ServiceDelivery>\n";
  xml += `    <ResponseTimestamp>${siri.serviceDelivery.responseTimestamp}</ResponseTimestamp>\n`;
  xml += `    <ProducerRef>${siri.serviceDelivery.producerRef}</ProducerRef>\n`;
  xml += "    <VehicleMonitoringDelivery>\n";
  const deliv = siri.serviceDelivery.vehicleMonitoringDelivery;
  xml += `      <ResponseTimestamp>${deliv.responseTimestamp}</ResponseTimestamp>\n`;
  for (const act of deliv.vehicleActivity) {
    xml += "      <VehicleActivity>\n";
    xml += `        <RecordedAtTime>${act.recordedAtTime}</RecordedAtTime>\n`;
    if (act.validUntilTime) xml += `        <ValidUntilTime>${act.validUntilTime}</ValidUntilTime>\n`;
    xml += `        <VehicleMonitoringRef>${act.vehicleMonitoringRef}</VehicleMonitoringRef>\n`;
    xml += "        <MonitoredVehicleJourney>\n";
    const mvj = act.monitoredVehicleJourney;
    xml += `          <LineRef>${mvj.lineRef}</LineRef>\n`;
    xml += `          <DirectionRef>${mvj.directionRef}</DirectionRef>\n`;
    if (mvj.framedVehicleJourneyRef) {
      xml += "          <FramedVehicleJourneyRef>\n";
      xml += `            <DataFrameRef>${mvj.framedVehicleJourneyRef.dataFrameRef}</DataFrameRef>\n`;
      xml += `            <DatedVehicleJourneyRef>${mvj.framedVehicleJourneyRef.datedVehicleJourneyRef}</DatedVehicleJourneyRef>\n`;
      xml += "          </FramedVehicleJourneyRef>\n";
    }
    if (mvj.publishedLineName) xml += `          <PublishedLineName>${mvj.publishedLineName}</PublishedLineName>\n`;
    if (mvj.operatorRef) xml += `          <OperatorRef>${mvj.operatorRef}</OperatorRef>\n`;
    if (mvj.originRef) xml += `          <OriginRef>${mvj.originRef}</OriginRef>\n`;
    if (mvj.destinationRef) xml += `          <DestinationRef>${mvj.destinationRef}</DestinationRef>\n`;
    if (mvj.destinationName) xml += `          <DestinationName>${mvj.destinationName}</DestinationName>\n`;
    xml += "          <VehicleLocation>\n";
    xml += `            <Longitude>${mvj.vehicleLocation.longitude}</Longitude>\n`;
    xml += `            <Latitude>${mvj.vehicleLocation.latitude}</Latitude>\n`;
    if (mvj.vehicleLocation.altitude !== undefined) xml += `            <Altitude>${mvj.vehicleLocation.altitude}</Altitude>\n`;
    xml += "          </VehicleLocation>\n";
    if (mvj.bearing !== undefined) xml += `          <Bearing>${mvj.bearing}</Bearing>\n`;
    if (mvj.progressRate) xml += `          <ProgressRate>${mvj.progressRate}</ProgressRate>\n`;
    if (mvj.occupancy) xml += `          <Occupancy>${mvj.occupancy}</Occupancy>\n`;
    if (mvj.delay) xml += `          <Delay>${mvj.delay}</Delay>\n`;
    xml += `          <VehicleRef>${mvj.vehicleRef}</VehicleRef>\n`;
    if (mvj.monitoredCall) {
      xml += "          <MonitoredCall>\n";
      xml += `            <StopPointRef>${mvj.monitoredCall.stopPointRef}</StopPointRef>\n`;
      if (mvj.monitoredCall.visitNumber !== undefined) xml += `            <VisitNumber>${mvj.monitoredCall.visitNumber}</VisitNumber>\n`;
      if (mvj.monitoredCall.stopPointName) xml += `            <StopPointName>${mvj.monitoredCall.stopPointName}</StopPointName>\n`;
      if (mvj.monitoredCall.vehicleAtStop !== undefined) xml += `            <VehicleAtStop>${mvj.monitoredCall.vehicleAtStop}</VehicleAtStop>\n`;
      if (mvj.monitoredCall.aimedArrivalTime) xml += `            <AimedArrivalTime>${mvj.monitoredCall.aimedArrivalTime}</AimedArrivalTime>\n`;
      if (mvj.monitoredCall.expectedArrivalTime) xml += `            <ExpectedArrivalTime>${mvj.monitoredCall.expectedArrivalTime}</ExpectedArrivalTime>\n`;
      xml += "          </MonitoredCall>\n";
    }
    xml += "        </MonitoredVehicleJourney>\n";
    xml += "      </VehicleActivity>\n";
  }
  xml += "    </VehicleMonitoringDelivery>\n";
  xml += "  </ServiceDelivery>\n";
  xml += "</Siri>\n";
  return xml;
}

function main() {
  console.log("================================================================================");
  console.log("🚍 PolyXML: Google GTFS-RT ↔ European CEN SIRI Transit Bridge (TypeScript 5+)");
  console.log("================================================================================");

  const dataPath = findDataFile();
  const rawData = fs.readFileSync(dataPath, "utf-8");
  const feed: GtfsFeed = JSON.parse(rawData);

  const entity = feed.entity[0];
  const vp = entity.vehicle;

  console.log(`Ingesting Live Transit Telemetry: Entity ${entity.id} (Route: ${vp.trip.route_id}, Stop: ${vp.stop_id})\n`);

  const siri: SiriType = {
    version: "2.0",
    serviceDelivery: {
      responseTimestamp: "2026-09-20T14:00:00Z",
      producerRef: "NDOV_LOKET_NL",
      vehicleMonitoringDelivery: {
        responseTimestamp: "2026-09-20T14:00:00Z",
        vehicleActivity: [
          {
            recordedAtTime: "2026-09-20T14:00:00Z",
            validUntilTime: "2026-09-20T14:05:00Z",
            vehicleMonitoringRef: entity.id,
            monitoredVehicleJourney: {
              lineRef: vp.trip.route_id,
              directionRef: String(vp.trip.direction_id),
              framedVehicleJourneyRef: {
                dataFrameRef: "2026-09-20",
                datedVehicleJourneyRef: vp.trip.trip_id,
              },
              publishedLineName: "Tram 4 - Centraal Station",
              operatorRef: "GVB_AMSTERDAM",
              originRef: "NL:S:30000099",
              destinationRef: "NL:S:30000001",
              destinationName: "Amsterdam Centraal Station",
              vehicleLocation: {
                longitude: vp.position.longitude,
                latitude: vp.position.latitude,
                altitude: vp.position.altitude ?? 2.5,
              },
              bearing: vp.position.bearing ?? 142.5,
              progressRate: ProgressRateEnum.NormalProgress,
              occupancy: mapOccupancy(vp.occupancy_status),
              delay: "PT0S",
              vehicleRef: "GVB_TRAM_2042",
              monitoredCall: {
                stopPointRef: vp.stop_id ?? "NL:S:30000001",
                visitNumber: vp.current_stop_sequence ?? 7,
                stopPointName: "Centraal Station",
                vehicleAtStop: vp.current_status === "STOPPED_AT",
                aimedArrivalTime: "2026-09-20T14:02:30Z",
                expectedArrivalTime: "2026-09-20T14:02:30Z",
              },
            },
          },
        ],
      },
    },
  };

  // Validate using generated Zod schema
  const validated = SiriTypeSchema.parse(siri);

  // 1. XML Serialization
  const t0 = performance.now();
  const xml = serializeXml(validated);
  const xmlMs = (performance.now() - t0) * 1000;

  console.log(`[1] Generated CEN SIRI v2.0 XML Message (latency: ${xmlMs.toFixed(2)}µs):`);
  console.log(xml.slice(0, 400) + "\n...\n");

  // 2. JSON Serialization
  const t1 = performance.now();
  const json = JSON.stringify(validated, null, 2);
  const jsonMs = (performance.now() - t1) * 1000;

  console.log(`[2] Generated Native JSON on Same Model (latency: ${jsonMs.toFixed(2)}µs):`);
  console.log(json.slice(0, 400) + "\n...\n");

  // 3. Inspection & Zod Schema Validation
  const mvj = validated.serviceDelivery.vehicleMonitoringDelivery.vehicleActivity[0].monitoredVehicleJourney;
  console.log("[3] TypeScript 5+ Model Inspection & Runtime Zod Schema Validation:");
  console.log(`    LineRef: ${mvj.lineRef}`);
  console.log(`    VehicleRef: ${mvj.vehicleRef}`);
  console.log(`    Coordinates: (${mvj.vehicleLocation.latitude}, ${mvj.vehicleLocation.longitude})`);
  console.log(`    Occupancy: ${mvj.occupancy}`);
  console.log("    Runtime Zod Schema Validation: PASS");

  console.log("\n✅ TypeScript 5+ GTFS-RT ↔ CEN SIRI Transit Bridge executed successfully!");
}

main();
