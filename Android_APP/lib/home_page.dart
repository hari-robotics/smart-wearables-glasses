import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:smart_wearables_app/connection/stream.dart';
import 'package:smart_wearables_app/utils/sensor_utils.dart';
import 'package:syncfusion_flutter_charts/charts.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key, required this.title, required this.stream});

  final String title;
  final MyStream stream;

  @override
  State<HomePage> createState() => _HomePageState();
}

class ChartData {
  ChartData(this.x, this.y);

  final int x;
  final double y;
}

class LoggedBlePacket {
  LoggedBlePacket({required this.timestamp, required this.bytes});

  final DateTime timestamp;
  final List<int> bytes;
}

class _HomePageState extends State<HomePage> {
  static const List<String> _csvColumns = <String>[
    'timestamp_iso',
    'type_code_hex',
    'type_name',
    'packet_hex',
    'accelerometer_x_raw',
    'accelerometer_y_raw',
    'accelerometer_z_raw',
    'accelerometer_x_g',
    'accelerometer_y_g',
    'accelerometer_z_g',
    'gyroscope_x_raw',
    'gyroscope_y_raw',
    'gyroscope_z_raw',
    'gyroscope_x_dps',
    'gyroscope_y_dps',
    'gyroscope_z_dps',
    'heart_rate_bpm',
    'spo2_percent',
    'temperature_c',
  ];

  late final StreamSubscription<List<int>> _dataSubscription;
  late final StreamSubscription<List<int>> _rawDataSubscription;

  final List<ChartData> heartRateData = <ChartData>[];
  final List<ChartData> spo2Data = <ChartData>[];
  final List<ChartData> temperatureData = <ChartData>[];
  final List<LoggedBlePacket> _recordedPackets = <LoggedBlePacket>[];
  final int maxDataPoints = 50;

  int xCounter = 0;
  int? dataTypeCode;
  bool _isRecording = false;
  bool _isSaving = false;
  BioSensorReading latestReading = const BioSensorReading();

  @override
  void initState() {
    super.initState();
    _dataSubscription = widget.stream.controller.stream.listen(_parsePacket);
    _rawDataSubscription = widget.stream.controllerRaw.stream.listen(
      _recordRawPacket,
    );
  }

  void _parsePacket(List<int> packet) {
    final typeCode = getPacketTypeCode(packet);
    if (typeCode == null || !isBioSensorTypeCode(typeCode)) {
      debugPrint('Ignoring non-bio-sensor packet: $packet');
      return;
    }

    final reading = parseBioSensorPacket(packet);
    if (reading == null) {
      debugPrint('Bio-sensor packet does not contain valid data: $packet');
      return;
    }

    setState(() {
      dataTypeCode = typeCode;
      latestReading = reading;

      if (reading.heartRateBpm != null) {
        heartRateData.add(ChartData(xCounter, reading.heartRateBpm!));
      }
      if (reading.spo2Percent != null) {
        spo2Data.add(ChartData(xCounter, reading.spo2Percent!));
      }
      if (reading.temperatureC != null) {
        temperatureData.add(ChartData(xCounter, reading.temperatureC!));
      }

      xCounter++;

      _trimData(heartRateData);
      _trimData(spo2Data);
      _trimData(temperatureData);
    });

    debugPrint(
      'Parsed biosensors: HR=${reading.heartRateBpm}, SpO2=${reading.spo2Percent}, Temp=${reading.temperatureC}',
    );
  }

  void _trimData(List<ChartData> data) {
    final overflow = data.length - maxDataPoints;
    if (overflow > 0) {
      data.removeRange(0, overflow);
    }
  }

  void _recordRawPacket(List<int> packet) {
    if (!_isRecording) {
      return;
    }

    _recordedPackets.add(
      LoggedBlePacket(timestamp: DateTime.now(), bytes: List<int>.from(packet)),
    );
  }

  String _formatValue(double? value, String unit) {
    if (value == null) {
      return '--';
    }

    return '${value.toStringAsFixed(1)} $unit';
  }

  Future<void> _toggleRecording() async {
    if (_isSaving) {
      return;
    }

    if (_isRecording) {
      setState(() {
        _isRecording = false;
      });

      if (_recordedPackets.isEmpty) {
        _showMessage('No BLE data was recorded.');
        return;
      }

      await _saveRecordingFlow();
      return;
    }

    setState(() {
      _recordedPackets.clear();
      _isRecording = true;
    });
    _showMessage('Started recording BLE data.');
  }

