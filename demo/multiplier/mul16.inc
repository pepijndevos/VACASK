
subckt not ( in out vdd vss )
  mp (out in vdd vdd) pmos w=1u l=0.2u
  mn (out in vss vss) nmos w=0.5u l=0.2u
ends

subckt nor2 ( in1 in2 out vdd vss )
  mp2 (int in2 vdd vdd) pmos w=1u l=0.2u
  mp1 (out in1 int vdd) pmos w=1u l=0.2u
  mn1 (out in1 vss vss) nmos w=0.5u l=0.2u
  mn2 (out in2 vss vss) nmos w=0.5u l=0.2u
ends

subckt and2 ( in1 in2 out vdd vss )
  mp2 (outx in2 vdd vdd) pmos w=1u l=0.2u
  mp1 (outx in1 vdd vdd) pmos w=1u l=0.2u
  mn1 (outx in1 int vss) nmos w=0.5u l=0.2u
  mn2 (int  in2 vss vss) nmos w=0.5u l=0.2u
  mp3 (out outx vdd vdd) pmos w=1u l=0.2u
  mn3 (out outx vss vss) nmos w=0.5u l=0.2u
ends

subckt drv(out)
  parameters v0=0 v1=1
  parameters rs=1 delay=0.1n rise=0.1n
  vdrv (int 0) v type="pulse" val0=v0*vdd val1=v1*vdd delay=delay rise=rise
  rdrv (int out) r r=rs
ends

subckt fa ( in1 in2 cin sout cout vdd vss )
  x1 ( in1 cin o1 vdd vss ) nor2
  x2 ( in1 o1 o2 vdd vss ) nor2
  x3 ( o1 cin o3 vdd vss ) nor2
  x4 ( o2 o3 o4 vdd vss ) nor2
  x5 ( in2 o4 o5 vdd vss ) nor2
  x6 ( in2 o5 o6 vdd vss ) nor2
  x7 ( o5 o4 o7 vdd vss ) nor2
  x8 ( o6 o7 sout vdd vss ) nor2
  x9 ( o5 o1 cout vdd vss ) nor2
ends

subckt ha ( in1 in2 sout cout vdd vss )
  x1 ( in1 in1n vdd vss ) not
  x5 ( in2 in1n o5 vdd vss ) nor2
  x6 ( in2 o5 o6 vdd vss ) nor2
  x7 ( o5 in1n cout vdd vss ) nor2
  x8 ( o6 cout sout vdd vss ) nor2
ends

