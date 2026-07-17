// Equivalent circuit for vector fitted s-matrix
// Created using scikit-rf 1.6.2 vectorFitting.py
// Command line: ../../python/ts2vc.py -p inductor-tap.s3p
//
// auto_fit options:
//   n_poles_init_real=3
//   n_poles_init_cmplx=3
//   n_poles_add=3
//   model_order_max=100
//   iters_start=3
//   iters_inter=3
//   iters_final=5
//   target_error=0.01
//   alpha=0.03
//   gamma=0.03
//   nu_samples=1.0
//   parameter_type=s
// passivity_enforce options:
//   n_samples=200
//   f_max=None
//   parameter_type=s
//   preserve_dc=True
// write_vacask_subcircuit_s options:
//   fitted_model_name='inductor_tap'
//   create_reference_pins=False
//
// Model order: 6
// Maximal magnitude error: 0.00799475 dB at 7.5e+08 Hz
//
// The enclosing circuit must load these modules beforehand:
//   load "spice/resistor.osdi"
//   load "spice/capacitor.osdi"
//
subckt inductor_tap (p1 p2 p3)
  model resistor resistor noisy=0
  model capacitor capacitor
  model vsource vsource
  model vccs vccs
  model cccs cccs
  //
  // Port network for port 1
  v1 (p1 s1) vsource dc=0
  r1 (s1 0) resistor r=50.0
  gd1_1 (0 s1 p1 0) vccs gain=-0.0029266833650138002
  fd1_1 (0 s1) cccs ctlinst="v1" gain=-0.14633416825069
  gr1_1_1 (0 s1 x1_a1 0) vccs gain=544097107404.69946
  gr2_re_1_1 (0 s1 x2_re_a1 0) vccs gain=-35767114124.05191
  gr2_im_1_1 (0 s1 x2_im_a1 0) vccs gain=-5270964089.533416
  gr3_1_1 (0 s1 x3_a1 0) vccs gain=-152547082509.55566
  gr4_1_1 (0 s1 x4_a1 0) vccs gain=-4567977.486678794
  gr5_1_1 (0 s1 x5_a1 0) vccs gain=-2539430040.981559
  gd1_2 (0 s1 p2 0) vccs gain=0.012122407246797813
  fd1_2 (0 s1) cccs ctlinst="v2" gain=0.6061203623398908
  gr1_1_2 (0 s1 x1_a2 0) vccs gain=-597032832299.5769
  gr2_re_1_2 (0 s1 x2_re_a2 0) vccs gain=21517677321.688206
  gr2_im_1_2 (0 s1 x2_im_a2 0) vccs gain=-2565260393.359834
  gr3_1_2 (0 s1 x3_a2 0) vccs gain=152483870388.29834
  gr4_1_2 (0 s1 x4_a2 0) vccs gain=10797317.969548091
  gr5_1_2 (0 s1 x5_a2 0) vccs gain=2386785080.9912586
  gd1_3 (0 s1 p3 0) vccs gain=0.0015125784020133418
  fd1_3 (0 s1) cccs ctlinst="v3" gain=0.07562892010066709
  gr1_1_3 (0 s1 x1_a3 0) vccs gain=177461770139.8626
  gr2_re_1_3 (0 s1 x2_re_a3 0) vccs gain=-18888690494.288403
  gr2_im_1_3 (0 s1 x2_im_a3 0) vccs gain=-10552122041.081167
  gr3_1_3 (0 s1 x3_a3 0) vccs gain=-465361881.6262604
  gr4_1_3 (0 s1 x4_a3 0) vccs gain=5093118.3712986475
  gr5_1_3 (0 s1 x5_a3 0) vccs gain=248513830.9306387
  //
  // State networks driven by port 1
  cx1_a1 (x1_a1 0) capacitor c=1.0
  gx1_a1 (0 x1_a1 p1 0) vccs gain=0.07071067811865475
  fx1_a1 (0 x1_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp1_a1 (0 x1_a1) resistor r=8.489223375550384e-13
  cx2_re_a1 (x2_re_a1 0) capacitor c=1.0
  gx2_re_a1 (0 x2_re_a1 p1 0) vccs gain=0.1414213562373095
  fx2_re_a1 (0 x2_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp2_re_re_a1 (0 x2_re_a1) resistor r=5.565753295068906e-12
  gp2_re_im_a1 (0 x2_re_a1 x2_im_a1 0) vccs gain=876079122562.9581
  cx2_im_a1 (x2_im_a1 0) capacitor c=1.0
  gp2_im_re_a1 (0 x2_im_a1 x2_re_a1 0) vccs gain=-876079122562.9581
  rp2_im_im_a1 (0 x2_im_a1) resistor r=5.565753295068906e-12
  cx3_a1 (x3_a1 0) capacitor c=1.0
  gx3_a1 (0 x3_a1 p1 0) vccs gain=0.07071067811865475
  fx3_a1 (0 x3_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp3_a1 (0 x3_a1) resistor r=3.1737189907214954e-12
  cx4_a1 (x4_a1 0) capacitor c=1.0
  gx4_a1 (0 x4_a1 p1 0) vccs gain=0.07071067811865475
  fx4_a1 (0 x4_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp4_a1 (0 x4_a1) resistor r=9.292417472009568e-11
  cx5_a1 (x5_a1 0) capacitor c=1.0
  gx5_a1 (0 x5_a1 p1 0) vccs gain=0.07071067811865475
  fx5_a1 (0 x5_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp5_a1 (0 x5_a1) resistor r=8.246018696932696e-12
  //
  // Port network for port 2
  v2 (p2 s2) vsource dc=0
  r2 (s2 0) resistor r=50.0
  gd2_1 (0 s2 p1 0) vccs gain=0.012122407246791965
  fd2_1 (0 s2) cccs ctlinst="v1" gain=0.6061203623395983
  gr1_2_1 (0 s2 x1_a1 0) vccs gain=-597032832299.3993
  gr2_re_2_1 (0 s2 x2_re_a1 0) vccs gain=21517677321.705532
  gr2_im_2_1 (0 s2 x2_im_a1 0) vccs gain=-2565260393.338923
  gr3_2_1 (0 s2 x3_a1 0) vccs gain=152483870388.287
  gr4_2_1 (0 s2 x4_a1 0) vccs gain=10797317.96954326
  gr5_2_1 (0 s2 x5_a1 0) vccs gain=2386785080.992061
  gd2_2 (0 s2 p2 0) vccs gain=-0.0029157888469082817
  fd2_2 (0 s2) cccs ctlinst="v2" gain=-0.14578944234541408
  gr1_2_2 (0 s2 x1_a2 0) vccs gain=556443021627.3452
  gr2_re_2_2 (0 s2 x2_re_a2 0) vccs gain=-39404486954.88383
  gr2_im_2_2 (0 s2 x2_im_a2 0) vccs gain=-4103731665.501135
  gr3_2_2 (0 s2 x3_a2 0) vccs gain=-154319476625.26825
  gr4_2_2 (0 s2 x4_a2 0) vccs gain=-4481125.431555959
  gr5_2_2 (0 s2 x5_a2 0) vccs gain=-2526478490.639403
  gd2_3 (0 s2 p3 0) vccs gain=0.0018920646074783458
  fd2_3 (0 s2) cccs ctlinst="v3" gain=0.0946032303739173
  gr1_2_3 (0 s2 x1_a3 0) vccs gain=168708949351.28555
  gr2_re_2_3 (0 s2 x2_re_a3 0) vccs gain=-21078311908.686985
  gr2_im_2_3 (0 s2 x2_im_a3 0) vccs gain=-11092658590.281643
  gr3_2_3 (0 s2 x3_a3 0) vccs gain=-108804589.59372143
  gr4_2_3 (0 s2 x4_a3 0) vccs gain=4939745.83217891
  gr5_2_3 (0 s2 x5_a3 0) vccs gain=225769287.92354202
  //
  // State networks driven by port 2
  cx1_a2 (x1_a2 0) capacitor c=1.0
  gx1_a2 (0 x1_a2 p2 0) vccs gain=0.07071067811865475
  fx1_a2 (0 x1_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp1_a2 (0 x1_a2) resistor r=8.489223375550384e-13
  cx2_re_a2 (x2_re_a2 0) capacitor c=1.0
  gx2_re_a2 (0 x2_re_a2 p2 0) vccs gain=0.1414213562373095
  fx2_re_a2 (0 x2_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp2_re_re_a2 (0 x2_re_a2) resistor r=5.565753295068906e-12
  gp2_re_im_a2 (0 x2_re_a2 x2_im_a2 0) vccs gain=876079122562.9581
  cx2_im_a2 (x2_im_a2 0) capacitor c=1.0
  gp2_im_re_a2 (0 x2_im_a2 x2_re_a2 0) vccs gain=-876079122562.9581
  rp2_im_im_a2 (0 x2_im_a2) resistor r=5.565753295068906e-12
  cx3_a2 (x3_a2 0) capacitor c=1.0
  gx3_a2 (0 x3_a2 p2 0) vccs gain=0.07071067811865475
  fx3_a2 (0 x3_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp3_a2 (0 x3_a2) resistor r=3.1737189907214954e-12
  cx4_a2 (x4_a2 0) capacitor c=1.0
  gx4_a2 (0 x4_a2 p2 0) vccs gain=0.07071067811865475
  fx4_a2 (0 x4_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp4_a2 (0 x4_a2) resistor r=9.292417472009568e-11
  cx5_a2 (x5_a2 0) capacitor c=1.0
  gx5_a2 (0 x5_a2 p2 0) vccs gain=0.07071067811865475
  fx5_a2 (0 x5_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp5_a2 (0 x5_a2) resistor r=8.246018696932696e-12
  //
  // Port network for port 3
  v3 (p3 s3) vsource dc=0
  r3 (s3 0) resistor r=50.0
  gd3_1 (0 s3 p1 0) vccs gain=0.0015125784020133394
  fd3_1 (0 s3) cccs ctlinst="v1" gain=0.07562892010066698
  gr1_3_1 (0 s3 x1_a1 0) vccs gain=177461770139.86285
  gr2_re_3_1 (0 s3 x2_re_a1 0) vccs gain=-18888690494.28842
  gr2_im_3_1 (0 s3 x2_im_a1 0) vccs gain=-10552122041.081154
  gr3_3_1 (0 s3 x3_a1 0) vccs gain=-465361881.62631196
  gr4_3_1 (0 s3 x4_a1 0) vccs gain=5093118.371295485
  gr5_3_1 (0 s3 x5_a1 0) vccs gain=248513830.93065348
  gd3_2 (0 s3 p2 0) vccs gain=0.0018920646074916982
  fd3_2 (0 s3) cccs ctlinst="v2" gain=0.09460323037458492
  gr1_3_2 (0 s3 x1_a2 0) vccs gain=168708949350.98233
  gr2_re_3_2 (0 s3 x2_re_a2 0) vccs gain=-21078311908.72997
  gr2_im_3_2 (0 s3 x2_im_a2 0) vccs gain=-11092658590.304312
  gr3_3_2 (0 s3 x3_a2 0) vccs gain=-108804589.57783166
  gr4_3_2 (0 s3 x4_a2 0) vccs gain=4939745.832363972
  gr5_3_2 (0 s3 x5_a2 0) vccs gain=225769287.92172566
  gd3_3 (0 s3 p3 0) vccs gain=0.007874929673093774
  fd3_3 (0 s3) cccs ctlinst="v3" gain=0.3937464836546888
  gr1_3_3 (0 s3 x1_a3 0) vccs gain=-249655264151.1352
  gr2_re_3_3 (0 s3 x2_re_a3 0) vccs gain=-228133852.24371275
  gr2_im_3_3 (0 s3 x2_im_a3 0) vccs gain=-5727363281.871225
  gr3_3_3 (0 s3 x3_a3 0) vccs gain=-121586511.46241249
  gr4_3_3 (0 s3 x4_a3 0) vccs gain=1579475.275895115
  gr5_3_3 (0 s3 x5_a3 0) vccs gain=-415720390.429595
  //
  // State networks driven by port 3
  cx1_a3 (x1_a3 0) capacitor c=1.0
  gx1_a3 (0 x1_a3 p3 0) vccs gain=0.07071067811865475
  fx1_a3 (0 x1_a3) cccs ctlinst="v3" gain=3.5355339059327378
  rp1_a3 (0 x1_a3) resistor r=8.489223375550384e-13
  cx2_re_a3 (x2_re_a3 0) capacitor c=1.0
  gx2_re_a3 (0 x2_re_a3 p3 0) vccs gain=0.1414213562373095
  fx2_re_a3 (0 x2_re_a3) cccs ctlinst="v3" gain=7.0710678118654755
  rp2_re_re_a3 (0 x2_re_a3) resistor r=5.565753295068906e-12
  gp2_re_im_a3 (0 x2_re_a3 x2_im_a3 0) vccs gain=876079122562.9581
  cx2_im_a3 (x2_im_a3 0) capacitor c=1.0
  gp2_im_re_a3 (0 x2_im_a3 x2_re_a3 0) vccs gain=-876079122562.9581
  rp2_im_im_a3 (0 x2_im_a3) resistor r=5.565753295068906e-12
  cx3_a3 (x3_a3 0) capacitor c=1.0
  gx3_a3 (0 x3_a3 p3 0) vccs gain=0.07071067811865475
  fx3_a3 (0 x3_a3) cccs ctlinst="v3" gain=3.5355339059327378
  rp3_a3 (0 x3_a3) resistor r=3.1737189907214954e-12
  cx4_a3 (x4_a3 0) capacitor c=1.0
  gx4_a3 (0 x4_a3 p3 0) vccs gain=0.07071067811865475
  fx4_a3 (0 x4_a3) cccs ctlinst="v3" gain=3.5355339059327378
  rp4_a3 (0 x4_a3) resistor r=9.292417472009568e-11
  cx5_a3 (x5_a3 0) capacitor c=1.0
  gx5_a3 (0 x5_a3 p3 0) vccs gain=0.07071067811865475
  fx5_a3 (0 x5_a3) cccs ctlinst="v3" gain=3.5355339059327378
  rp5_a3 (0 x5_a3) resistor r=8.246018696932696e-12
ends inductor_tap
