# gf180tovc.py 
# =============================================================================
# Convert GlobalFoundries gf180mcuD (ngspice) PDK to VACASK
# Based on VACASK's sg13g2tovc script
#
# =============================================================================
 
import sys, os, shutil, re
from pathlib import Path
from ng2vclib.converter import Converter
from ng2vclib.dfl import default_config
 
CONVERT_XSCHEM_SYMBOLS = False

tech_file = "sm141064.ngspice"
tech_file2 = ("design.ngspice", None, None, "design.lib")
output_path = "../vacask/models/"
# (section, read_process_depth, output_depth, output_name)
sections = [
    ("typical", None, None, "mos_tt.lib"),
    ("ff", None, None, "mos_ff.lib"),
    ("ss", None, None, "mos_ss.lib"),
    ("fs", None, None, "mos_fs.lib"),
    ("sf", None, None, "mos_sf.lib"),
    ("bjt_typical", None, None, "bjt_tt.lib"),
    ("bjt_ss", None, None, "bjt_ss.lib"),
    ("bjt_ff", None, None, "bjt_ff.lib"),
    ("diode_typical", None, None, "diode_tt.lib"),
    ("diode_ff", None, None, "diode_ff.lib"),
    ("diode_ss", None, None, "diode_ss.lib"),
    ("res_typical", None, None, "res_tt.lib"),
    ("res_ff", None, None, "res_ff.lib"),
    ("res_ss", None, None, "res_ss.lib"),
    ("mimcap_typical", None, None, "mimcap_tt.lib"),
    ("mimcap_ff", None, None, "mimcap_ff.lib"),
    ("mimcap_ss", None, None, "mimcap_ss.lib"),
    ("moscap_typical", None, None, "moscap_tt.lib"),
    ("moscap_ff", None, None, "moscap_ff.lib"),
    ("moscap_ss", None, None, "moscap_ss.lib"),
    ("statistical", None, None, "statistical.lib"),
    ("noise_corner", None, None, "noise_corner.lib")
]
 
## Known-bug patches template
patches = {
    "sm141064.ngspice": [
        (
            ".subckt nplus_u_m1 1 2 3 lr=lr wr=wr dtemp=0",
            ".subckt nplus_u_m1 1 2 3 lr=-1 wr=-1 dtemp=0"
        ),
        (
            ".subckt pplus_u_m1 1 2 3 lr=lr wr=wr dtemp=0",
            ".subckt pplus_u_m1 1 2 3 lr=-1 wr=-1 dtemp=0"
        ),
        (
            ".subckt pplus_u_m2 1 2 3 lr=lr wr=wr dtemp=0",
            ".subckt pplus_u_m2 1 2 3 lr=-1 wr=-1 dtemp=0"
        ),
    ],
}


family_map_update = {
    ("mos", 54, None): ("spice/bsim4v8.osdi", "sp_bsim4v8", {}),
    ("mos", 54, "4.5"): ("spice/bsim4v8.osdi", "sp_bsim4v8", {}),
    ("mos", 54, "4.6"): ("spice/bsim4v8.osdi", "sp_bsim4v8", {}),
}

remove_model_params_update = {
    "sp_bsim4v8": {
        "rgeomod"
    }
}

subckt_multiplier_update = {}
 
