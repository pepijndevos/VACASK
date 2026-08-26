# Convert IHP PDK for sg13g2 to VACASK format
# For now it does nnot handle the mismatch models, stay tuned. 

# Environmental variables
# PDK_ROOT .. directory created by cloning the PDK 
# PDK .. subdirectory with the PDK, by default ihp-sg13g2

import sys, os, platform, subprocess, shutil, re
from pathlib import Path
from pprint import pprint
from ng2vclib.converter import Converter
from ng2vclib.dfl import default_config
import xschem2vc

# Default destination relative to original file
tech_dest_dir = "../../vacask/models"

tech_files = [
    # file  read&process depth  output depth  destination (relative path)
    ( "capacitors_mod.lib", 1, None ), 
    ( "capacitors_mod_mismatch.lib", 1, None ), 
    ( "capacitors_stat.lib", 1, None ), 
    
    ( "cornerCAP.lib", 0, 0 ),  
    ( "cornerDIO.lib", 0, 0 ),  
    ( "cornerHBT.lib", 0, 0 ), 
    ( "cornerMOShv.lib", 0, 0 ), 
    ( "cornerMOSlv.lib", 0, 0 ), 
    ( "cornerRES.lib", 0, 0 ), 
    
    ( "diodes.lib", 1, None ), 
    
    ( "resistors_mod.lib", 1, None ), 
    ( "resistors_mod_mismatch.lib", 1, None ), 
    ( "resistors_stat.lib", 1, None ), 
    
    ( "sg13g2_bondpad.lib", 1, None ), 
    
    ( "sg13g2_dschottky_nbl1_mod.lib", 1, None ), 
    
    ( "sg13g2_esd.lib", 1, None ), 
    
    ( "sg13g2_hbt_mod.lib", 1, None ), 
    ( "sg13g2_hbt_mod_mismatch.lib", 1, None ), 
    ( "sg13g2_hbt_stat.lib", 1, None ), 
    
    ( "sg13g2_moshv_mismatch.lib", 1, None ),  
    ( "sg13g2_moshv_mod.lib", 1, None ), 
    ( "sg13g2_moshv_mod_mismatch.lib", 1, None), 
    # "sg13g2_moshv_parm.lib", # flattened
    ( "sg13g2_moshv_stat.lib", 1, None ), 
    
    ( "sg13g2_moslv_mismatch.lib", 1, None), 
    ( "sg13g2_moslv_mod.lib", 1, None ), 
    ( "sg13g2_moslv_mod_mismatch.lib", 1, None ), 
    # "sg13g2_moslv_parm.lib", # flattened
    ( "sg13g2_moslv_stat.lib", 1, None ), 
    
    ( "sg13g2_svaricaphv_mod.lib", 1, None ),  
    ( "sg13g2_svaricaphv_mod_mismatch.lib", 1, None), 

    # Standard cells and I/O, non-default destination
    ( "sg13g2_stdcell.spice", 1, None, "../vacask/sg13g2_stdcell.inc" ), 
    ( "sg13g2_io.spice", 1, None, "../vacask/sg13g2_io.inc" ), 
]

def patch_dig(line):
    if "format" not in line:
        return None
    
    line = line.replace("format=", "spectre_format=")

    # Split 
    parts = line.split()

    if len(parts)<2:
        return line

    # Look for @prefix
    at = None
    for ii, p in enumerate(parts):
        if p.startswith("@prefix"):
            at = ii
            break
    
    # Not found
    if at is None:
        return line
    
    # Put terminals in parentheses
    parts = parts[:1] + [ "(" ] + parts[1:at] + [ ")" ] + parts[at:] + [ "\n" ] 

    return " ".join(parts)

pat_pinlist_body = re.compile(r'(@pinlist\s*@body)')
pat_model = re.compile(r'@model')
pat_identifier_assign = re.compile(r'[A-Za-z_][A-Za-z0-9_]*=')

# 'format=' translator for analog symbols
def patch_analog(line):
    """
    @pinlist @body -> ( @pinlist @body ) 
    @pinlist -> ( @pinlist ) 
    
    Anything after @model of the form <identifier>= -> <lowercase identifier>=
    """
    if "format" not in line:
        return None
    
    # Search for pins and body pin
    m = pat_pinlist_body.search(line)

    if m:
        # Put parentheses around the matched pattern
        line = line[:m.start()] + '( ' + m.group(0) + ' )' + line[m.end():]
    else:
        # Parenthesize @pinlist
        line = line.replace("@pinlist", "( @pinlist )")

    # Find @model
    m = pat_model.search(line)
    if m:
        # Cut in two parts
        line1 = line[:m.end()]
        line2 = line[m.end():]

        # In line 2 find all occurences of <identifier>=, lowercase them
        m1 = list(pat_identifier_assign.finditer(line2))
        for m in reversed(m1):
            text = m.group(0).lower()
            line2 = line2[:m.start()] + text + line2[m.end():]
        line = line1 + line2

    return line.replace("format=", "spectre_format=")

