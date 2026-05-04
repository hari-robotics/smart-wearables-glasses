import 'dart:typed_data';

const int kPacketStartByte = 123;
const int kPacketEndByte = 125;
const int kAsciiAccelerometerType = 0x41; // 'A'
const int kAsciiBioSensorsType = 0x42; // 'B'
const int kAsciiGyroscopeType = 0x47; // 'G'

class BioSensorReading {
  const BioSensorReading({
    this.heartRateBpm,
    this.spo2Percent,
    this.temperatureC,
  });

  final double? heartRateBpm;
  final double? spo2Percent;
  final double? temperatureC;
}

int? getPacketTypeCode(List<int> packet) {
  if (packet.isEmpty) {
    return null;
  }

  if (packet.length >= 3 &&
      packet.first == kPacketStartByte &&
      packet.last == kPacketEndByte) {
    return packet[1];
  }

  return packet.first;
}

bool isBioSensorPacket(List<int> packet) {
  final typeCode = getPacketTypeCode(packet);
  return typeCode != null && isBioSensorTypeCode(typeCode);
}

bool isBioSensorTypeCode(int typeCode) {
  return typeCode == kAsciiBioSensorsType;
}

String getSensorNameFromTypeCode(int? typeCode) {
  if (typeCode == kAsciiAccelerometerType) {
    return 'Accelerometer';
  }

  if (typeCode == kAsciiGyroscopeType) {
    return 'Gyroscope';
  }

  if (typeCode == kAsciiBioSensorsType) {
    return 'Bio Sensors (PPG)';
  }

  if (typeCode == null) {
    return 'Unknown';
  }

  final hexCode = typeCode.toRadixString(16).padLeft(2, '0').toUpperCase();
  return 'Unknown (0x$hexCode)';
}

List<int> getPacketPayload(List<int> packet) {
  if (packet.length >= 3 &&
      packet.first == kPacketStartByte &&
      packet.last == kPacketEndByte) {
    return packet.sublist(2, packet.length - 1);
  }

  if (packet.length >= 2) {
    return packet.sublist(1);
  }

  return const <int>[];
}

BioSensorReading? parseBioSensorPacket(List<int> packet) {
  if (!isBioSensorPacket(packet)) {
    return null;
  }

  final payload = getPacketPayload(packet);
  if (payload.length < 6) {
    return null;
  }

  final byteData = Uint8List.fromList(
    payload.sublist(0, 6),
  ).buffer.asByteData();

  final heartRateRaw = byteData.getInt16(0, Endian.big);
  final spo2Raw = byteData.getInt16(2, Endian.big);
  final temperatureRaw = byteData.getInt16(4, Endian.big);

  return BioSensorReading(
    heartRateBpm: _scaledInt16ToDoubleOrNull(heartRateRaw),
    spo2Percent: _scaledInt16ToDoubleOrNull(spo2Raw),
    temperatureC: _scaledInt16ToDoubleOrNull(temperatureRaw),
  );
}

double? _scaledInt16ToDoubleOrNull(int value) {
  if (value == 0) {
    return null;
  }

  return value / 10.0;
}