if __name__ == "__main__":
    # Environment
    pdkroot = os.getenv("PDK_ROOT")
    if pdkroot is None:
        print("The PDK_ROOT environmental variable must point to the PDK directory.")
        sys.exit(1)
    pdk = os.getenv("PDK")
    if pdk is None:
        pdk = "gf180mcuD"
 
    # Source directories
    tech_src = os.path.realpath(os.path.join(pdkroot, pdk, "libs.tech", "ngspice"))
    # Source directory (stdcell)
    stdcell_src = os.path.realpath(os.path.join(pdkroot, pdk, "libs.ref", "gf180mcu_fd_sc_mcu9t5v0", "spice"))
    # Source directory (io)
    io_src = os.path.realpath(os.path.join(pdkroot, pdk, "libs.ref", "gf180mcu_fd_io", "spice"))
 
    # Convert tech files
    osdi_files = set()
    dflmods = set()
    print("Converting technology files")
    
    # extra file
    file, read_process_depth, output_depth, output_name = tech_file2
    destpath = output_path + output_name
    cfg = default_config()
    cfg.update({
        "default_model_prefix": "gf180_default_mod_",
        "sourcepath": [".", tech_src, stdcell_src, io_src],
        "read_depth": read_process_depth,
        "process_depth": read_process_depth,
        "output_depth": output_depth,
        "patch": patches,
        "original_case_subckt": True,
        "original_case_model": True,
    })
    cfg["family_map"].update(family_map_update)
    cfg["remove_model_params"].update(remove_model_params_update)
    cfg["subckt_multiplier"].update(subckt_multiplier_update)
    cfg["signature"] = "// Converted from GlobalFoundries gf180mcuD PDK for Ngspice\n"
    
    cvt = Converter(cfg, indent=4, debug=1)
    cvt.convert(file, destpath)
        
    # multiple sections in the same file
    for section, read_process_depth, output_depth, output_name in sections:
        print(" ", tech_file, "section", section)
        destpath = output_path + output_name
 
        cfg = default_config()
        cfg.update({
            "default_model_prefix": "gf180_default_mod_",
            "sourcepath": [".", tech_src, stdcell_src, io_src],
            "read_depth": read_process_depth,
            "process_depth": read_process_depth,
            "output_depth": output_depth,
            "patch": patches,
            "original_case_subckt": True,
            "original_case_model": True,
        })
        cfg["family_map"].update(family_map_update)
        cfg["remove_model_params"].update(remove_model_params_update)
        cfg["subckt_multiplier"].update(subckt_multiplier_update)
        cfg["signature"] = "// Converted from GlobalFoundries gf180mcuD PDK for Ngspice\n"
    
        cvt = Converter(cfg, indent=4, debug=1)
        cvt.convert(tech_file, destpath, section)
        
        # OSDI files based on defined and used models
        for mname, in_sub in cvt.data["model_usage"]:
            if (None, in_sub) in cvt.data["models"]:
                # simple models
                if mname in cvt.data["models"][(None, in_sub)]:
                    builtin, mtype, family, level, version, _ = cvt.data["models"][(None, in_sub)][mname]
                    k = family, level, version
                    if k in cvt.cfg["family_map"]:
                        osdi_file, _, _ = cvt.cfg["family_map"][k]
                        osdi_files.add(osdi_file)
            else:
                # binned models
                if mname in cvt.data["bins"][(None, in_sub)]:
                    builtin, mtype, family, level, version, _ = cvt.data["bins"][(None, in_sub)][mname][0]
                    k = family, level, version
                    if k in cvt.cfg["family_map"]:
                        osdi_file, _, _ = cvt.cfg["family_map"][k]
                        osdi_files.add(osdi_file)
                
        # OSDI files based on builtin models
        for mt in cvt.data["default_models_needed"]:
            osdi_file, module = cvt.cfg["default_models"][mt]
            osdi_files.add(osdi_file)
            dflmods.add((mt, module))
        print("\n")

   
    # Create .vacaskrc.toml  
    # with output path prefixes and path to osdi modules
    print("Creating sample .vacaskrc.toml")
    vacask_cfg = """# VACASK configuration file
[Paths]
include_path_prefix = [
  "$(PDK_ROOT)/$(PDK)/libs.tech/vacask/models",
  "$(PDK_ROOT)/$(PDK)/libs.ref/gf180mcu_fd_sc_mcu9t5v0/vacask", 
  "$(PDK_ROOT)/$(PDK)/libs.ref/gf180mcu_fd_io/vacask" 
]
module_path_prefix = ["/opt/vacask/lib/vacask/mod"]
"""
    with open(os.path.join(pdkroot, pdk, "libs.tech", "vacask", ".vacaskrc.toml"), "w") as f:
        f.write(vacask_cfg)
 
 
    # Common include file (load statements + default model declarations)
    print("Creating common include file")
    txt = ""
    if len(osdi_files) > 0:
        txt += "// OSDI files\n"
        for f in sorted(list(osdi_files)):
            txt += "load \"" + f + "\"\n"
        if len(dflmods) > 0:
            txt += "\n"
    if len(dflmods) > 0:
        txt += "// Default models\n"
        for mt, module in sorted(dflmods, key=lambda p: p[0]):
            txt += "model " + cfg["default_model_prefix"] + mt + " " + module + "\n"
 
    common_include = os.path.realpath(
        os.path.join(tech_src, "..", "vacask", "models", "gf180_vacask_common.lib")
    )
    print(" ", common_include)
    os.makedirs(os.path.dirname(common_include), exist_ok=True)
    with open(common_include, "w") as f:
        f.write(txt)
 
#     # Xschem symbol conversion
#     if CONVERT_XSCHEM_SYMBOLS:


    print("\nDone")
 
