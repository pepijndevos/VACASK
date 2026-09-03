
import re
from .generators import traverse
from .exc import ConverterError
from .patterns import *

class MastersMixin:
    pat_paramassign = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*=')

    def split_instance(self, line, orig_line, in_sec=None, in_sub=None):
        """
        Splits an instance into fragments, extract model position. 

        Returns 
        * name
        * name with original case
        * a list of parts 
        * a list of parts with original case
        * index of first model in the list of parts, None if no model found. 

        Tracks model usage. 
        """
        parts = line.split()
        orig_parts = orig_line.split()
        
        # Is it a subcircuit instance? 
        name = parts[0]
        issubinst = name[0]=="x"

        # Find first model
        mod_index = None
        for ndx, part in enumerate(parts):
            # Skip first fragment (name)
            if ndx==0:
                continue

            if issubinst:
                # Subcircuit instance
                # Look for first entry of the form name=value
                if "=" in part:
                    # Found first parameter
                    mod_index = ndx-1
                    break
            else:
                # Device instance
                # Check local models first
                if in_sub is not None:
                    local_model_found = (in_sec, in_sub) in self.data["models"] and part in self.data["models"][(in_sec, in_sub)]
                    local_binned_model_found = (in_sec, in_sub) in self.data["bins"] and part in self.data["bins"][(in_sec, in_sub)]
                    if local_model_found or local_binned_model_found:
                        # Found local model
                        mod_index = ndx
                        key = (part, in_sub)
                        if key not in self.data["model_usage"]:
                            self.data["model_usage"][key] = set()
                        self.data["model_usage"][key].add(in_sub)
                        break
                # Check global models
                global_model_found = (in_sec, None) in self.data["models"] and part in self.data["models"][(in_sec, None)]
                global_binned_model_found = (in_sec, None) in self.data["bins"] and part in self.data["bins"][(in_sec, None)]
                if global_model_found or global_binned_model_found:
                    # Found global model
                    mod_index = ndx
                    key = (part, None)
                    if key not in self.data["model_usage"]:
                        self.data["model_usage"][key] = set()
                    self.data["model_usage"][key].add(in_sub)
                    break
        
        # No parameters found for a subcircuit instance
        if issubinst and mod_index is None:
            # Last word is the subcircuit definition name
            mod_index = len(parts)-1

        return parts, orig_parts, mod_index
    
    def preprocess_instance(self, lnum, lws, line, eol, annot, in_sec, in_sub):
        """
        First stage of instance procesing. 
        
        Split card into name and words list, find model. 
        Store name, list of words, and model index in annotations. 
        
        Track used models. 
        """
        parts, orig_parts, mod_index = self.split_instance(line, annot["origline"], in_sec, in_sub)

        # Extract name and original name
        name = parts[0]
        orig_name = orig_parts[0]

        # Remove first fragment (name) from parts
        parts = parts[1:]
        orig_parts = orig_parts[1:]
        if mod_index is not None:
            mod_index -= 1
        
        # Replace [] with _ in name and orignal name
        name = name.replace("[", "_")
        name = name.replace("]", "_")
        orig_name = orig_name.replace("[", "_")
        orig_name = orig_name.replace("]", "_")

        annot["name"] = name
        annot["orig_name"] = orig_name
        annot["words"] = parts
        annot["orig_words"] = orig_parts
        annot["mod_index"] = mod_index
        annot["mod_name"] = parts[mod_index] if mod_index is not None else None
        annot["orig_mod_name"] = orig_parts[mod_index] if mod_index is not None else None

        # Instance name for output
        if self.cfg.get("original_case_instance", False):
            annot["output_name"] = annot["orig_name"]
        else:
            annot["output_name"] = annot["name"]

        # Model name for output (for instances)
        if self.cfg.get("original_case_model", False):
            annot["output_mod_name"] = annot["orig_mod_name"]
        else:
            annot["output_mod_name"] = annot["mod_name"]

    def collect_masters(self):
        """
        Colects defined subcircuits and models from deck.  
        """
        deck = self.data["deck"]
        self.data["bins"] = {}  

        in_sub = None
        in_sec = None

        # Pass 1 - collect models
        # Pass 2 - preprocess instances, construct model usage table
        for passno in [1, 2]:
            for history, line, depth, in_control_block in traverse(deck, depth=self.cfg.get("process_depth", None)):
                if in_control_block and passno==1:
                    # In control block, pass 1, look for pre_osdi
                    lnum, lws, l, eolc, annot = line
                    l1 = l.strip()
                    if l1.startswith("*"):
                        l1 = l1[1:].strip()
                        if pat_preosdi.match(l1):
                            # Have a pre_osdi, add to list of files
                            osdi_file = l1[8:].strip()
                            self.data["osdi_loads"].add(osdi_file)
                    continue

                lnum, lws, l, eolc, annot = line

                if pat_cidotlib.match(l):
                    # Lib
                    name, section, subdeck = eolc
                    if name is None:
                        # Section start marker
                        in_sec = section
                elif pat_cidotendl.match(l):
                    # End of section marker
                    in_sec = None
                elif isinstance(eolc, tuple):
                    # If eolc is a tuple this is an .include/.lib line
                    # We do not process .include 
                    continue
                elif l.startswith("*"):
                    # Comment
                    continue
                elif len(l)==0:
                    # Empty line
                    continue
                elif pat_cidotsubckt.match(l):
                    # Subcircuit
                    parts = l.split(" ")
                    in_sub = parts[1]

                    if passno==1:
                        # Find first parameter
                        first_param = len(parts)
                        for ndx, s in enumerate(parts):
                            if self.pat_paramassign.match(s):
                                first_param = ndx
                                break
                        
                        # Get terminals
                        terminals = parts[2:first_param]

                        # Get parameters, split into name and value
                        sub_params = [ p.split("=") for p in parts[first_param:]]

                        # Store
                        self.data["subckts"][in_sub] = (terminals, sub_params)
                elif pat_cidotends.match(l):
                    # End of subcircuit
                    in_sub = None
                elif pat_cidotmodel.match(l):
                    if passno==1:
                        # Detect model
                        parts = l.split(" ")
                        name = parts[1]
                        mtype = parts[2]
                        params = parts[3:]
                        
                        annot["name"] = name

                        # Split params into name-value pairs
                        params = self.split_params(params, handle_m=False)

                        # Process expressions
                        params = self.process_expressions(params)

                        # Get information on model type
                        family = None
                        level = None
                        version = None
                        builtin = False
                        if mtype in self.cfg["type_map"]:
                            # Builtins needs special handling
                            builtin = True
                            extra_params, family, remove_level, remove_version = self.cfg["type_map"][mtype]
                            
                            # Collect level and version and remove them if requested
                            pnew = []
                            for pname, pval in params:
                                if pname=="level":
                                    level = int(pval)
                                    if remove_level:
                                        continue
                                elif pname=="version":
                                    version = pval.strip()
                                    if (
                                        version.startswith("'") and version.endswith("'") or
                                        version.startswith('"') and version.endswith('"')
                                    ):
                                        version = version[1:-1]
                                    if remove_version:
                                        continue
                                else:
                                    pnew.append((pname, pval))
                            params = pnew

                        # Add to list of bins in binned models dictionary
                        if "." in name:
                            name = name.split(".")
                            bin_number = name[1]
                            name = name[0]
                            if (in_sec, in_sub) not in self.data["bins"]:
                                self.data["bins"][(in_sec, in_sub)] = {}
                            if name not in self.data["bins"][(in_sec, in_sub)]:
                                self.data["bins"][(in_sec, in_sub)][name] = []
                            self.data["bins"][(in_sec, in_sub)][name].append((
                                builtin, mtype, family, level, version, params
                            ))
                            annot["name"] = name
                        # Add to models dictionary
                        else:
                            if (in_sec, in_sub) not in self.data["models"]:
                                self.data["models"][(in_sec, in_sub)] = {}
                            self.data["models"][(in_sec, in_sub)][name] = (
                                builtin, mtype, family, level, version, params
                            )
                elif not l.startswith("."):
                    if passno==2:
                        # Not a dot command, must be an instance
                        # Preprocess it
                        try:
                            self.preprocess_instance(*line, in_sec, in_sub)
                        except ConverterError as e:
                            raise ConverterError(str(e), history, lnum)

        if self.debug>1:
            print("\nModels:") 
            for scope in self.data["models"]:
                print(scope)
                if isinstance(self.data["models"][scope], dict):
                    for mod in self.data["models"][scope]:
                        print("\t"+mod)
            print("\nBinned models:") 
            for scope in self.data["bins"]:
                print(scope)
                if isinstance(self.data["bins"][scope], dict):
                    for mod in self.data["bins"][scope]:
                        print("\t"+mod+" "+str(len(self.data["bins"][scope][mod])))
            print("\nModels usage:") 
            print(self.data["model_usage"])
            print("\n") 



                