patches = {
    # A bug in Ngspice sg13g2_esd.lib
    "sg13g2_esd.lib": [
        (
            ".MODEL diodevss_mod D (tnom = 27 level = 1 is=9.017E-019 rs=200   n=1.03 isr=3.776E-015   ikf=0.0001754 cj0=9.42E-016  m=0.3012  vj=0.6684 bv=11.28 ibv=1E-009 8 nbv=1.324   eg=1.17 xti=3  )", 
            ".MODEL diodevss_mod D (tnom = 27 level = 1 is=9.017E-019 rs=200   n=1.03 isr=3.776E-015   ikf=0.0001754 cj0=9.42E-016  m=0.3012  vj=0.6684 bv=11.28 ibv=1E-009 nbv=1.324   eg=1.17 xti=3  )"
        ), 
    ],
    # A bug in sg13g2_svaricaphv_mod.lib
    "sg13g2_svaricaphv_mod.lib": [
        (
            "+ stuac 40", 
            "+ stuac=40"
        )
    ],
    # Remove global SWSOA parameter
    "sg13g2_moshv_mod.lib": [
        (
            ".param SWSOA = 0", 
            ""
        )
    ], 
    "sg13g2_moshv_mod_mismatch.lib": [
        (
            ".param SWSOA = 0", 
            ""
        )
    ], 
    "sg13g2_moslv_mod.lib": [
        (
            ".param SWSOA = 0", 
            ""
        )
    ], 
    "sg13g2_moslv_mod_mismatch.lib": [
        (
            ".param SWSOA = 0", 
            ""
        )
    ], 
}

family_map_update = {
    ("bjt",   4,    None):     ( "vbic_1p3_5t.osdi",     "vbic13_5t",    {} ), 
    ("bjt",   9,    None):     ( "vbic_1p3_5t.osdi",     "vbic13_5t",    {} ), 
    ("bjt",   1,    None):     ( "spice/full/bjt.osdi",  "sp_bjt",       {} ), 
    ("bjt",   None, None):     ( "spice/full/bjt.osdi",  "sp_bjt",       {} ), 
}

remove_model_params_update = {
    # translated device name: set of param names
    "vbic13_5t": set(["vbe_max", "vbc_max", "vce_max"])
}

subckt_multiplier_update = {
    "cap_cmim": [ "m", True ], 
    "cap_rfcmim": [ "m", True ], 
}

va_patches = {
    "r3_cmc/r3_cmc.va": [
        (
            "if (abs(Irb/weff_um)>jmax) begin", 
            "`ifdef SOA_CHECK\n        if (abs(Irb/weff_um)>jmax) begin"
        ), 
        (
            """if (abs(Vc2)>vmax) begin
            $warning("WARNING: V(i2,c) voltage is greater than specified by vmax");
        end""", 
            """if (abs(Vc2)>vmax) begin
            $warning("WARNING: V(i2,c) voltage is greater than specified by vmax");
        end
        `endif"""
        )

    ]
}

included_va_files = [
    # file                                     options
    ( "r3_cmc/r3_cmc.va",     [ "-D__NGSPICE" ] ), 
    ( "psp103/psp103.va",     [ "-D__NGSPICE" ] ), 
    ( "psp103/psp103_nqs.va", [ "-D__NGSPICE" ] ), 
    ( "mosvar/mosvar.va",     [ "-D__NGSPICE" ] ), 
]

