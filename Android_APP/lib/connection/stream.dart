import 'dart:async';

import 'package:smart_wearables_app/connection/message_type.dart';
import 'package:smart_wearables_app/utils/sensor_utils.dart';

class MyStream {
  final StreamController<List<int>> controller =
      StreamController<List<int>>.broadcast();
  final StreamController<List<int>> controllerBattery =
      StreamController<List<int>>.broadcast();
  final StreamController<List<int>> controllerRaw =
      StreamController<List<int>>.broadcast();
  final StreamController<List<int>> controllerSend =
      StreamController<List<int>>.broadcast();

  void addRawPacket(List<int> data) {
    controllerRaw.add(List<int>.unmodifiable(data));
  }

  void setNum(List<int> data) {
    final typeCode = getPacketTypeCode(data);

    if (typeCode == MsgType.battery.description) {
      controllerBattery.add(data);
    } else {
      controller.add(data);
    }
  }

  void sendData(List<int> data) {
    controllerSend.add(data);
  }
}
