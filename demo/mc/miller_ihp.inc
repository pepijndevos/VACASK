// Miller OTA definition, IHP SG13G2 devices

subckt miller(inp inn out vdd vss)
  i0 (vdd net002) isource dc=ibias
  xmn3 (net002 net002 vss vss) sg13_lv_nmos w=mirr_w  l=mirr_l
  xmn4 (net003 net002 vss vss) sg13_lv_nmos w=mirr_wd l=mirr_ld
  xmn5 (out net002 vss vss)    sg13_lv_nmos w=mirr_wo l=mirr_l
  xmp1 (net005 net005 vdd vdd) sg13_lv_pmos w=load_w  l=load_l
  xmp2 (net004 net005 vdd vdd) sg13_lv_pmos w=load_w  l=load_l
  xmp3 (out net004 vdd vdd)    sg13_lv_pmos w=out_w   l=out_l
  r1 (out net001) resistor r=r_out
  c1 (net001 net004) capacitor c=c_out
  xmn1 (net005 inn net003 vss) sg13_lv_nmos w=diff_w  l=diff_l
  xmn2 (net004 inp net003 vss) sg13_lv_nmos w=diff_w  l=diff_l
ends
