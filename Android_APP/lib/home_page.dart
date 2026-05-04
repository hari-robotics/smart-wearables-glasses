import 'dart:async';

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

class _HomePageState extends State<HomePage> {
  late final StreamSubscription<List<int>> _dataSubscription;

  final List<ChartData> heartRateData = <ChartData>[];
  final List<ChartData> spo2Data = <ChartData>[];
  final List<ChartData> temperatureData = <ChartData>[];
  final int maxDataPoints = 50;

  int xCounter = 0;
  int? dataTypeCode;
  BioSensorReading latestReading = const BioSensorReading();

  @override
  void initState() {
    super.initState();
    _dataSubscription = widget.stream.controller.stream.listen(_parsePacket);
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

  String _formatValue(double? value, String unit) {
    if (value == null) {
      return '--';
    }

    return '${value.toStringAsFixed(1)} $unit';
  }

  @override
  void dispose() {
    _dataSubscription.cancel();
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