subckt mul16 ( a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12 b13 b14 b15 p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15 p16 p17 p18 p19 p20 p21 p22 p23 p24 p25 p26 p27 p28 p29 p30 p31 vdd vss )
  // Partial products
  xpp_0_0 ( b0 a0 p0 vdd vss ) and2
  xpp_0_1 ( b0 a1 pp_0_1 vdd vss ) and2
  xpp_0_2 ( b0 a2 pp_0_2 vdd vss ) and2
  xpp_0_3 ( b0 a3 pp_0_3 vdd vss ) and2
  xpp_0_4 ( b0 a4 pp_0_4 vdd vss ) and2
  xpp_0_5 ( b0 a5 pp_0_5 vdd vss ) and2
  xpp_0_6 ( b0 a6 pp_0_6 vdd vss ) and2
  xpp_0_7 ( b0 a7 pp_0_7 vdd vss ) and2
  xpp_0_8 ( b0 a8 pp_0_8 vdd vss ) and2
  xpp_0_9 ( b0 a9 pp_0_9 vdd vss ) and2
  xpp_0_10 ( b0 a10 pp_0_10 vdd vss ) and2
  xpp_0_11 ( b0 a11 pp_0_11 vdd vss ) and2
  xpp_0_12 ( b0 a12 pp_0_12 vdd vss ) and2
  xpp_0_13 ( b0 a13 pp_0_13 vdd vss ) and2
  xpp_0_14 ( b0 a14 pp_0_14 vdd vss ) and2
  xpp_0_15 ( b0 a15 pp_0_15 vdd vss ) and2
  xpp_1_0 ( b1 a0 pp_1_0 vdd vss ) and2
  xpp_1_1 ( b1 a1 pp_1_1 vdd vss ) and2
  xpp_1_2 ( b1 a2 pp_1_2 vdd vss ) and2
  xpp_1_3 ( b1 a3 pp_1_3 vdd vss ) and2
  xpp_1_4 ( b1 a4 pp_1_4 vdd vss ) and2
  xpp_1_5 ( b1 a5 pp_1_5 vdd vss ) and2
  xpp_1_6 ( b1 a6 pp_1_6 vdd vss ) and2
  xpp_1_7 ( b1 a7 pp_1_7 vdd vss ) and2
  xpp_1_8 ( b1 a8 pp_1_8 vdd vss ) and2
  xpp_1_9 ( b1 a9 pp_1_9 vdd vss ) and2
  xpp_1_10 ( b1 a10 pp_1_10 vdd vss ) and2
  xpp_1_11 ( b1 a11 pp_1_11 vdd vss ) and2
  xpp_1_12 ( b1 a12 pp_1_12 vdd vss ) and2
  xpp_1_13 ( b1 a13 pp_1_13 vdd vss ) and2
  xpp_1_14 ( b1 a14 pp_1_14 vdd vss ) and2
  xpp_1_15 ( b1 a15 pp_1_15 vdd vss ) and2
  xpp_2_0 ( b2 a0 pp_2_0 vdd vss ) and2
  xpp_2_1 ( b2 a1 pp_2_1 vdd vss ) and2
  xpp_2_2 ( b2 a2 pp_2_2 vdd vss ) and2
  xpp_2_3 ( b2 a3 pp_2_3 vdd vss ) and2
  xpp_2_4 ( b2 a4 pp_2_4 vdd vss ) and2
  xpp_2_5 ( b2 a5 pp_2_5 vdd vss ) and2
  xpp_2_6 ( b2 a6 pp_2_6 vdd vss ) and2
  xpp_2_7 ( b2 a7 pp_2_7 vdd vss ) and2
  xpp_2_8 ( b2 a8 pp_2_8 vdd vss ) and2
  xpp_2_9 ( b2 a9 pp_2_9 vdd vss ) and2
  xpp_2_10 ( b2 a10 pp_2_10 vdd vss ) and2
  xpp_2_11 ( b2 a11 pp_2_11 vdd vss ) and2
  xpp_2_12 ( b2 a12 pp_2_12 vdd vss ) and2
  xpp_2_13 ( b2 a13 pp_2_13 vdd vss ) and2
  xpp_2_14 ( b2 a14 pp_2_14 vdd vss ) and2
  xpp_2_15 ( b2 a15 pp_2_15 vdd vss ) and2
  xpp_3_0 ( b3 a0 pp_3_0 vdd vss ) and2
  xpp_3_1 ( b3 a1 pp_3_1 vdd vss ) and2
  xpp_3_2 ( b3 a2 pp_3_2 vdd vss ) and2
  xpp_3_3 ( b3 a3 pp_3_3 vdd vss ) and2
  xpp_3_4 ( b3 a4 pp_3_4 vdd vss ) and2
  xpp_3_5 ( b3 a5 pp_3_5 vdd vss ) and2
  xpp_3_6 ( b3 a6 pp_3_6 vdd vss ) and2
  xpp_3_7 ( b3 a7 pp_3_7 vdd vss ) and2
  xpp_3_8 ( b3 a8 pp_3_8 vdd vss ) and2
  xpp_3_9 ( b3 a9 pp_3_9 vdd vss ) and2
  xpp_3_10 ( b3 a10 pp_3_10 vdd vss ) and2
  xpp_3_11 ( b3 a11 pp_3_11 vdd vss ) and2
  xpp_3_12 ( b3 a12 pp_3_12 vdd vss ) and2
  xpp_3_13 ( b3 a13 pp_3_13 vdd vss ) and2
  xpp_3_14 ( b3 a14 pp_3_14 vdd vss ) and2
  xpp_3_15 ( b3 a15 pp_3_15 vdd vss ) and2
  xpp_4_0 ( b4 a0 pp_4_0 vdd vss ) and2
  xpp_4_1 ( b4 a1 pp_4_1 vdd vss ) and2
  xpp_4_2 ( b4 a2 pp_4_2 vdd vss ) and2
  xpp_4_3 ( b4 a3 pp_4_3 vdd vss ) and2
  xpp_4_4 ( b4 a4 pp_4_4 vdd vss ) and2
  xpp_4_5 ( b4 a5 pp_4_5 vdd vss ) and2
  xpp_4_6 ( b4 a6 pp_4_6 vdd vss ) and2
  xpp_4_7 ( b4 a7 pp_4_7 vdd vss ) and2
  xpp_4_8 ( b4 a8 pp_4_8 vdd vss ) and2
  xpp_4_9 ( b4 a9 pp_4_9 vdd vss ) and2
  xpp_4_10 ( b4 a10 pp_4_10 vdd vss ) and2
  xpp_4_11 ( b4 a11 pp_4_11 vdd vss ) and2
  xpp_4_12 ( b4 a12 pp_4_12 vdd vss ) and2
  xpp_4_13 ( b4 a13 pp_4_13 vdd vss ) and2
  xpp_4_14 ( b4 a14 pp_4_14 vdd vss ) and2
  xpp_4_15 ( b4 a15 pp_4_15 vdd vss ) and2
  xpp_5_0 ( b5 a0 pp_5_0 vdd vss ) and2
  xpp_5_1 ( b5 a1 pp_5_1 vdd vss ) and2
  xpp_5_2 ( b5 a2 pp_5_2 vdd vss ) and2
  xpp_5_3 ( b5 a3 pp_5_3 vdd vss ) and2
  xpp_5_4 ( b5 a4 pp_5_4 vdd vss ) and2
  xpp_5_5 ( b5 a5 pp_5_5 vdd vss ) and2
  xpp_5_6 ( b5 a6 pp_5_6 vdd vss ) and2
  xpp_5_7 ( b5 a7 pp_5_7 vdd vss ) and2
  xpp_5_8 ( b5 a8 pp_5_8 vdd vss ) and2
  xpp_5_9 ( b5 a9 pp_5_9 vdd vss ) and2
  xpp_5_10 ( b5 a10 pp_5_10 vdd vss ) and2
  xpp_5_11 ( b5 a11 pp_5_11 vdd vss ) and2
  xpp_5_12 ( b5 a12 pp_5_12 vdd vss ) and2
  xpp_5_13 ( b5 a13 pp_5_13 vdd vss ) and2
  xpp_5_14 ( b5 a14 pp_5_14 vdd vss ) and2
  xpp_5_15 ( b5 a15 pp_5_15 vdd vss ) and2
  xpp_6_0 ( b6 a0 pp_6_0 vdd vss ) and2
  xpp_6_1 ( b6 a1 pp_6_1 vdd vss ) and2
  xpp_6_2 ( b6 a2 pp_6_2 vdd vss ) and2
  xpp_6_3 ( b6 a3 pp_6_3 vdd vss ) and2
  xpp_6_4 ( b6 a4 pp_6_4 vdd vss ) and2
  xpp_6_5 ( b6 a5 pp_6_5 vdd vss ) and2
  xpp_6_6 ( b6 a6 pp_6_6 vdd vss ) and2
  xpp_6_7 ( b6 a7 pp_6_7 vdd vss ) and2
  xpp_6_8 ( b6 a8 pp_6_8 vdd vss ) and2
  xpp_6_9 ( b6 a9 pp_6_9 vdd vss ) and2
  xpp_6_10 ( b6 a10 pp_6_10 vdd vss ) and2
  xpp_6_11 ( b6 a11 pp_6_11 vdd vss ) and2
  xpp_6_12 ( b6 a12 pp_6_12 vdd vss ) and2
  xpp_6_13 ( b6 a13 pp_6_13 vdd vss ) and2
  xpp_6_14 ( b6 a14 pp_6_14 vdd vss ) and2
  xpp_6_15 ( b6 a15 pp_6_15 vdd vss ) and2
  xpp_7_0 ( b7 a0 pp_7_0 vdd vss ) and2
  xpp_7_1 ( b7 a1 pp_7_1 vdd vss ) and2
  xpp_7_2 ( b7 a2 pp_7_2 vdd vss ) and2
  xpp_7_3 ( b7 a3 pp_7_3 vdd vss ) and2
  xpp_7_4 ( b7 a4 pp_7_4 vdd vss ) and2
  xpp_7_5 ( b7 a5 pp_7_5 vdd vss ) and2
  xpp_7_6 ( b7 a6 pp_7_6 vdd vss ) and2
  xpp_7_7 ( b7 a7 pp_7_7 vdd vss ) and2
  xpp_7_8 ( b7 a8 pp_7_8 vdd vss ) and2
  xpp_7_9 ( b7 a9 pp_7_9 vdd vss ) and2
  xpp_7_10 ( b7 a10 pp_7_10 vdd vss ) and2
  xpp_7_11 ( b7 a11 pp_7_11 vdd vss ) and2
  xpp_7_12 ( b7 a12 pp_7_12 vdd vss ) and2
  xpp_7_13 ( b7 a13 pp_7_13 vdd vss ) and2
  xpp_7_14 ( b7 a14 pp_7_14 vdd vss ) and2
  xpp_7_15 ( b7 a15 pp_7_15 vdd vss ) and2
  xpp_8_0 ( b8 a0 pp_8_0 vdd vss ) and2
  xpp_8_1 ( b8 a1 pp_8_1 vdd vss ) and2
  xpp_8_2 ( b8 a2 pp_8_2 vdd vss ) and2
  xpp_8_3 ( b8 a3 pp_8_3 vdd vss ) and2
  xpp_8_4 ( b8 a4 pp_8_4 vdd vss ) and2
  xpp_8_5 ( b8 a5 pp_8_5 vdd vss ) and2
  xpp_8_6 ( b8 a6 pp_8_6 vdd vss ) and2
  xpp_8_7 ( b8 a7 pp_8_7 vdd vss ) and2
  xpp_8_8 ( b8 a8 pp_8_8 vdd vss ) and2
  xpp_8_9 ( b8 a9 pp_8_9 vdd vss ) and2
  xpp_8_10 ( b8 a10 pp_8_10 vdd vss ) and2
  xpp_8_11 ( b8 a11 pp_8_11 vdd vss ) and2
  xpp_8_12 ( b8 a12 pp_8_12 vdd vss ) and2
  xpp_8_13 ( b8 a13 pp_8_13 vdd vss ) and2
  xpp_8_14 ( b8 a14 pp_8_14 vdd vss ) and2
  xpp_8_15 ( b8 a15 pp_8_15 vdd vss ) and2
  xpp_9_0 ( b9 a0 pp_9_0 vdd vss ) and2
  xpp_9_1 ( b9 a1 pp_9_1 vdd vss ) and2
  xpp_9_2 ( b9 a2 pp_9_2 vdd vss ) and2
  xpp_9_3 ( b9 a3 pp_9_3 vdd vss ) and2
  xpp_9_4 ( b9 a4 pp_9_4 vdd vss ) and2
  xpp_9_5 ( b9 a5 pp_9_5 vdd vss ) and2
  xpp_9_6 ( b9 a6 pp_9_6 vdd vss ) and2
  xpp_9_7 ( b9 a7 pp_9_7 vdd vss ) and2
  xpp_9_8 ( b9 a8 pp_9_8 vdd vss ) and2
  xpp_9_9 ( b9 a9 pp_9_9 vdd vss ) and2
  xpp_9_10 ( b9 a10 pp_9_10 vdd vss ) and2
  xpp_9_11 ( b9 a11 pp_9_11 vdd vss ) and2
  xpp_9_12 ( b9 a12 pp_9_12 vdd vss ) and2
  xpp_9_13 ( b9 a13 pp_9_13 vdd vss ) and2
  xpp_9_14 ( b9 a14 pp_9_14 vdd vss ) and2
  xpp_9_15 ( b9 a15 pp_9_15 vdd vss ) and2
  xpp_10_0 ( b10 a0 pp_10_0 vdd vss ) and2
  xpp_10_1 ( b10 a1 pp_10_1 vdd vss ) and2
  xpp_10_2 ( b10 a2 pp_10_2 vdd vss ) and2
  xpp_10_3 ( b10 a3 pp_10_3 vdd vss ) and2
  xpp_10_4 ( b10 a4 pp_10_4 vdd vss ) and2
  xpp_10_5 ( b10 a5 pp_10_5 vdd vss ) and2
  xpp_10_6 ( b10 a6 pp_10_6 vdd vss ) and2
  xpp_10_7 ( b10 a7 pp_10_7 vdd vss ) and2
  xpp_10_8 ( b10 a8 pp_10_8 vdd vss ) and2
  xpp_10_9 ( b10 a9 pp_10_9 vdd vss ) and2
  xpp_10_10 ( b10 a10 pp_10_10 vdd vss ) and2
  xpp_10_11 ( b10 a11 pp_10_11 vdd vss ) and2
  xpp_10_12 ( b10 a12 pp_10_12 vdd vss ) and2
  xpp_10_13 ( b10 a13 pp_10_13 vdd vss ) and2
  xpp_10_14 ( b10 a14 pp_10_14 vdd vss ) and2
  xpp_10_15 ( b10 a15 pp_10_15 vdd vss ) and2
  xpp_11_0 ( b11 a0 pp_11_0 vdd vss ) and2
  xpp_11_1 ( b11 a1 pp_11_1 vdd vss ) and2
  xpp_11_2 ( b11 a2 pp_11_2 vdd vss ) and2
  xpp_11_3 ( b11 a3 pp_11_3 vdd vss ) and2
  xpp_11_4 ( b11 a4 pp_11_4 vdd vss ) and2
  xpp_11_5 ( b11 a5 pp_11_5 vdd vss ) and2
  xpp_11_6 ( b11 a6 pp_11_6 vdd vss ) and2
  xpp_11_7 ( b11 a7 pp_11_7 vdd vss ) and2
  xpp_11_8 ( b11 a8 pp_11_8 vdd vss ) and2
  xpp_11_9 ( b11 a9 pp_11_9 vdd vss ) and2
  xpp_11_10 ( b11 a10 pp_11_10 vdd vss ) and2
  xpp_11_11 ( b11 a11 pp_11_11 vdd vss ) and2
  xpp_11_12 ( b11 a12 pp_11_12 vdd vss ) and2
  xpp_11_13 ( b11 a13 pp_11_13 vdd vss ) and2
  xpp_11_14 ( b11 a14 pp_11_14 vdd vss ) and2
  xpp_11_15 ( b11 a15 pp_11_15 vdd vss ) and2
  xpp_12_0 ( b12 a0 pp_12_0 vdd vss ) and2
  xpp_12_1 ( b12 a1 pp_12_1 vdd vss ) and2
  xpp_12_2 ( b12 a2 pp_12_2 vdd vss ) and2
  xpp_12_3 ( b12 a3 pp_12_3 vdd vss ) and2
  xpp_12_4 ( b12 a4 pp_12_4 vdd vss ) and2
  xpp_12_5 ( b12 a5 pp_12_5 vdd vss ) and2
  xpp_12_6 ( b12 a6 pp_12_6 vdd vss ) and2
  xpp_12_7 ( b12 a7 pp_12_7 vdd vss ) and2
  xpp_12_8 ( b12 a8 pp_12_8 vdd vss ) and2
  xpp_12_9 ( b12 a9 pp_12_9 vdd vss ) and2
  xpp_12_10 ( b12 a10 pp_12_10 vdd vss ) and2
  xpp_12_11 ( b12 a11 pp_12_11 vdd vss ) and2
  xpp_12_12 ( b12 a12 pp_12_12 vdd vss ) and2
  xpp_12_13 ( b12 a13 pp_12_13 vdd vss ) and2
  xpp_12_14 ( b12 a14 pp_12_14 vdd vss ) and2
  xpp_12_15 ( b12 a15 pp_12_15 vdd vss ) and2
  xpp_13_0 ( b13 a0 pp_13_0 vdd vss ) and2
  xpp_13_1 ( b13 a1 pp_13_1 vdd vss ) and2
  xpp_13_2 ( b13 a2 pp_13_2 vdd vss ) and2
  xpp_13_3 ( b13 a3 pp_13_3 vdd vss ) and2
  xpp_13_4 ( b13 a4 pp_13_4 vdd vss ) and2
  xpp_13_5 ( b13 a5 pp_13_5 vdd vss ) and2
  xpp_13_6 ( b13 a6 pp_13_6 vdd vss ) and2
  xpp_13_7 ( b13 a7 pp_13_7 vdd vss ) and2
  xpp_13_8 ( b13 a8 pp_13_8 vdd vss ) and2
  xpp_13_9 ( b13 a9 pp_13_9 vdd vss ) and2
  xpp_13_10 ( b13 a10 pp_13_10 vdd vss ) and2
  xpp_13_11 ( b13 a11 pp_13_11 vdd vss ) and2
  xpp_13_12 ( b13 a12 pp_13_12 vdd vss ) and2
  xpp_13_13 ( b13 a13 pp_13_13 vdd vss ) and2
  xpp_13_14 ( b13 a14 pp_13_14 vdd vss ) and2
  xpp_13_15 ( b13 a15 pp_13_15 vdd vss ) and2
  xpp_14_0 ( b14 a0 pp_14_0 vdd vss ) and2
  xpp_14_1 ( b14 a1 pp_14_1 vdd vss ) and2
  xpp_14_2 ( b14 a2 pp_14_2 vdd vss ) and2
  xpp_14_3 ( b14 a3 pp_14_3 vdd vss ) and2
  xpp_14_4 ( b14 a4 pp_14_4 vdd vss ) and2
  xpp_14_5 ( b14 a5 pp_14_5 vdd vss ) and2
  xpp_14_6 ( b14 a6 pp_14_6 vdd vss ) and2
  xpp_14_7 ( b14 a7 pp_14_7 vdd vss ) and2
  xpp_14_8 ( b14 a8 pp_14_8 vdd vss ) and2
  xpp_14_9 ( b14 a9 pp_14_9 vdd vss ) and2
  xpp_14_10 ( b14 a10 pp_14_10 vdd vss ) and2
  xpp_14_11 ( b14 a11 pp_14_11 vdd vss ) and2
  xpp_14_12 ( b14 a12 pp_14_12 vdd vss ) and2
  xpp_14_13 ( b14 a13 pp_14_13 vdd vss ) and2
  xpp_14_14 ( b14 a14 pp_14_14 vdd vss ) and2
  xpp_14_15 ( b14 a15 pp_14_15 vdd vss ) and2
  xpp_15_0 ( b15 a0 pp_15_0 vdd vss ) and2
  xpp_15_1 ( b15 a1 pp_15_1 vdd vss ) and2
  xpp_15_2 ( b15 a2 pp_15_2 vdd vss ) and2
  xpp_15_3 ( b15 a3 pp_15_3 vdd vss ) and2
  xpp_15_4 ( b15 a4 pp_15_4 vdd vss ) and2
  xpp_15_5 ( b15 a5 pp_15_5 vdd vss ) and2
  xpp_15_6 ( b15 a6 pp_15_6 vdd vss ) and2
  xpp_15_7 ( b15 a7 pp_15_7 vdd vss ) and2
  xpp_15_8 ( b15 a8 pp_15_8 vdd vss ) and2
  xpp_15_9 ( b15 a9 pp_15_9 vdd vss ) and2
  xpp_15_10 ( b15 a10 pp_15_10 vdd vss ) and2
  xpp_15_11 ( b15 a11 pp_15_11 vdd vss ) and2
  xpp_15_12 ( b15 a12 pp_15_12 vdd vss ) and2
  xpp_15_13 ( b15 a13 pp_15_13 vdd vss ) and2
  xpp_15_14 ( b15 a14 pp_15_14 vdd vss ) and2
  xpp_15_15 ( b15 a15 pp_15_15 vdd vss ) and2
  
  // Row 0 (half adders)
  xha_0_1 ( pp_0_1 pp_1_0 p1 c_0_1 vdd vss ) ha
  xha_0_2 ( pp_0_2 pp_1_1 s_0_2 c_0_2 vdd vss ) ha
  xha_0_3 ( pp_0_3 pp_1_2 s_0_3 c_0_3 vdd vss ) ha
  xha_0_4 ( pp_0_4 pp_1_3 s_0_4 c_0_4 vdd vss ) ha
  xha_0_5 ( pp_0_5 pp_1_4 s_0_5 c_0_5 vdd vss ) ha
  xha_0_6 ( pp_0_6 pp_1_5 s_0_6 c_0_6 vdd vss ) ha
  xha_0_7 ( pp_0_7 pp_1_6 s_0_7 c_0_7 vdd vss ) ha
  xha_0_8 ( pp_0_8 pp_1_7 s_0_8 c_0_8 vdd vss ) ha
  xha_0_9 ( pp_0_9 pp_1_8 s_0_9 c_0_9 vdd vss ) ha
  xha_0_10 ( pp_0_10 pp_1_9 s_0_10 c_0_10 vdd vss ) ha
  xha_0_11 ( pp_0_11 pp_1_10 s_0_11 c_0_11 vdd vss ) ha
  xha_0_12 ( pp_0_12 pp_1_11 s_0_12 c_0_12 vdd vss ) ha
  xha_0_13 ( pp_0_13 pp_1_12 s_0_13 c_0_13 vdd vss ) ha
  xha_0_14 ( pp_0_14 pp_1_13 s_0_14 c_0_14 vdd vss ) ha
  xha_0_15 ( pp_0_15 pp_1_14 s_0_15 c_0_15 vdd vss ) ha
  
  // Row 1
  xfa_1_1 ( c_0_1 s_0_2 pp_2_1 p2 c_1_1 vdd vss ) fa
  xfa_1_2 ( c_0_2 s_0_3 pp_2_2 s_1_2 c_1_2 vdd vss ) fa
  xfa_1_3 ( c_0_3 s_0_4 pp_2_3 s_1_3 c_1_3 vdd vss ) fa
  xfa_1_4 ( c_0_4 s_0_5 pp_2_4 s_1_4 c_1_4 vdd vss ) fa
  xfa_1_5 ( c_0_5 s_0_6 pp_2_5 s_1_5 c_1_5 vdd vss ) fa
  xfa_1_6 ( c_0_6 s_0_7 pp_2_6 s_1_6 c_1_6 vdd vss ) fa
  xfa_1_7 ( c_0_7 s_0_8 pp_2_7 s_1_7 c_1_7 vdd vss ) fa
  xfa_1_8 ( c_0_8 s_0_9 pp_2_8 s_1_8 c_1_8 vdd vss ) fa
  xfa_1_9 ( c_0_9 s_0_10 pp_2_9 s_1_9 c_1_9 vdd vss ) fa
  xfa_1_10 ( c_0_10 s_0_11 pp_2_10 s_1_10 c_1_10 vdd vss ) fa
  xfa_1_11 ( c_0_11 s_0_12 pp_2_11 s_1_11 c_1_11 vdd vss ) fa
  xfa_1_12 ( c_0_12 s_0_13 pp_2_12 s_1_12 c_1_12 vdd vss ) fa
  xfa_1_13 ( c_0_13 s_0_14 pp_2_13 s_1_13 c_1_13 vdd vss ) fa
  xfa_1_14 ( c_0_14 s_0_15 pp_2_14 s_1_14 c_1_14 vdd vss ) fa
  xfa_1_15 ( c_0_15 pp_1_15 pp_2_15 s_1_15 c_1_15 vdd vss ) fa
  
  // Row 2
  xfa_2_1 ( c_1_1 s_1_2 pp_3_1 p3 c_2_1 vdd vss ) fa
  xfa_2_2 ( c_1_2 s_1_3 pp_3_2 s_2_2 c_2_2 vdd vss ) fa
  xfa_2_3 ( c_1_3 s_1_4 pp_3_3 s_2_3 c_2_3 vdd vss ) fa
  xfa_2_4 ( c_1_4 s_1_5 pp_3_4 s_2_4 c_2_4 vdd vss ) fa
  xfa_2_5 ( c_1_5 s_1_6 pp_3_5 s_2_5 c_2_5 vdd vss ) fa
  xfa_2_6 ( c_1_6 s_1_7 pp_3_6 s_2_6 c_2_6 vdd vss ) fa
  xfa_2_7 ( c_1_7 s_1_8 pp_3_7 s_2_7 c_2_7 vdd vss ) fa
  xfa_2_8 ( c_1_8 s_1_9 pp_3_8 s_2_8 c_2_8 vdd vss ) fa
  xfa_2_9 ( c_1_9 s_1_10 pp_3_9 s_2_9 c_2_9 vdd vss ) fa
  xfa_2_10 ( c_1_10 s_1_11 pp_3_10 s_2_10 c_2_10 vdd vss ) fa
  xfa_2_11 ( c_1_11 s_1_12 pp_3_11 s_2_11 c_2_11 vdd vss ) fa
  xfa_2_12 ( c_1_12 s_1_13 pp_3_12 s_2_12 c_2_12 vdd vss ) fa
  xfa_2_13 ( c_1_13 s_1_14 pp_3_13 s_2_13 c_2_13 vdd vss ) fa
  xfa_2_14 ( c_1_14 s_1_15 pp_3_14 s_2_14 c_2_14 vdd vss ) fa
  xfa_2_15 ( c_1_15 pp_2_15 pp_3_15 s_2_15 c_2_15 vdd vss ) fa
  
  // Row 3
  xfa_3_1 ( c_2_1 s_2_2 pp_4_1 p4 c_3_1 vdd vss ) fa
  xfa_3_2 ( c_2_2 s_2_3 pp_4_2 s_3_2 c_3_2 vdd vss ) fa
  xfa_3_3 ( c_2_3 s_2_4 pp_4_3 s_3_3 c_3_3 vdd vss ) fa
  xfa_3_4 ( c_2_4 s_2_5 pp_4_4 s_3_4 c_3_4 vdd vss ) fa
  xfa_3_5 ( c_2_5 s_2_6 pp_4_5 s_3_5 c_3_5 vdd vss ) fa
  xfa_3_6 ( c_2_6 s_2_7 pp_4_6 s_3_6 c_3_6 vdd vss ) fa
  xfa_3_7 ( c_2_7 s_2_8 pp_4_7 s_3_7 c_3_7 vdd vss ) fa
  xfa_3_8 ( c_2_8 s_2_9 pp_4_8 s_3_8 c_3_8 vdd vss ) fa
  xfa_3_9 ( c_2_9 s_2_10 pp_4_9 s_3_9 c_3_9 vdd vss ) fa
  xfa_3_10 ( c_2_10 s_2_11 pp_4_10 s_3_10 c_3_10 vdd vss ) fa
  xfa_3_11 ( c_2_11 s_2_12 pp_4_11 s_3_11 c_3_11 vdd vss ) fa
  xfa_3_12 ( c_2_12 s_2_13 pp_4_12 s_3_12 c_3_12 vdd vss ) fa
  xfa_3_13 ( c_2_13 s_2_14 pp_4_13 s_3_13 c_3_13 vdd vss ) fa
  xfa_3_14 ( c_2_14 s_2_15 pp_4_14 s_3_14 c_3_14 vdd vss ) fa
  xfa_3_15 ( c_2_15 pp_3_15 pp_4_15 s_3_15 c_3_15 vdd vss ) fa
  
  // Row 4
  xfa_4_1 ( c_3_1 s_3_2 pp_5_1 p5 c_4_1 vdd vss ) fa
  xfa_4_2 ( c_3_2 s_3_3 pp_5_2 s_4_2 c_4_2 vdd vss ) fa
  xfa_4_3 ( c_3_3 s_3_4 pp_5_3 s_4_3 c_4_3 vdd vss ) fa
  xfa_4_4 ( c_3_4 s_3_5 pp_5_4 s_4_4 c_4_4 vdd vss ) fa
  xfa_4_5 ( c_3_5 s_3_6 pp_5_5 s_4_5 c_4_5 vdd vss ) fa
  xfa_4_6 ( c_3_6 s_3_7 pp_5_6 s_4_6 c_4_6 vdd vss ) fa
  xfa_4_7 ( c_3_7 s_3_8 pp_5_7 s_4_7 c_4_7 vdd vss ) fa
  xfa_4_8 ( c_3_8 s_3_9 pp_5_8 s_4_8 c_4_8 vdd vss ) fa
  xfa_4_9 ( c_3_9 s_3_10 pp_5_9 s_4_9 c_4_9 vdd vss ) fa
  xfa_4_10 ( c_3_10 s_3_11 pp_5_10 s_4_10 c_4_10 vdd vss ) fa
  xfa_4_11 ( c_3_11 s_3_12 pp_5_11 s_4_11 c_4_11 vdd vss ) fa
  xfa_4_12 ( c_3_12 s_3_13 pp_5_12 s_4_12 c_4_12 vdd vss ) fa
  xfa_4_13 ( c_3_13 s_3_14 pp_5_13 s_4_13 c_4_13 vdd vss ) fa
  xfa_4_14 ( c_3_14 s_3_15 pp_5_14 s_4_14 c_4_14 vdd vss ) fa
  xfa_4_15 ( c_3_15 pp_4_15 pp_5_15 s_4_15 c_4_15 vdd vss ) fa
  
  // Row 5
  xfa_5_1 ( c_4_1 s_4_2 pp_6_1 p6 c_5_1 vdd vss ) fa
  xfa_5_2 ( c_4_2 s_4_3 pp_6_2 s_5_2 c_5_2 vdd vss ) fa
  xfa_5_3 ( c_4_3 s_4_4 pp_6_3 s_5_3 c_5_3 vdd vss ) fa
  xfa_5_4 ( c_4_4 s_4_5 pp_6_4 s_5_4 c_5_4 vdd vss ) fa
  xfa_5_5 ( c_4_5 s_4_6 pp_6_5 s_5_5 c_5_5 vdd vss ) fa
  xfa_5_6 ( c_4_6 s_4_7 pp_6_6 s_5_6 c_5_6 vdd vss ) fa
  xfa_5_7 ( c_4_7 s_4_8 pp_6_7 s_5_7 c_5_7 vdd vss ) fa
  xfa_5_8 ( c_4_8 s_4_9 pp_6_8 s_5_8 c_5_8 vdd vss ) fa
  xfa_5_9 ( c_4_9 s_4_10 pp_6_9 s_5_9 c_5_9 vdd vss ) fa
  xfa_5_10 ( c_4_10 s_4_11 pp_6_10 s_5_10 c_5_10 vdd vss ) fa
  xfa_5_11 ( c_4_11 s_4_12 pp_6_11 s_5_11 c_5_11 vdd vss ) fa
  xfa_5_12 ( c_4_12 s_4_13 pp_6_12 s_5_12 c_5_12 vdd vss ) fa
  xfa_5_13 ( c_4_13 s_4_14 pp_6_13 s_5_13 c_5_13 vdd vss ) fa
  xfa_5_14 ( c_4_14 s_4_15 pp_6_14 s_5_14 c_5_14 vdd vss ) fa
  xfa_5_15 ( c_4_15 pp_5_15 pp_6_15 s_5_15 c_5_15 vdd vss ) fa
  
  // Row 6
  xfa_6_1 ( c_5_1 s_5_2 pp_7_1 p7 c_6_1 vdd vss ) fa
  xfa_6_2 ( c_5_2 s_5_3 pp_7_2 s_6_2 c_6_2 vdd vss ) fa
  xfa_6_3 ( c_5_3 s_5_4 pp_7_3 s_6_3 c_6_3 vdd vss ) fa
  xfa_6_4 ( c_5_4 s_5_5 pp_7_4 s_6_4 c_6_4 vdd vss ) fa
  xfa_6_5 ( c_5_5 s_5_6 pp_7_5 s_6_5 c_6_5 vdd vss ) fa
  xfa_6_6 ( c_5_6 s_5_7 pp_7_6 s_6_6 c_6_6 vdd vss ) fa
  xfa_6_7 ( c_5_7 s_5_8 pp_7_7 s_6_7 c_6_7 vdd vss ) fa
  xfa_6_8 ( c_5_8 s_5_9 pp_7_8 s_6_8 c_6_8 vdd vss ) fa
  xfa_6_9 ( c_5_9 s_5_10 pp_7_9 s_6_9 c_6_9 vdd vss ) fa
  xfa_6_10 ( c_5_10 s_5_11 pp_7_10 s_6_10 c_6_10 vdd vss ) fa
  xfa_6_11 ( c_5_11 s_5_12 pp_7_11 s_6_11 c_6_11 vdd vss ) fa
  xfa_6_12 ( c_5_12 s_5_13 pp_7_12 s_6_12 c_6_12 vdd vss ) fa
  xfa_6_13 ( c_5_13 s_5_14 pp_7_13 s_6_13 c_6_13 vdd vss ) fa
  xfa_6_14 ( c_5_14 s_5_15 pp_7_14 s_6_14 c_6_14 vdd vss ) fa
  xfa_6_15 ( c_5_15 pp_6_15 pp_7_15 s_6_15 c_6_15 vdd vss ) fa
  
  // Row 7
  xfa_7_1 ( c_6_1 s_6_2 pp_8_1 p8 c_7_1 vdd vss ) fa
  xfa_7_2 ( c_6_2 s_6_3 pp_8_2 s_7_2 c_7_2 vdd vss ) fa
  xfa_7_3 ( c_6_3 s_6_4 pp_8_3 s_7_3 c_7_3 vdd vss ) fa
  xfa_7_4 ( c_6_4 s_6_5 pp_8_4 s_7_4 c_7_4 vdd vss ) fa
  xfa_7_5 ( c_6_5 s_6_6 pp_8_5 s_7_5 c_7_5 vdd vss ) fa
  xfa_7_6 ( c_6_6 s_6_7 pp_8_6 s_7_6 c_7_6 vdd vss ) fa
  xfa_7_7 ( c_6_7 s_6_8 pp_8_7 s_7_7 c_7_7 vdd vss ) fa
  xfa_7_8 ( c_6_8 s_6_9 pp_8_8 s_7_8 c_7_8 vdd vss ) fa
  xfa_7_9 ( c_6_9 s_6_10 pp_8_9 s_7_9 c_7_9 vdd vss ) fa
  xfa_7_10 ( c_6_10 s_6_11 pp_8_10 s_7_10 c_7_10 vdd vss ) fa
  xfa_7_11 ( c_6_11 s_6_12 pp_8_11 s_7_11 c_7_11 vdd vss ) fa
  xfa_7_12 ( c_6_12 s_6_13 pp_8_12 s_7_12 c_7_12 vdd vss ) fa
  xfa_7_13 ( c_6_13 s_6_14 pp_8_13 s_7_13 c_7_13 vdd vss ) fa
  xfa_7_14 ( c_6_14 s_6_15 pp_8_14 s_7_14 c_7_14 vdd vss ) fa
  xfa_7_15 ( c_6_15 pp_7_15 pp_8_15 s_7_15 c_7_15 vdd vss ) fa
  
  // Row 8
  xfa_8_1 ( c_7_1 s_7_2 pp_9_1 p9 c_8_1 vdd vss ) fa
  xfa_8_2 ( c_7_2 s_7_3 pp_9_2 s_8_2 c_8_2 vdd vss ) fa
  xfa_8_3 ( c_7_3 s_7_4 pp_9_3 s_8_3 c_8_3 vdd vss ) fa
  xfa_8_4 ( c_7_4 s_7_5 pp_9_4 s_8_4 c_8_4 vdd vss ) fa
  xfa_8_5 ( c_7_5 s_7_6 pp_9_5 s_8_5 c_8_5 vdd vss ) fa
  xfa_8_6 ( c_7_6 s_7_7 pp_9_6 s_8_6 c_8_6 vdd vss ) fa
  xfa_8_7 ( c_7_7 s_7_8 pp_9_7 s_8_7 c_8_7 vdd vss ) fa
  xfa_8_8 ( c_7_8 s_7_9 pp_9_8 s_8_8 c_8_8 vdd vss ) fa
  xfa_8_9 ( c_7_9 s_7_10 pp_9_9 s_8_9 c_8_9 vdd vss ) fa
  xfa_8_10 ( c_7_10 s_7_11 pp_9_10 s_8_10 c_8_10 vdd vss ) fa
  xfa_8_11 ( c_7_11 s_7_12 pp_9_11 s_8_11 c_8_11 vdd vss ) fa
  xfa_8_12 ( c_7_12 s_7_13 pp_9_12 s_8_12 c_8_12 vdd vss ) fa
  xfa_8_13 ( c_7_13 s_7_14 pp_9_13 s_8_13 c_8_13 vdd vss ) fa
  xfa_8_14 ( c_7_14 s_7_15 pp_9_14 s_8_14 c_8_14 vdd vss ) fa
  xfa_8_15 ( c_7_15 pp_8_15 pp_9_15 s_8_15 c_8_15 vdd vss ) fa
  
  // Row 9
  xfa_9_1 ( c_8_1 s_8_2 pp_10_1 p10 c_9_1 vdd vss ) fa
  xfa_9_2 ( c_8_2 s_8_3 pp_10_2 s_9_2 c_9_2 vdd vss ) fa
  xfa_9_3 ( c_8_3 s_8_4 pp_10_3 s_9_3 c_9_3 vdd vss ) fa
  xfa_9_4 ( c_8_4 s_8_5 pp_10_4 s_9_4 c_9_4 vdd vss ) fa
  xfa_9_5 ( c_8_5 s_8_6 pp_10_5 s_9_5 c_9_5 vdd vss ) fa
  xfa_9_6 ( c_8_6 s_8_7 pp_10_6 s_9_6 c_9_6 vdd vss ) fa
  xfa_9_7 ( c_8_7 s_8_8 pp_10_7 s_9_7 c_9_7 vdd vss ) fa
  xfa_9_8 ( c_8_8 s_8_9 pp_10_8 s_9_8 c_9_8 vdd vss ) fa
  xfa_9_9 ( c_8_9 s_8_10 pp_10_9 s_9_9 c_9_9 vdd vss ) fa
  xfa_9_10 ( c_8_10 s_8_11 pp_10_10 s_9_10 c_9_10 vdd vss ) fa
  xfa_9_11 ( c_8_11 s_8_12 pp_10_11 s_9_11 c_9_11 vdd vss ) fa
  xfa_9_12 ( c_8_12 s_8_13 pp_10_12 s_9_12 c_9_12 vdd vss ) fa
  xfa_9_13 ( c_8_13 s_8_14 pp_10_13 s_9_13 c_9_13 vdd vss ) fa
  xfa_9_14 ( c_8_14 s_8_15 pp_10_14 s_9_14 c_9_14 vdd vss ) fa
  xfa_9_15 ( c_8_15 pp_9_15 pp_10_15 s_9_15 c_9_15 vdd vss ) fa
  
  // Row 10
  xfa_10_1 ( c_9_1 s_9_2 pp_11_1 p11 c_10_1 vdd vss ) fa
  xfa_10_2 ( c_9_2 s_9_3 pp_11_2 s_10_2 c_10_2 vdd vss ) fa
  xfa_10_3 ( c_9_3 s_9_4 pp_11_3 s_10_3 c_10_3 vdd vss ) fa
  xfa_10_4 ( c_9_4 s_9_5 pp_11_4 s_10_4 c_10_4 vdd vss ) fa
  xfa_10_5 ( c_9_5 s_9_6 pp_11_5 s_10_5 c_10_5 vdd vss ) fa
  xfa_10_6 ( c_9_6 s_9_7 pp_11_6 s_10_6 c_10_6 vdd vss ) fa
  xfa_10_7 ( c_9_7 s_9_8 pp_11_7 s_10_7 c_10_7 vdd vss ) fa
  xfa_10_8 ( c_9_8 s_9_9 pp_11_8 s_10_8 c_10_8 vdd vss ) fa
  xfa_10_9 ( c_9_9 s_9_10 pp_11_9 s_10_9 c_10_9 vdd vss ) fa
  xfa_10_10 ( c_9_10 s_9_11 pp_11_10 s_10_10 c_10_10 vdd vss ) fa
  xfa_10_11 ( c_9_11 s_9_12 pp_11_11 s_10_11 c_10_11 vdd vss ) fa
  xfa_10_12 ( c_9_12 s_9_13 pp_11_12 s_10_12 c_10_12 vdd vss ) fa
  xfa_10_13 ( c_9_13 s_9_14 pp_11_13 s_10_13 c_10_13 vdd vss ) fa
  xfa_10_14 ( c_9_14 s_9_15 pp_11_14 s_10_14 c_10_14 vdd vss ) fa
  xfa_10_15 ( c_9_15 pp_10_15 pp_11_15 s_10_15 c_10_15 vdd vss ) fa
  
  // Row 11
  xfa_11_1 ( c_10_1 s_10_2 pp_12_1 p12 c_11_1 vdd vss ) fa
  xfa_11_2 ( c_10_2 s_10_3 pp_12_2 s_11_2 c_11_2 vdd vss ) fa
  xfa_11_3 ( c_10_3 s_10_4 pp_12_3 s_11_3 c_11_3 vdd vss ) fa
  xfa_11_4 ( c_10_4 s_10_5 pp_12_4 s_11_4 c_11_4 vdd vss ) fa
  xfa_11_5 ( c_10_5 s_10_6 pp_12_5 s_11_5 c_11_5 vdd vss ) fa
  xfa_11_6 ( c_10_6 s_10_7 pp_12_6 s_11_6 c_11_6 vdd vss ) fa
  xfa_11_7 ( c_10_7 s_10_8 pp_12_7 s_11_7 c_11_7 vdd vss ) fa
  xfa_11_8 ( c_10_8 s_10_9 pp_12_8 s_11_8 c_11_8 vdd vss ) fa
  xfa_11_9 ( c_10_9 s_10_10 pp_12_9 s_11_9 c_11_9 vdd vss ) fa
  xfa_11_10 ( c_10_10 s_10_11 pp_12_10 s_11_10 c_11_10 vdd vss ) fa
  xfa_11_11 ( c_10_11 s_10_12 pp_12_11 s_11_11 c_11_11 vdd vss ) fa
  xfa_11_12 ( c_10_12 s_10_13 pp_12_12 s_11_12 c_11_12 vdd vss ) fa
  xfa_11_13 ( c_10_13 s_10_14 pp_12_13 s_11_13 c_11_13 vdd vss ) fa
  xfa_11_14 ( c_10_14 s_10_15 pp_12_14 s_11_14 c_11_14 vdd vss ) fa
  xfa_11_15 ( c_10_15 pp_11_15 pp_12_15 s_11_15 c_11_15 vdd vss ) fa
  
  // Row 12
  xfa_12_1 ( c_11_1 s_11_2 pp_13_1 p13 c_12_1 vdd vss ) fa
  xfa_12_2 ( c_11_2 s_11_3 pp_13_2 s_12_2 c_12_2 vdd vss ) fa
  xfa_12_3 ( c_11_3 s_11_4 pp_13_3 s_12_3 c_12_3 vdd vss ) fa
  xfa_12_4 ( c_11_4 s_11_5 pp_13_4 s_12_4 c_12_4 vdd vss ) fa
  xfa_12_5 ( c_11_5 s_11_6 pp_13_5 s_12_5 c_12_5 vdd vss ) fa
  xfa_12_6 ( c_11_6 s_11_7 pp_13_6 s_12_6 c_12_6 vdd vss ) fa
  xfa_12_7 ( c_11_7 s_11_8 pp_13_7 s_12_7 c_12_7 vdd vss ) fa
  xfa_12_8 ( c_11_8 s_11_9 pp_13_8 s_12_8 c_12_8 vdd vss ) fa
  xfa_12_9 ( c_11_9 s_11_10 pp_13_9 s_12_9 c_12_9 vdd vss ) fa
  xfa_12_10 ( c_11_10 s_11_11 pp_13_10 s_12_10 c_12_10 vdd vss ) fa
  xfa_12_11 ( c_11_11 s_11_12 pp_13_11 s_12_11 c_12_11 vdd vss ) fa
  xfa_12_12 ( c_11_12 s_11_13 pp_13_12 s_12_12 c_12_12 vdd vss ) fa
  xfa_12_13 ( c_11_13 s_11_14 pp_13_13 s_12_13 c_12_13 vdd vss ) fa
  xfa_12_14 ( c_11_14 s_11_15 pp_13_14 s_12_14 c_12_14 vdd vss ) fa
  xfa_12_15 ( c_11_15 pp_12_15 pp_13_15 s_12_15 c_12_15 vdd vss ) fa
  
  // Row 13
  xfa_13_1 ( c_12_1 s_12_2 pp_14_1 p14 c_13_1 vdd vss ) fa
  xfa_13_2 ( c_12_2 s_12_3 pp_14_2 s_13_2 c_13_2 vdd vss ) fa
  xfa_13_3 ( c_12_3 s_12_4 pp_14_3 s_13_3 c_13_3 vdd vss ) fa
  xfa_13_4 ( c_12_4 s_12_5 pp_14_4 s_13_4 c_13_4 vdd vss ) fa
  xfa_13_5 ( c_12_5 s_12_6 pp_14_5 s_13_5 c_13_5 vdd vss ) fa
  xfa_13_6 ( c_12_6 s_12_7 pp_14_6 s_13_6 c_13_6 vdd vss ) fa
  xfa_13_7 ( c_12_7 s_12_8 pp_14_7 s_13_7 c_13_7 vdd vss ) fa
  xfa_13_8 ( c_12_8 s_12_9 pp_14_8 s_13_8 c_13_8 vdd vss ) fa
  xfa_13_9 ( c_12_9 s_12_10 pp_14_9 s_13_9 c_13_9 vdd vss ) fa
  xfa_13_10 ( c_12_10 s_12_11 pp_14_10 s_13_10 c_13_10 vdd vss ) fa
  xfa_13_11 ( c_12_11 s_12_12 pp_14_11 s_13_11 c_13_11 vdd vss ) fa
  xfa_13_12 ( c_12_12 s_12_13 pp_14_12 s_13_12 c_13_12 vdd vss ) fa
  xfa_13_13 ( c_12_13 s_12_14 pp_14_13 s_13_13 c_13_13 vdd vss ) fa
  xfa_13_14 ( c_12_14 s_12_15 pp_14_14 s_13_14 c_13_14 vdd vss ) fa
  xfa_13_15 ( c_12_15 pp_13_15 pp_14_15 s_13_15 c_13_15 vdd vss ) fa
  
  // Row 14
  xfa_14_1 ( c_13_1 s_13_2 pp_15_1 p15 c_14_1 vdd vss ) fa
  xfa_14_2 ( c_13_2 s_13_3 pp_15_2 s_14_2 c_14_2 vdd vss ) fa
  xfa_14_3 ( c_13_3 s_13_4 pp_15_3 s_14_3 c_14_3 vdd vss ) fa
  xfa_14_4 ( c_13_4 s_13_5 pp_15_4 s_14_4 c_14_4 vdd vss ) fa
  xfa_14_5 ( c_13_5 s_13_6 pp_15_5 s_14_5 c_14_5 vdd vss ) fa
  xfa_14_6 ( c_13_6 s_13_7 pp_15_6 s_14_6 c_14_6 vdd vss ) fa
  xfa_14_7 ( c_13_7 s_13_8 pp_15_7 s_14_7 c_14_7 vdd vss ) fa
  xfa_14_8 ( c_13_8 s_13_9 pp_15_8 s_14_8 c_14_8 vdd vss ) fa
  xfa_14_9 ( c_13_9 s_13_10 pp_15_9 s_14_9 c_14_9 vdd vss ) fa
  xfa_14_10 ( c_13_10 s_13_11 pp_15_10 s_14_10 c_14_10 vdd vss ) fa
  xfa_14_11 ( c_13_11 s_13_12 pp_15_11 s_14_11 c_14_11 vdd vss ) fa
  xfa_14_12 ( c_13_12 s_13_13 pp_15_12 s_14_12 c_14_12 vdd vss ) fa
  xfa_14_13 ( c_13_13 s_13_14 pp_15_13 s_14_13 c_14_13 vdd vss ) fa
  xfa_14_14 ( c_13_14 s_13_15 pp_15_14 s_14_14 c_14_14 vdd vss ) fa
  xfa_14_15 ( c_13_15 pp_14_15 pp_15_15 s_14_15 c_14_15 vdd vss ) fa
  
  // Row 15 (last row)
  xha_14_1 ( c_14_1 s_14_2 p16 c_15_1 vdd vss ) ha
  xha_14_2 ( c_14_2 s_14_3 c_15_1 p17 c_15_2 vdd vss ) fa
  xha_14_3 ( c_14_3 s_14_4 c_15_2 p18 c_15_3 vdd vss ) fa
  xha_14_4 ( c_14_4 s_14_5 c_15_3 p19 c_15_4 vdd vss ) fa
  xha_14_5 ( c_14_5 s_14_6 c_15_4 p20 c_15_5 vdd vss ) fa
  xha_14_6 ( c_14_6 s_14_7 c_15_5 p21 c_15_6 vdd vss ) fa
  xha_14_7 ( c_14_7 s_14_8 c_15_6 p22 c_15_7 vdd vss ) fa
  xha_14_8 ( c_14_8 s_14_9 c_15_7 p23 c_15_8 vdd vss ) fa
  xha_14_9 ( c_14_9 s_14_10 c_15_8 p24 c_15_9 vdd vss ) fa
  xha_14_10 ( c_14_10 s_14_11 c_15_9 p25 c_15_10 vdd vss ) fa
  xha_14_11 ( c_14_11 s_14_12 c_15_10 p26 c_15_11 vdd vss ) fa
  xha_14_12 ( c_14_12 s_14_13 c_15_11 p27 c_15_12 vdd vss ) fa
  xha_14_13 ( c_14_13 s_14_14 c_15_12 p28 c_15_13 vdd vss ) fa
  xha_14_14 ( c_14_14 s_14_15 c_15_13 p29 c_15_14 vdd vss ) fa
  xha_14_15 ( c_14_15 pp_15_15 c_15_14 p30 p31 vdd vss ) fa
