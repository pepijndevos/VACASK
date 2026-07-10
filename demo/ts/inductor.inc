// Equivalent circuit for vector fitted s-matrix
// Created using scikit-rf 1.6.2 vectorFitting.py
// Command line: ../../python/ts2vc.py -p -mo 20 -ga 1e-7 -pr 5 -pc 5 -rp inductor.s2p
//
// auto_fit options:
//   n_poles_init_real=5
//   n_poles_init_cmplx=5
//   n_poles_add=3
//   model_order_max=20
//   iters_start=3
//   iters_inter=3
//   iters_final=5
//   target_error=0.01
//   alpha=0.03
//   gamma=1e-07
//   nu_samples=1.0
//   parameter_type=s
// passivity_enforce: not called, fit was already passive
// write_vacask_subcircuit_s options:
//   fitted_model_name='inductor'
//   create_reference_pins=True
//
// Model order: 15
// Maximal magnitude error: 0.791054 dB at 2.6625e+10 Hz
//
// The enclosing circuit must load these modules beforehand:
//   load "spice/resistor.osdi"
//   load "spice/capacitor.osdi"
//
subckt inductor (p1 p1_ref p2 p2_ref)
  model resistor resistor noisy=0
  model capacitor capacitor
  model vsource vsource
  model vccs vccs
  model cccs cccs
  //
  // Port network for port 1
  v1 (p1 s1) vsource dc=0
  r1 (s1 p1_ref) resistor r=50.0
  gd1_1 (p1_ref s1 p1 p1_ref) vccs gain=1.6028226652496645e-07
  fd1_1 (p1_ref s1) cccs ctlinst="v1" gain=8.014113326248323e-06
  gr1_1_1 (p1_ref s1 x1_a1 0) vccs gain=-212010650272.519
  gr2_re_1_1 (p1_ref s1 x2_re_a1 0) vccs gain=124885743444.26477
  gr2_im_1_1 (p1_ref s1 x2_im_a1 0) vccs gain=-42951333061.34982
  gr3_re_1_1 (p1_ref s1 x3_re_a1 0) vccs gain=9098582.513878532
  gr3_im_1_1 (p1_ref s1 x3_im_a1 0) vccs gain=9764808.894207586
  gr4_re_1_1 (p1_ref s1 x4_re_a1 0) vccs gain=-10999965.3085119
  gr4_im_1_1 (p1_ref s1 x4_im_a1 0) vccs gain=16861404.644481216
  gr5_re_1_1 (p1_ref s1 x5_re_a1 0) vccs gain=-40409795.54438665
  gr5_im_1_1 (p1_ref s1 x5_im_a1 0) vccs gain=-48101020.97352789
  gr6_1_1 (p1_ref s1 x6_a1 0) vccs gain=960247.8092216637
  gr7_re_1_1 (p1_ref s1 x7_re_a1 0) vccs gain=503468861.072248
  gr7_im_1_1 (p1_ref s1 x7_im_a1 0) vccs gain=-337663612.29082316
  gr8_1_1 (p1_ref s1 x8_a1 0) vccs gain=-1640036159.833462
  gr9_re_1_1 (p1_ref s1 x9_re_a1 0) vccs gain=-16229126254.3162
  gr9_im_1_1 (p1_ref s1 x9_im_a1 0) vccs gain=-71576049964.41063
  gd1_2 (p1_ref s1 p2 p2_ref) vccs gain=-6.392526051879343e-07
  fd1_2 (p1_ref s1) cccs ctlinst="v2" gain=-3.196263025939672e-05
  gr1_1_2 (p1_ref s1 x1_a2 0) vccs gain=232929495617.3754
  gr2_re_1_2 (p1_ref s1 x2_re_a2 0) vccs gain=-66184719577.22926
  gr2_im_1_2 (p1_ref s1 x2_im_a2 0) vccs gain=105847275185.369
  gr3_re_1_2 (p1_ref s1 x3_re_a2 0) vccs gain=-18656787.740253184
  gr3_im_1_2 (p1_ref s1 x3_im_a2 0) vccs gain=-21944080.217745975
  gr4_re_1_2 (p1_ref s1 x4_re_a2 0) vccs gain=25553471.67243662
  gr4_im_1_2 (p1_ref s1 x4_im_a2 0) vccs gain=-30442457.618910752
  gr5_re_1_2 (p1_ref s1 x5_re_a2 0) vccs gain=42752604.19205155
  gr5_im_1_2 (p1_ref s1 x5_im_a2 0) vccs gain=97554166.95805083
  gr6_1_2 (p1_ref s1 x6_a2 0) vccs gain=-1530786.8221888111
  gr7_re_1_2 (p1_ref s1 x7_re_a2 0) vccs gain=-979851998.2308954
  gr7_im_1_2 (p1_ref s1 x7_im_a2 0) vccs gain=156800694.76581383
  gr8_1_2 (p1_ref s1 x8_a2 0) vccs gain=3879709083.3908234
  gr9_re_1_2 (p1_ref s1 x9_re_a2 0) vccs gain=18900849515.442196
  gr9_im_1_2 (p1_ref s1 x9_im_a2 0) vccs gain=113715594892.15108
  //
  // State networks driven by port 1
  cx1_a1 (x1_a1 0) capacitor c=1.0
  gx1_a1 (0 x1_a1 p1 p1_ref) vccs gain=0.07071067811865475
  fx1_a1 (0 x1_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp1_a1 (0 x1_a1) resistor r=1.742719302438997e-12
  cx2_re_a1 (x2_re_a1 0) capacitor c=1.0
  gx2_re_a1 (0 x2_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx2_re_a1 (0 x2_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp2_re_re_a1 (0 x2_re_a1) resistor r=3.3792980989733566e-12
  gp2_re_im_a1 (0 x2_re_a1 x2_im_a1 0) vccs gain=125706648150.63338
  cx2_im_a1 (x2_im_a1 0) capacitor c=1.0
  gp2_im_re_a1 (0 x2_im_a1 x2_re_a1 0) vccs gain=-125706648150.63338
  rp2_im_im_a1 (0 x2_im_a1) resistor r=3.3792980989733566e-12
  cx3_re_a1 (x3_re_a1 0) capacitor c=1.0
  gx3_re_a1 (0 x3_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx3_re_a1 (0 x3_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp3_re_re_a1 (0 x3_re_a1) resistor r=6.646971606439884e-11
  gp3_re_im_a1 (0 x3_re_a1 x3_im_a1 0) vccs gain=171046332714.48782
  cx3_im_a1 (x3_im_a1 0) capacitor c=1.0
  gp3_im_re_a1 (0 x3_im_a1 x3_re_a1 0) vccs gain=-171046332714.48782
  rp3_im_im_a1 (0 x3_im_a1) resistor r=6.646971606439884e-11
  cx4_re_a1 (x4_re_a1 0) capacitor c=1.0
  gx4_re_a1 (0 x4_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx4_re_a1 (0 x4_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp4_re_re_a1 (0 x4_re_a1) resistor r=5.196247406708308e-11
  gp4_re_im_a1 (0 x4_re_a1 x4_im_a1 0) vccs gain=132128308336.11786
  cx4_im_a1 (x4_im_a1 0) capacitor c=1.0
  gp4_im_re_a1 (0 x4_im_a1 x4_re_a1 0) vccs gain=-132128308336.11786
  rp4_im_im_a1 (0 x4_im_a1) resistor r=5.196247406708308e-11
  cx5_re_a1 (x5_re_a1 0) capacitor c=1.0
  gx5_re_a1 (0 x5_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx5_re_a1 (0 x5_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp5_re_re_a1 (0 x5_re_a1) resistor r=4.0058447235076145e-11
  gp5_re_im_a1 (0 x5_re_a1 x5_im_a1 0) vccs gain=85243490906.3212
  cx5_im_a1 (x5_im_a1 0) capacitor c=1.0
  gp5_im_re_a1 (0 x5_im_a1 x5_re_a1 0) vccs gain=-85243490906.3212
  rp5_im_im_a1 (0 x5_im_a1) resistor r=4.0058447235076145e-11
  cx6_a1 (x6_a1 0) capacitor c=1.0
  gx6_a1 (0 x6_a1 p1 p1_ref) vccs gain=0.07071067811865475
  fx6_a1 (0 x6_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp6_a1 (0 x6_a1) resistor r=3.495980930066513e-10
  cx7_re_a1 (x7_re_a1 0) capacitor c=1.0
  gx7_re_a1 (0 x7_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx7_re_a1 (0 x7_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp7_re_re_a1 (0 x7_re_a1) resistor r=3.0303354470508616e-11
  gp7_re_im_a1 (0 x7_re_a1 x7_im_a1 0) vccs gain=40869115167.41904
  cx7_im_a1 (x7_im_a1 0) capacitor c=1.0
  gp7_im_re_a1 (0 x7_im_a1 x7_re_a1 0) vccs gain=-40869115167.41904
  rp7_im_im_a1 (0 x7_im_a1) resistor r=3.0303354470508616e-11
  cx8_a1 (x8_a1 0) capacitor c=1.0
  gx8_a1 (0 x8_a1 p1 p1_ref) vccs gain=0.07071067811865475
  fx8_a1 (0 x8_a1) cccs ctlinst="v1" gain=3.5355339059327378
  rp8_a1 (0 x8_a1) resistor r=3.399998983357487e-11
  cx9_re_a1 (x9_re_a1 0) capacitor c=1.0
  gx9_re_a1 (0 x9_re_a1 p1 p1_ref) vccs gain=0.1414213562373095
  fx9_re_a1 (0 x9_re_a1) cccs ctlinst="v1" gain=7.0710678118654755
  rp9_re_re_a1 (0 x9_re_a1) resistor r=1.838536383247396e-11
  gp9_re_im_a1 (0 x9_re_a1 x9_im_a1 0) vccs gain=3739057498.9046626
  cx9_im_a1 (x9_im_a1 0) capacitor c=1.0
  gp9_im_re_a1 (0 x9_im_a1 x9_re_a1 0) vccs gain=-3739057498.9046626
  rp9_im_im_a1 (0 x9_im_a1) resistor r=1.838536383247396e-11
  //
  // Port network for port 2
  v2 (p2 s2) vsource dc=0
  r2 (s2 p2_ref) resistor r=50.0
  gd2_1 (p2_ref s2 p1 p1_ref) vccs gain=-6.358313282484041e-07
  fd2_1 (p2_ref s2) cccs ctlinst="v1" gain=-3.179156641242021e-05
  gr1_2_1 (p2_ref s2 x1_a1 0) vccs gain=232916511691.595
  gr2_re_2_1 (p2_ref s2 x2_re_a1 0) vccs gain=-66179618326.13439
  gr2_im_2_1 (p2_ref s2 x2_im_a1 0) vccs gain=105853157072.10771
  gr3_re_2_1 (p2_ref s2 x3_re_a1 0) vccs gain=-18659596.79461506
  gr3_im_2_1 (p2_ref s2 x3_im_a1 0) vccs gain=-21946538.463418014
  gr4_re_2_1 (p2_ref s2 x4_re_a1 0) vccs gain=25557169.694324642
  gr4_im_2_1 (p2_ref s2 x4_im_a1 0) vccs gain=-30445985.44364145
  gr5_re_2_1 (p2_ref s2 x5_re_a1 0) vccs gain=42755656.63790825
  gr5_im_2_1 (p2_ref s2 x5_im_a1 0) vccs gain=97566453.96839722
  gr6_2_1 (p2_ref s2 x6_a1 0) vccs gain=-1531005.2214674598
  gr7_re_2_1 (p2_ref s2 x7_re_a1 0) vccs gain=-979932067.4639312
  gr7_im_2_1 (p2_ref s2 x7_im_a1 0) vccs gain=156806809.6574537
  gr8_2_1 (p2_ref s2 x8_a1 0) vccs gain=3879916664.077501
  gr9_re_2_1 (p2_ref s2 x9_re_a1 0) vccs gain=18901279297.146088
  gr9_im_2_1 (p2_ref s2 x9_im_a1 0) vccs gain=113720583105.93764
  gd2_2 (p2_ref s2 p2 p2_ref) vccs gain=1.5759706861798866e-07
  fd2_2 (p2_ref s2) cccs ctlinst="v2" gain=7.879853430899433e-06
  gr1_2_2 (p2_ref s2 x1_a2 0) vccs gain=-209895075662.2144
  gr2_re_2_2 (p2_ref s2 x2_re_a2 0) vccs gain=123749248709.72365
  gr2_im_2_2 (p2_ref s2 x2_im_a2 0) vccs gain=-45092487602.71586
  gr3_re_2_2 (p2_ref s2 x3_re_a2 0) vccs gain=9557829.722466305
  gr3_im_2_2 (p2_ref s2 x3_im_a2 0) vccs gain=10453871.617356827
  gr4_re_2_2 (p2_ref s2 x4_re_a2 0) vccs gain=-11803655.026842363
  gr4_im_2_2 (p2_ref s2 x4_im_a2 0) vccs gain=17606074.147776842
  gr5_re_2_2 (p2_ref s2 x5_re_a2 0) vccs gain=-41189107.32097514
  gr5_im_2_2 (p2_ref s2 x5_im_a2 0) vccs gain=-50593159.02483017
  gr6_2_2 (p2_ref s2 x6_a2 0) vccs gain=1000392.3446394533
  gr7_re_2_2 (p2_ref s2 x7_re_a2 0) vccs gain=520692745.84670943
  gr7_im_2_2 (p2_ref s2 x7_im_a2 0) vccs gain=-340359615.8237839
  gr8_2_2 (p2_ref s2 x8_a2 0) vccs gain=-1679205373.7456238
  gr9_re_2_2 (p2_ref s2 x9_re_a2 0) vccs gain=-16352797333.306345
  gr9_im_2_2 (p2_ref s2 x9_im_a2 0) vccs gain=-72743771206.86192
  //
  // State networks driven by port 2
  cx1_a2 (x1_a2 0) capacitor c=1.0
  gx1_a2 (0 x1_a2 p2 p2_ref) vccs gain=0.07071067811865475
  fx1_a2 (0 x1_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp1_a2 (0 x1_a2) resistor r=1.742719302438997e-12
  cx2_re_a2 (x2_re_a2 0) capacitor c=1.0
  gx2_re_a2 (0 x2_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx2_re_a2 (0 x2_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp2_re_re_a2 (0 x2_re_a2) resistor r=3.3792980989733566e-12
  gp2_re_im_a2 (0 x2_re_a2 x2_im_a2 0) vccs gain=125706648150.63338
  cx2_im_a2 (x2_im_a2 0) capacitor c=1.0
  gp2_im_re_a2 (0 x2_im_a2 x2_re_a2 0) vccs gain=-125706648150.63338
  rp2_im_im_a2 (0 x2_im_a2) resistor r=3.3792980989733566e-12
  cx3_re_a2 (x3_re_a2 0) capacitor c=1.0
  gx3_re_a2 (0 x3_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx3_re_a2 (0 x3_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp3_re_re_a2 (0 x3_re_a2) resistor r=6.646971606439884e-11
  gp3_re_im_a2 (0 x3_re_a2 x3_im_a2 0) vccs gain=171046332714.48782
  cx3_im_a2 (x3_im_a2 0) capacitor c=1.0
  gp3_im_re_a2 (0 x3_im_a2 x3_re_a2 0) vccs gain=-171046332714.48782
  rp3_im_im_a2 (0 x3_im_a2) resistor r=6.646971606439884e-11
  cx4_re_a2 (x4_re_a2 0) capacitor c=1.0
  gx4_re_a2 (0 x4_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx4_re_a2 (0 x4_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp4_re_re_a2 (0 x4_re_a2) resistor r=5.196247406708308e-11
  gp4_re_im_a2 (0 x4_re_a2 x4_im_a2 0) vccs gain=132128308336.11786
  cx4_im_a2 (x4_im_a2 0) capacitor c=1.0
  gp4_im_re_a2 (0 x4_im_a2 x4_re_a2 0) vccs gain=-132128308336.11786
  rp4_im_im_a2 (0 x4_im_a2) resistor r=5.196247406708308e-11
  cx5_re_a2 (x5_re_a2 0) capacitor c=1.0
  gx5_re_a2 (0 x5_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx5_re_a2 (0 x5_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp5_re_re_a2 (0 x5_re_a2) resistor r=4.0058447235076145e-11
  gp5_re_im_a2 (0 x5_re_a2 x5_im_a2 0) vccs gain=85243490906.3212
  cx5_im_a2 (x5_im_a2 0) capacitor c=1.0
  gp5_im_re_a2 (0 x5_im_a2 x5_re_a2 0) vccs gain=-85243490906.3212
  rp5_im_im_a2 (0 x5_im_a2) resistor r=4.0058447235076145e-11
  cx6_a2 (x6_a2 0) capacitor c=1.0
  gx6_a2 (0 x6_a2 p2 p2_ref) vccs gain=0.07071067811865475
  fx6_a2 (0 x6_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp6_a2 (0 x6_a2) resistor r=3.495980930066513e-10
  cx7_re_a2 (x7_re_a2 0) capacitor c=1.0
  gx7_re_a2 (0 x7_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx7_re_a2 (0 x7_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp7_re_re_a2 (0 x7_re_a2) resistor r=3.0303354470508616e-11
  gp7_re_im_a2 (0 x7_re_a2 x7_im_a2 0) vccs gain=40869115167.41904
  cx7_im_a2 (x7_im_a2 0) capacitor c=1.0
  gp7_im_re_a2 (0 x7_im_a2 x7_re_a2 0) vccs gain=-40869115167.41904
  rp7_im_im_a2 (0 x7_im_a2) resistor r=3.0303354470508616e-11
  cx8_a2 (x8_a2 0) capacitor c=1.0
  gx8_a2 (0 x8_a2 p2 p2_ref) vccs gain=0.07071067811865475
  fx8_a2 (0 x8_a2) cccs ctlinst="v2" gain=3.5355339059327378
  rp8_a2 (0 x8_a2) resistor r=3.399998983357487e-11
  cx9_re_a2 (x9_re_a2 0) capacitor c=1.0
  gx9_re_a2 (0 x9_re_a2 p2 p2_ref) vccs gain=0.1414213562373095
  fx9_re_a2 (0 x9_re_a2) cccs ctlinst="v2" gain=7.0710678118654755
  rp9_re_re_a2 (0 x9_re_a2) resistor r=1.838536383247396e-11
  gp9_re_im_a2 (0 x9_re_a2 x9_im_a2 0) vccs gain=3739057498.9046626
  cx9_im_a2 (x9_im_a2 0) capacitor c=1.0
  gp9_im_re_a2 (0 x9_im_a2 x9_re_a2 0) vccs gain=-3739057498.9046626
  rp9_im_im_a2 (0 x9_im_a2) resistor r=1.838536383247396e-11
ends inductor
