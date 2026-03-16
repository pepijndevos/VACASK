
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

subckt mul8 ( a0 a1 a2 a3 a4 a5 a6 a7 b0 b1 b2 b3 b4 b5 b6 b7 p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15 vdd vss )
  // Partial products
  xpp_0_0 ( b0 a0 p0 vdd vss ) and2
  xpp_0_1 ( b0 a1 pp_0_1 vdd vss ) and2
  xpp_0_2 ( b0 a2 pp_0_2 vdd vss ) and2
  xpp_0_3 ( b0 a3 pp_0_3 vdd vss ) and2
  xpp_0_4 ( b0 a4 pp_0_4 vdd vss ) and2
  xpp_0_5 ( b0 a5 pp_0_5 vdd vss ) and2
  xpp_0_6 ( b0 a6 pp_0_6 vdd vss ) and2
  xpp_0_7 ( b0 a7 pp_0_7 vdd vss ) and2
  xpp_1_0 ( b1 a0 pp_1_0 vdd vss ) and2
  xpp_1_1 ( b1 a1 pp_1_1 vdd vss ) and2
  xpp_1_2 ( b1 a2 pp_1_2 vdd vss ) and2
  xpp_1_3 ( b1 a3 pp_1_3 vdd vss ) and2
  xpp_1_4 ( b1 a4 pp_1_4 vdd vss ) and2
  xpp_1_5 ( b1 a5 pp_1_5 vdd vss ) and2
  xpp_1_6 ( b1 a6 pp_1_6 vdd vss ) and2
  xpp_1_7 ( b1 a7 pp_1_7 vdd vss ) and2
  xpp_2_0 ( b2 a0 pp_2_0 vdd vss ) and2
  xpp_2_1 ( b2 a1 pp_2_1 vdd vss ) and2
  xpp_2_2 ( b2 a2 pp_2_2 vdd vss ) and2
  xpp_2_3 ( b2 a3 pp_2_3 vdd vss ) and2
  xpp_2_4 ( b2 a4 pp_2_4 vdd vss ) and2
  xpp_2_5 ( b2 a5 pp_2_5 vdd vss ) and2
  xpp_2_6 ( b2 a6 pp_2_6 vdd vss ) and2
  xpp_2_7 ( b2 a7 pp_2_7 vdd vss ) and2
  xpp_3_0 ( b3 a0 pp_3_0 vdd vss ) and2
  xpp_3_1 ( b3 a1 pp_3_1 vdd vss ) and2
  xpp_3_2 ( b3 a2 pp_3_2 vdd vss ) and2
  xpp_3_3 ( b3 a3 pp_3_3 vdd vss ) and2
  xpp_3_4 ( b3 a4 pp_3_4 vdd vss ) and2
  xpp_3_5 ( b3 a5 pp_3_5 vdd vss ) and2
  xpp_3_6 ( b3 a6 pp_3_6 vdd vss ) and2
  xpp_3_7 ( b3 a7 pp_3_7 vdd vss ) and2
  xpp_4_0 ( b4 a0 pp_4_0 vdd vss ) and2
  xpp_4_1 ( b4 a1 pp_4_1 vdd vss ) and2
  xpp_4_2 ( b4 a2 pp_4_2 vdd vss ) and2
  xpp_4_3 ( b4 a3 pp_4_3 vdd vss ) and2
  xpp_4_4 ( b4 a4 pp_4_4 vdd vss ) and2
  xpp_4_5 ( b4 a5 pp_4_5 vdd vss ) and2
  xpp_4_6 ( b4 a6 pp_4_6 vdd vss ) and2
  xpp_4_7 ( b4 a7 pp_4_7 vdd vss ) and2
  xpp_5_0 ( b5 a0 pp_5_0 vdd vss ) and2
  xpp_5_1 ( b5 a1 pp_5_1 vdd vss ) and2
  xpp_5_2 ( b5 a2 pp_5_2 vdd vss ) and2
  xpp_5_3 ( b5 a3 pp_5_3 vdd vss ) and2
  xpp_5_4 ( b5 a4 pp_5_4 vdd vss ) and2
  xpp_5_5 ( b5 a5 pp_5_5 vdd vss ) and2
  xpp_5_6 ( b5 a6 pp_5_6 vdd vss ) and2
  xpp_5_7 ( b5 a7 pp_5_7 vdd vss ) and2
  xpp_6_0 ( b6 a0 pp_6_0 vdd vss ) and2
  xpp_6_1 ( b6 a1 pp_6_1 vdd vss ) and2
  xpp_6_2 ( b6 a2 pp_6_2 vdd vss ) and2
  xpp_6_3 ( b6 a3 pp_6_3 vdd vss ) and2
  xpp_6_4 ( b6 a4 pp_6_4 vdd vss ) and2
  xpp_6_5 ( b6 a5 pp_6_5 vdd vss ) and2
  xpp_6_6 ( b6 a6 pp_6_6 vdd vss ) and2
  xpp_6_7 ( b6 a7 pp_6_7 vdd vss ) and2
  xpp_7_0 ( b7 a0 pp_7_0 vdd vss ) and2
  xpp_7_1 ( b7 a1 pp_7_1 vdd vss ) and2
  xpp_7_2 ( b7 a2 pp_7_2 vdd vss ) and2
  xpp_7_3 ( b7 a3 pp_7_3 vdd vss ) and2
  xpp_7_4 ( b7 a4 pp_7_4 vdd vss ) and2
  xpp_7_5 ( b7 a5 pp_7_5 vdd vss ) and2
  xpp_7_6 ( b7 a6 pp_7_6 vdd vss ) and2
  xpp_7_7 ( b7 a7 pp_7_7 vdd vss ) and2
  
  // Row 0 (half adders)
  xha_0_1 ( pp_0_1 pp_1_0 p1 c_0_1 vdd vss ) ha
  xha_0_2 ( pp_0_2 pp_1_1 s_0_2 c_0_2 vdd vss ) ha
  xha_0_3 ( pp_0_3 pp_1_2 s_0_3 c_0_3 vdd vss ) ha
  xha_0_4 ( pp_0_4 pp_1_3 s_0_4 c_0_4 vdd vss ) ha
  xha_0_5 ( pp_0_5 pp_1_4 s_0_5 c_0_5 vdd vss ) ha
  xha_0_6 ( pp_0_6 pp_1_5 s_0_6 c_0_6 vdd vss ) ha
  xha_0_7 ( pp_0_7 pp_1_6 s_0_7 c_0_7 vdd vss ) ha
  
  // Row 1
  xfa_1_1 ( c_0_1 s_0_2 pp_2_1 p2 c_1_1 vdd vss ) fa
  xfa_1_2 ( c_0_2 s_0_3 pp_2_2 s_1_2 c_1_2 vdd vss ) fa
  xfa_1_3 ( c_0_3 s_0_4 pp_2_3 s_1_3 c_1_3 vdd vss ) fa
  xfa_1_4 ( c_0_4 s_0_5 pp_2_4 s_1_4 c_1_4 vdd vss ) fa
  xfa_1_5 ( c_0_5 s_0_6 pp_2_5 s_1_5 c_1_5 vdd vss ) fa
  xfa_1_6 ( c_0_6 s_0_7 pp_2_6 s_1_6 c_1_6 vdd vss ) fa
  xfa_1_7 ( c_0_7 pp_1_7 pp_2_7 s_1_7 c_1_7 vdd vss ) fa
  
  // Row 2
  xfa_2_1 ( c_1_1 s_1_2 pp_3_1 p3 c_2_1 vdd vss ) fa
  xfa_2_2 ( c_1_2 s_1_3 pp_3_2 s_2_2 c_2_2 vdd vss ) fa
  xfa_2_3 ( c_1_3 s_1_4 pp_3_3 s_2_3 c_2_3 vdd vss ) fa
  xfa_2_4 ( c_1_4 s_1_5 pp_3_4 s_2_4 c_2_4 vdd vss ) fa
  xfa_2_5 ( c_1_5 s_1_6 pp_3_5 s_2_5 c_2_5 vdd vss ) fa
  xfa_2_6 ( c_1_6 s_1_7 pp_3_6 s_2_6 c_2_6 vdd vss ) fa
  xfa_2_7 ( c_1_7 pp_2_7 pp_3_7 s_2_7 c_2_7 vdd vss ) fa
  
  // Row 3
  xfa_3_1 ( c_2_1 s_2_2 pp_4_1 p4 c_3_1 vdd vss ) fa
  xfa_3_2 ( c_2_2 s_2_3 pp_4_2 s_3_2 c_3_2 vdd vss ) fa
  xfa_3_3 ( c_2_3 s_2_4 pp_4_3 s_3_3 c_3_3 vdd vss ) fa
  xfa_3_4 ( c_2_4 s_2_5 pp_4_4 s_3_4 c_3_4 vdd vss ) fa
  xfa_3_5 ( c_2_5 s_2_6 pp_4_5 s_3_5 c_3_5 vdd vss ) fa
  xfa_3_6 ( c_2_6 s_2_7 pp_4_6 s_3_6 c_3_6 vdd vss ) fa
  xfa_3_7 ( c_2_7 pp_3_7 pp_4_7 s_3_7 c_3_7 vdd vss ) fa
  
  // Row 4
  xfa_4_1 ( c_3_1 s_3_2 pp_5_1 p5 c_4_1 vdd vss ) fa
  xfa_4_2 ( c_3_2 s_3_3 pp_5_2 s_4_2 c_4_2 vdd vss ) fa
  xfa_4_3 ( c_3_3 s_3_4 pp_5_3 s_4_3 c_4_3 vdd vss ) fa
  xfa_4_4 ( c_3_4 s_3_5 pp_5_4 s_4_4 c_4_4 vdd vss ) fa
  xfa_4_5 ( c_3_5 s_3_6 pp_5_5 s_4_5 c_4_5 vdd vss ) fa
  xfa_4_6 ( c_3_6 s_3_7 pp_5_6 s_4_6 c_4_6 vdd vss ) fa
  xfa_4_7 ( c_3_7 pp_4_7 pp_5_7 s_4_7 c_4_7 vdd vss ) fa
  
  // Row 5
  xfa_5_1 ( c_4_1 s_4_2 pp_6_1 p6 c_5_1 vdd vss ) fa
  xfa_5_2 ( c_4_2 s_4_3 pp_6_2 s_5_2 c_5_2 vdd vss ) fa
  xfa_5_3 ( c_4_3 s_4_4 pp_6_3 s_5_3 c_5_3 vdd vss ) fa
  xfa_5_4 ( c_4_4 s_4_5 pp_6_4 s_5_4 c_5_4 vdd vss ) fa
  xfa_5_5 ( c_4_5 s_4_6 pp_6_5 s_5_5 c_5_5 vdd vss ) fa
  xfa_5_6 ( c_4_6 s_4_7 pp_6_6 s_5_6 c_5_6 vdd vss ) fa
  xfa_5_7 ( c_4_7 pp_5_7 pp_6_7 s_5_7 c_5_7 vdd vss ) fa
  
  // Row 6
  xfa_6_1 ( c_5_1 s_5_2 pp_7_1 p7 c_6_1 vdd vss ) fa
  xfa_6_2 ( c_5_2 s_5_3 pp_7_2 s_6_2 c_6_2 vdd vss ) fa
  xfa_6_3 ( c_5_3 s_5_4 pp_7_3 s_6_3 c_6_3 vdd vss ) fa
  xfa_6_4 ( c_5_4 s_5_5 pp_7_4 s_6_4 c_6_4 vdd vss ) fa
  xfa_6_5 ( c_5_5 s_5_6 pp_7_5 s_6_5 c_6_5 vdd vss ) fa
  xfa_6_6 ( c_5_6 s_5_7 pp_7_6 s_6_6 c_6_6 vdd vss ) fa
  xfa_6_7 ( c_5_7 pp_6_7 pp_7_7 s_6_7 c_6_7 vdd vss ) fa
  
  // Row 7 (last row)
  xha_6_1 ( c_6_1 s_6_2 p8 c_7_1 vdd vss ) ha
  xha_6_2 ( c_6_2 s_6_3 c_7_1 p9 c_7_2 vdd vss ) fa
  xha_6_3 ( c_6_3 s_6_4 c_7_2 p10 c_7_3 vdd vss ) fa
  xha_6_4 ( c_6_4 s_6_5 c_7_3 p11 c_7_4 vdd vss ) fa
  xha_6_5 ( c_6_5 s_6_6 c_7_4 p12 c_7_5 vdd vss ) fa
  xha_6_6 ( c_6_6 s_6_7 c_7_5 p13 c_7_6 vdd vss ) fa
  xha_6_7 ( c_6_7 pp_7_7 c_7_6 p14 p15 vdd vss ) fa
ends

subckt mul_test (  )
  xmul ( a0 a1 a2 a3 a4 a5 a6 a7 b0 b1 b2 b3 b4 b5 b6 b7 p0 p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15 vdd vss ) mul8 
  da0 ( a0 ) drv v0=0 v1=1
  da1 ( a1 ) drv v0=0 v1=1
  da2 ( a2 ) drv v0=0 v1=1
  da3 ( a3 ) drv v0=0 v1=1
  da4 ( a4 ) drv v0=0 v1=1
  da5 ( a5 ) drv v0=0 v1=1
  da6 ( a6 ) drv v0=0 v1=1
  da7 ( a7 ) drv v0=0 v1=1
  db0 ( b0 ) drv v0=0 v1=1
  db1 ( b1 ) drv v0=0 v1=1
  db2 ( b2 ) drv v0=0 v1=1
  db3 ( b3 ) drv v0=0 v1=1
  db4 ( b4 ) drv v0=0 v1=1
  db5 ( b5 ) drv v0=0 v1=1
  db6 ( b6 ) drv v0=0 v1=1
  db7 ( b7 ) drv v0=0 v1=1
ends