  Future<void> _saveRecordingFlow() async {
    setState(() {
      _isSaving = true;
    });

    try {
      final fileName = await _promptForFileName();
      if (!mounted) {
        return;
      }
      if (fileName == null) {
        _showMessage('Save cancelled. The recording was not exported.');
        return;
      }

      await _waitForOverlayToClose();
      if (!mounted) {
        return;
      }

      final savedPath = await _saveRecordingWithPicker(fileName);
      if (!mounted) {
        return;
      }
      if (savedPath == null) {
        _showMessage('Save cancelled. The recording was not exported.');
        return;
      }

      final pathSuffix = savedPath.isEmpty ? '.' : ' to $savedPath';
      _showMessage('Saved BLE log$pathSuffix');
      _recordedPackets.clear();
    } catch (error) {
      if (mounted) {
        _showMessage('Failed to save BLE log: $error');
      }
    } finally {
      if (mounted) {
        setState(() {
          _isSaving = false;
        });
      }
    }
  }

  Future<String?> _promptForFileName() async {
    final controller = TextEditingController(text: _buildDefaultFileName());

    final result = await showDialog<String>(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext dialogContext) {
        return AlertDialog(
          title: const Text('Save BLE recording'),
          content: TextField(
            controller: controller,
            autofocus: true,
            decoration: const InputDecoration(
              labelText: 'File name',
              hintText: 'ble_log_20260511_103000.csv',
            ),
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () {
                final fileName = controller.text.trim();
                if (fileName.isEmpty) {
                  return;
                }
                Navigator.of(dialogContext).pop(fileName);
              },
              child: const Text('Confirm'),
            ),
          ],
        );
      },
    );

    await Future<void>.delayed(Duration.zero);
    await WidgetsBinding.instance.endOfFrame;
    controller.dispose();
    return result;
  }

  Future<void> _waitForOverlayToClose() async {
    FocusManager.instance.primaryFocus?.unfocus();
    await Future<void>.delayed(Duration.zero);
    await WidgetsBinding.instance.endOfFrame;
  }

  Future<String?> _saveRecordingWithPicker(String fileName) {
    final normalizedFileName = _normalizeFileName(fileName);
    final bytes = Uint8List.fromList(utf8.encode(_buildCsvContent()));

    return FilePicker.platform.saveFile(
      dialogTitle: 'Select where to save BLE log',
      fileName: normalizedFileName,
      type: FileType.custom,
      allowedExtensions: const <String>['csv'],
      bytes: bytes,
    );
  }

  String _buildCsvContent() {
    final buffer = StringBuffer();
    buffer.writeln(_csvColumns.map(_csvCell).join(','));

    for (final packet in _recordedPackets) {
      final row = _buildCsvRow(packet);
      buffer.writeln(
        _csvColumns
            .map((String column) => _csvCell(row[column] ?? ''))
            .join(','),
      );
    }

    return buffer.toString();
  }

  Map<String, String> _buildCsvRow(LoggedBlePacket packet) {
    final typeCode = getPacketTypeCode(packet.bytes);
    final typeCodeHex = typeCode == null
        ? ''
        : '0x${typeCode.toRadixString(16).padLeft(2, '0').toUpperCase()}';
    final packetHex = packet.bytes
        .map((int byte) => byte.toRadixString(16).padLeft(2, '0').toUpperCase())
        .join(' ');

    final row = <String, String>{
      'timestamp_iso': packet.timestamp.toIso8601String(),
      'type_code_hex': typeCodeHex,
      'type_name': getSensorNameFromTypeCode(typeCode),
      'packet_hex': packetHex,
    };

    if (typeCode == null) {
      return row;
    }

    if (isAccelerometerTypeCode(typeCode)) {
      final reading = parseAccelerometerPacket(packet.bytes);
      if (reading == null) {
        return row;
      }

      row.addAll(<String, String>{
        'accelerometer_x_raw': _formatOptionalInt(reading.xRaw),
        'accelerometer_y_raw': _formatOptionalInt(reading.yRaw),
        'accelerometer_z_raw': _formatOptionalInt(reading.zRaw),
        'accelerometer_x_g': _formatOptionalDouble(reading.xScaled, 6),
        'accelerometer_y_g': _formatOptionalDouble(reading.yScaled, 6),
        'accelerometer_z_g': _formatOptionalDouble(reading.zScaled, 6),
      });
      return row;
    }

    if (isGyroscopeTypeCode(typeCode)) {
      final reading = parseGyroscopePacket(packet.bytes);
      if (reading == null) {
        return row;
      }

      row.addAll(<String, String>{
        'gyroscope_x_raw': _formatOptionalInt(reading.xRaw),
        'gyroscope_y_raw': _formatOptionalInt(reading.yRaw),
        'gyroscope_z_raw': _formatOptionalInt(reading.zRaw),
        'gyroscope_x_dps': _formatOptionalDouble(reading.xScaled, 6),
        'gyroscope_y_dps': _formatOptionalDouble(reading.yScaled, 6),
        'gyroscope_z_dps': _formatOptionalDouble(reading.zScaled, 6),
      });
      return row;
    }

    if (isBioSensorTypeCode(typeCode)) {
      final reading = parseBioSensorPacket(packet.bytes);
      if (reading == null) {
        return row;
      }

      row.addAll(<String, String>{
        'heart_rate_bpm': _formatOptionalDouble(reading.heartRateBpm, 1),
        'spo2_percent': _formatOptionalDouble(reading.spo2Percent, 1),
        'temperature_c': _formatOptionalDouble(reading.temperatureC, 1),
      });
    }

    return row;
  }

  String _csvCell(String value) {
    final escapedValue = value.replaceAll('"', '""');
    return '"$escapedValue"';
  }

  String _formatOptionalInt(int? value) {
    return value?.toString() ?? '';
  }

  String _formatOptionalDouble(double? value, int fractionDigits) {
    if (value == null) {
      return '';
    }
    return value.toStringAsFixed(fractionDigits);
  }

  String _normalizeFileName(String input) {
    final sanitized = input.replaceAll(RegExp(r'[\\/:*?"<>|]'), '_').trim();
    final baseName = sanitized.isEmpty ? _buildDefaultFileName() : sanitized;
    if (baseName.toLowerCase().endsWith('.csv')) {
      return baseName;
    }
    return '$baseName.csv';
  }

  String _buildDefaultFileName() {
    final now = DateTime.now();
    final year = now.year.toString().padLeft(4, '0');
    final month = now.month.toString().padLeft(2, '0');
    final day = now.day.toString().padLeft(2, '0');
    final hour = now.hour.toString().padLeft(2, '0');
    final minute = now.minute.toString().padLeft(2, '0');
    final second = now.second.toString().padLeft(2, '0');
    return 'ble_log_$year$month${day}_$hour$minute$second.csv';
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  void dispose() {
    _dataSubscription.cancel();
    _rawDataSubscription.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: ListView(
        padding: const EdgeInsets.all(12),
        children: <Widget>[
          Text(
            'Sensor Type: ${getSensorNameFromTypeCode(dataTypeCode)}',
            style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 12),
          Row(
            children: <Widget>[
              Expanded(
                child: _buildMetricCard(
                  title: 'Heart Rate',
                  value: _formatValue(latestReading.heartRateBpm, 'bpm'),
                  color: Colors.red,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _buildMetricCard(
                  title: 'SpO2',
                  value: _formatValue(latestReading.spo2Percent, '%'),
                  color: Colors.blue,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _buildMetricCard(
            title: 'Temperature',
            value: _formatValue(latestReading.temperatureC, 'C'),
            color: Colors.orange,
          ),
          const SizedBox(height: 20),
          _buildChart(
            title: 'Heart Rate Trend',
            data: heartRateData,
            color: Colors.red,
            yAxisLabel: '{value} bpm',
          ),
          const SizedBox(height: 16),
          _buildChart(
            title: 'SpO2 Trend',
            data: spo2Data,
            color: Colors.blue,
            yAxisLabel: '{value}%',
          ),
          const SizedBox(height: 16),
          _buildChart(
            title: 'Temperature Trend',
            data: temperatureData,
            color: Colors.orange,
            yAxisLabel: '{value} C',
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _isSaving ? null : _toggleRecording,
        icon: Icon(
          _isSaving
              ? Icons.save_outlined
              : _isRecording
              ? Icons.stop_circle_outlined
              : Icons.fiber_manual_record,
        ),
        label: Text(
          _isSaving
              ? 'Saving...'
              : _isRecording
              ? 'Stop Recording'
              : 'Record BLE',
        ),
        backgroundColor: _isRecording ? Colors.red : null,
      ),
    );
  }

  Widget _buildMetricCard({
    required String title,
    required String value,
    required Color color,
  }) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              title,
              style: TextStyle(
                color: color,
                fontSize: 14,
                fontWeight: FontWeight.w600,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              value,
              style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildChart({
    required String title,
    required List<ChartData> data,
    required Color color,
    required String yAxisLabel,
  }) {
    final chartData = List<ChartData>.of(data);

    return SizedBox(
      height: 220,
      child: Card(
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            children: <Widget>[
              Text(title, style: const TextStyle(fontWeight: FontWeight.bold)),
              const SizedBox(height: 8),
              Expanded(
                child: SfCartesianChart(
                  primaryXAxis: NumericAxis(
                    autoScrollingMode: AutoScrollingMode.end,
                    autoScrollingDelta: maxDataPoints,
                    isVisible: false,
                  ),
                  primaryYAxis: NumericAxis(labelFormat: yAxisLabel),
                  series: <LineSeries<ChartData, int>>[
                    LineSeries<ChartData, int>(
                      dataSource: chartData,
                      xValueMapper: (ChartData point, _) => point.x,
                      yValueMapper: (ChartData point, _) => point.y,
                      color: color,
                      width: 2,
                      animationDuration: 0,
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