if __name__=="__main__":
    # Collect options
    # --openvaf-options .. all options that follow are passed to OpenVAF
    # Print a usage note if unknown option is given
    openvaf_cmdline_options = []
    args = sys.argv[1:]
    ii = 0
    while ii < len(args):
        if args[ii] == "--openvaf-options":
            openvaf_cmdline_options = args[ii + 1:]
            break
        else:
            print(f"Unknown option: {args[ii]}")
            print(f"Usage: {sys.argv[0]} [--openvaf-options <opt1> <opt2> ...]")
            sys.exit(1)
        ii += 1

    # Get PDK_ROOT environmental variable
    pdkroot = os.getenv("PDK_ROOT")
    if pdkroot is None:
        print("The PDK_ROOT environmental variable must point to the PDK directory.")
        sys.exit(1)

    pdk = os.getenv("PDK")
    if pdk is None:
        pdk = "ihp-sg13g2"

    openvafdir = os.getenv("OPENVAF_DIR")

    #
    # Technology files and standard cells
    #

    # Source directory (tech)
    tech_src = os.path.abspath(os.path.join(pdkroot, pdk, "libs.tech", "ngspice", "models"))

    # Source directory (stdcell)
    stdcell_src = os.path.abspath(os.path.join(pdkroot, pdk, "libs.ref", "sg13g2_stdcell", "spice"))

    # Source directory (io)
    io_src = os.path.abspath(os.path.join(pdkroot, pdk, "libs.ref", "sg13g2_io", "spice"))
        
    # Go through tech files and convert
    osdi_files = set()
    dflmods = set()
    print("Converting technology files and standard cells")
    for tech_file in tech_files:
        if len(tech_file)==4:
            file, read_process_depth, output_depth, destpath = tech_file 
        else:
            file, read_process_depth, output_depth = tech_file 
            destpath = os.path.join(tech_dest_dir, file)

        print(" ", file)
        cfg = default_config()
        cfg.update({
            "default_model_prefix": "sg13g2_default_mod_", 
            "sourcepath": [ ".", tech_src, stdcell_src, io_src ], 
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
        cfg["signature"] = "// Converted from IHP SG13G2 PDK for Ngspice\n"

        cvt = Converter(cfg, indent=4, debug=1)
        cvt.convert(file, destpath)
        
        # OSDI files based on defined and used models
        for mname, in_sub in cvt.data["model_usage"]:
            builtin, mtype, family, level, version, _ = cvt.data["models"][in_sub][mname]
            k = family, level, version
            if k in cvt.cfg["family_map"]:
                osdi_file, _, _ = cvt.cfg["family_map"][k]
                osdi_files.add(osdi_file)

        # OSDI files based on builtin models
        for mt in cvt.data["default_models_needed"]:
            osdi_file, module = cvt.cfg["default_models"][mt]
            osdi_files.add(osdi_file)
            dflmods.add((mt, module))
    
    # Create .vacaskrc.toml
    print("Creating sample .vacaskrc.toml")
    vacask_cfg="""# VACASK configuration file 
[Paths]
include_path_prefix = [ 
  "$(PDK_ROOT)/$(PDK)/libs.tech/vacask/models", 
  "$(PDK_ROOT)/$(PDK)/libs.ref/sg13g2_stdcell/vacask", 
  "$(PDK_ROOT)/$(PDK)/libs.ref/sg13g2_io/vacask" 
]
module_path_prefix = [ "$(PDK_ROOT)/$(PDK)/libs.tech/vacask/osdi" ]
"""
    with open(os.path.join(pdkroot, pdk, "libs.tech", "vacask", ".vacaskrc.toml"), "w") as f:
        f.write(vacask_cfg)

    #
    # Xschem symbol conversion
    #

    # Process xschem symbol files
    xschem_path_pfx = os.path.abspath(os.path.join(pdkroot, pdk, "libs.tech", "xschem"))

    symfiles = []

    # All .sym files in xschem_path_pfx/sg13g2_pr
    directory = Path(xschem_path_pfx) / "sg13g2_pr"
    files = [str(p.resolve()) for p in directory.iterdir() if p.is_file() and p.suffix == ".sym"]
    symfiles += [[f, patch_analog] for f in files]

    # All .sym files in sg13g2_stdcells
    # Try new path first
    directory = Path(pdkroot) / pdk / "libs.ref" / "sg13g2_stdcell" / "sym" / "xschem"
    # To be removed
    if not directory.is_dir():
        # If it is not found, try old path
        directory = Path(xschem_path_pfx) / "sg13g2_stdcells"
    # End of to be removed
    files = [str(p.resolve()) for p in directory.iterdir() if p.is_file() and p.suffix == ".sym"]
    symfiles += [[f, patch_dig] for f in files]

    print("Processing Xschem symbol files")
    for fn, symcvt in symfiles:
        print(" ", fn)

        fname = os.path.join(xschem_path_pfx, fn)
        try:
            if symcvt is None:
                xschem2vc.convert(fname)
            else:
                xschem2vc.convert(fname, symcvt)
        except:
            print("    FAILED", )
            raise
            
    # 
    # Compilation of .va files
    #

    # Platform
    system = platform.system()
    if system=="Windows":
        openvaf_bin = "openvaf-r.exe"
    else:
        openvaf_bin = "openvaf-r"

    # Find this module
    module_path = os.path.dirname(os.path.abspath(__file__))

    # Find OpenVAF
    openvaf = None
    candidates = [ openvafdir ] if openvafdir is not None else []
    candidates += [
        # <rootdir>/VACASK/python (VS Code build system)
        os.path.join("..", "..", "build.VACASK", "Release", "simulator"), 
        os.path.join("..", "..", "build.VACASK", "Debug", "simulator"), 
        # <rootdir>/lib/vacask/python (Linux)
        os.path.join("..", "..", "..", "bin"), 
        # <rootdir>/lib/python (Windows)
        os.path.join("..", "..", "bin"), 
    ]

    print("Looking for OpenVAF-Reloaded candidate")
    for cand in candidates:
        if os.path.isabs(cand):
            d = cand
        else:
            d = os.path.join(module_path, cand)
        f = os.path.join(d, openvaf_bin)
        f = os.path.abspath(f)
        print(" ", d, ":", f)
        if (os.path.isdir(d) and os.path.isfile(f)):
            openvaf = f
            print("Found in", d)
            break
    
    if openvaf is None:
        print("OpenVAF-Reloaded not found.")
        sys.exit(1)
    
    # Modules directory
    mdir = os.path.join(pdkroot, pdk, "libs.tech", "vacask", "osdi")
    os.makedirs(mdir, exist_ok=True)
    
    # Compile modules
    d = os.path.join(pdkroot, pdk, "libs.tech", "verilog-a")
    lead_path = os.path.normpath(mdir)
    for fi, extra_opts in included_va_files:
        f = os.path.join(d, fi)
        fbase = f

        if fi in va_patches:
            # Rename 
            root, ext = os.path.splitext(f)
            fpatched = f"{root}-patched{ext}"

            # Load
            with open(f, 'r') as file:
                content = file.read()

            # Apply patches
            for patch in va_patches[fi]:
                olds, news = patch
                content = content.replace(olds, news)

            # Write
            with open(fpatched, 'w', encoding='utf-8') as file:
                file.write(content)

            # Set new input file name
            f = fpatched            
        
        fb = os.path.basename(fbase)
        fo = os.path.join(mdir, fb[:-3]+".osdi")
        
        # Remove leading part from fo, add to list
        fo_n = os.path.normpath(fo)
        fo_r = os.path.relpath(fo_n, lead_path)
        osdi_files.add(fo_r)
        
        print("Compiling", f)
        cmdline = [ openvaf ] + extra_opts + openvaf_cmdline_options + [ "-o", fo, f ]
        # print(cmdline)
        retval = subprocess.run(cmdline)
        if retval.returncode != 0:
            print("Verilog-A compiler error.")
            sys.exit(1)

    #
    # VACASK specific include file
    #

    # Create an include file with common loads and models
    print("Creating common include file")
    
    txt = ""
    if len(osdi_files)>0:
        txt += "// Disable SOA checks\n"
        txt += "parameters swsoa=0\n"
        txt += "// OSDI files\n"
        for f in sorted(list(osdi_files)):
            txt += "load \""+f+"\"\n"
        if len(dflmods)>0:
            txt +="\n"
    if len(dflmods)>0:
        txt += "\n// Default models\n"
        for mt, module in sorted(dflmods, key=lambda p: p[0]):
            txt += "model "+cvt.cfg["default_model_prefix"]+mt+" "+module+"\n"
    
    common_include = os.path.join(tech_src, "..", "..", "vacask", "models", "sg13g2_vacask_common.lib")
    common_include = os.path.abspath(common_include)
    print(" ", common_include)
    with open(common_include, "w") as f:
        f.write(txt)

    #
    # Xschem config patcher
    # 

    print("Patching xschem configuration")

    # Original config and old version
    xsch = os.path.join(pdkroot, pdk, "libs.tech", "xschem", "xschemrc")
    xschorig = os.path.join(pdkroot, pdk, "libs.tech", "xschem", "xschemrc.orig")

    # Look for old version
    print(" ", xsch)
    if not os.path.isfile(xschorig):
        # Copy
        shutil.copy(xsch, xschorig)
    
    # Read orig file
    orig_lines = []
    with open(xschorig, "r") as f:
        for l in f:
            orig_lines.append(l)
    
    # Write updated file, add VACASK specific part
    with open(xsch, "w") as f:
        for l in orig_lines:
            f.write(l)
        
        f.write("""
# VACASK support
if {[info exists PDK_ROOT]} {
  if {[info exists PDK]} {
    if {[file exists $PDK_ROOT/$PDK/libs.tech/xschem/xschem-vacask]} {
      source $PDK_ROOT/$PDK/libs.tech/xschem/xschem-vacask
    }
  }
}

# Show netlist
# set netlist_show 1

# Netlist type
if {[info exists env(XSCHEM_NETLIST_TYPE)]} {
  puts "Netlist mode: $::env(XSCHEM_NETLIST_TYPE)"
  set netlist_type $::env(XSCHEM_NETLIST_TYPE)
} else {
  puts "Netlist mode: <default>"
}
""")

    # Write xschemrc extension for VACASK
    xschext = os.path.join(pdkroot, pdk, "libs.tech", "xschem", "xschem-vacask")
    print(" ", xschext)

    # Get source file
    src = os.path.join(os.path.dirname(__file__), "sg13g2xschem.tcl")
    shutil.copy(src, xschext)
    