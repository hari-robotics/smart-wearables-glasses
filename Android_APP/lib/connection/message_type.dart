enum MsgType {
  /* --------------------------------- general -------------------------------- */
  battery(0xb0),
  notWorn(0xb1),
  calibration(0xb2),
  frameNotActive(0xb3),
  frameMoving(0xb4),
  /* ----------------------------------- ppg ---------------------------------- */
  ppgIr(0xc0),
  // ppg_ir_filt(0xc1),
  ppgBeat(0xc2),
  /* ----------------------------------- ecg ---------------------------------- */
  ecg(0xd0),
  ecgR2r(0xd1),
  ecgStart(0xd2),
  ecgStop(0xd3),
  pat(0xd4),
  /* ----------------------------------- har ---------------------------------- */
  har(0xe0),
  /* ----------------------------------- end ---------------------------------- */
  end(0xff); // end tag

  final int description;
  const MsgType(this.description);
}