ends

subckt mul_test (  )
  xmul ( a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12 b13 b14 b15 p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15 p16 p17 p18 p19 p20 p21 p22 p23 p24 p25 p26 p27 p28 p29 p30 p31 vdd vss ) mul16 
  da0 ( a0 ) drv v0=0 v1=1
  da1 ( a1 ) drv v0=0 v1=1
  da2 ( a2 ) drv v0=0 v1=1
  da3 ( a3 ) drv v0=0 v1=1
  da4 ( a4 ) drv v0=0 v1=1
  da5 ( a5 ) drv v0=0 v1=1
  da6 ( a6 ) drv v0=0 v1=1
  da7 ( a7 ) drv v0=0 v1=1
  da8 ( a8 ) drv v0=0 v1=1
  da9 ( a9 ) drv v0=0 v1=1
  da10 ( a10 ) drv v0=0 v1=1
  da11 ( a11 ) drv v0=0 v1=1
  da12 ( a12 ) drv v0=0 v1=1
  da13 ( a13 ) drv v0=0 v1=1
  da14 ( a14 ) drv v0=0 v1=1
  da15 ( a15 ) drv v0=0 v1=1
  db0 ( b0 ) drv v0=0 v1=1
  db1 ( b1 ) drv v0=0 v1=1
  db2 ( b2 ) drv v0=0 v1=1
  db3 ( b3 ) drv v0=0 v1=1
  db4 ( b4 ) drv v0=0 v1=1
  db5 ( b5 ) drv v0=0 v1=1
  db6 ( b6 ) drv v0=0 v1=1
  db7 ( b7 ) drv v0=0 v1=1
  db8 ( b8 ) drv v0=0 v1=1
  db9 ( b9 ) drv v0=0 v1=1
  db10 ( b10 ) drv v0=0 v1=1
  db11 ( b11 ) drv v0=0 v1=1
  db12 ( b12 ) drv v0=0 v1=1
  db13 ( b13 ) drv v0=0 v1=1
  db14 ( b14 ) drv v0=0 v1=1
  db15 ( b15 ) drv v0=0 v1=1
ends
