from .exc import ConverterError

rb_verilog_a =    """
  declarations=<<<DECL
    parameter real w=10e-6;
    parameter real l=10e-6;
    parameter real temp=27;
    parameter real dtemp=0;
    parameter real kf = 0;
    parameter real af = 1;
    parameter real ef = 1;
    parameter real short = 0;
    parameter real narrow = 0;
    parameter real lf = 1;
    parameter real wf = 1;
    parameter integer noise = 1;
    real devTemp;
    real noiseArea;
    real cpscale = $simparam(\"scale\", 1);
    real r;
    real i;
  >>>DECL
  evaluation=<<<EVAL
    if ($param_given(temp)) devTemp = temp+`P_CELSIUS0;
    else devTemp = $temperature + dtemp;
    if ($param_given(w) || $param_given(l)) begin
      noiseArea = pow(l*cpscale-2*short, lf)*pow(w*cpscale-2*narrow, wf);
    end else begin
      noiseArea = 1;
    end
    r = #expr#;
    i = V(br)/r;\n
    I(br) <+ i;
    if (noise) begin
      I(br) <+ white_noise(4*`P_K*devTemp/r, \"thermal\");
      I(br) <+ flicker_noise(kf*pow(i, af)/noiseArea, ef, \"flicker\");
    end
  >>>EVAL
"""

class InstancePassiveMixin:
    def process_instance_r(self, lws, line, eol, annot, in_sec, in_sub):
        """
        Process R instance (resistor). 

        rname p n <r=value>|<value> [<p1=value1> <p2=value2> ...]
        rname p n <model> [<p1=value1> <p2=value2> ...]
        rname p n <value> <model> [<p1=value1> <p2=value2> ...]
        """
        name = annot["name"]
        parts = annot["words"]
        mod_index = annot["mod_index"]
        model = annot["mod_name"]

        behavioural = True if name[1]=='b' else False
        
        terminals = self.process_terminals(parts[:2])

        if model is None:
            # No model specified
            if not behavioural:
                model = self.cfg["default_model_prefix"]+"r"
                self.data["default_models_needed"].add("r")

            # Check if part 3 is not a parameter assignment
            if "=" not in parts[2]:
                # Part 3 is the resistance, the rest are parameter assignments
                params = parts[3:]

                # Add value as first parameter assignment
                psplit = [("r", self.format_value(parts[2]))]
            else:
                # Part 3 is a parameter assignment
                params = parts[2:]

                # No parameter assignments yet
                psplit = []
            
        else:
            # Have model
            if mod_index==2:
                # Third entry, immediately after terminals, no value
                psplit = []
                model = annot["output_mod_name"]
                params = parts[3:]
            elif mod_index==3:
                # Model is 4th entry, 3rd entry must be a value
                psplit = [("r", self.format_value(parts[2]))]
                model = annot["output_mod_name"]
                params = parts[4:]
            else:
                # Don't know how to handle
                raise ConverterError("Cannot handle model at position "+str(mod_index+1)+".")

        # Process parameters
        psplit += self.process_instance_params(params, "r", handle_m=True, in_sub=in_sub)

        # behavioural resistor into behavioural source
        if behavioural:
            if model is None:
                # behavioural resistors without model do not hget the default model
                for ii,p in enumerate(psplit):
                    # convert to behavioural current source
                    if p[0] == "r":
                        psplit[ii] = ("i", "v(" + parts[0] + "," + parts[1] + ")/(" + psplit[0][1] + ")")
                        break
                txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") "
            else:
                # behavioural resistors with model require Verilog-A
                for ii,p in enumerate(psplit):
                    # remove expression from parameters
                    new_psplit = []
                    if p[0] == "r":
                        expr = p[1]
                    else:
                        new_psplit.append(p)
                # get model parameters. builtin, mtype, family, level, version, params 
                _, _, _, _, _, model_params = self.data["models"][(in_sec, in_sub)][model]
                for p in model_params:
                    new_psplit.append(p)
                psplit = new_psplit
                txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") (\n  expr=" + expr + rb_verilog_a + "), "

        # normal resistor
        else:    
            txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+model+" "

        if len(psplit)>0:
            fmted, need_split, split = self.format_params(psplit, len(txt))
            if need_split or split:
                fmted = self.indent(fmted, len(lws)+2)
                txt += "(" + eol + "\n" + fmted
                txt += "\n" + lws + ")"
            else:
                txt += " " + fmted
        
        return txt

    def process_instance_c(self, lws, line, eol, annot, in_sec, in_sub):
        """
        Process C instance (capacitor). 

        cname p n <value> [<p1=value1> <p2=value2> ...]
        cname p n <model> [<p1=value1> <p2=value2> ...]
        cname p n <value> <model> [<p1=value1> <p2=value2> ...]

        Removes ic parameter. 
        """
        name = annot["name"]
        parts = annot["words"]
        mod_index = annot["mod_index"]
        model = annot["mod_name"]
        
        terminals = self.process_terminals(parts[:2])

        if model is None:
            # No model specified
            model = self.cfg["default_model_prefix"]+"c"
            self.data["default_models_needed"].add("c")

            # Part 3 is a parameter assignment
            params = parts[2:]

            # No parameter assignments yet
            psplit = []
        else:
            # Have model
            if mod_index==2:
                # Third entry, immediately after terminals, no value
                psplit = []
                model = annot["output_mod_name"]
                params = parts[3:]
            elif mod_index==3:
                # Model is 4th entry, 3rd entry must be a value
                psplit = [("c", self.format_value(parts[2]))]
                model = annot["output_mod_name"]
                params = parts[4:]
            else:
                # Don't know how to handle
                raise ConverterError("Cannot handle model at position "+str(mod_index+1)+".")

        # Process parameters
        psplit += self.process_instance_params(params, "c", handle_m=True, in_sub=in_sub)
                
        txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+model+" "

        if len(psplit)>0:
            fmted, need_split, split = self.format_params(psplit, len(txt))
            if need_split or split:
                fmted = self.indent(fmted, len(lws)+2)
                txt += "(" + eol + "\n" + fmted
                txt += "\n" + lws + ")"
            else:
                txt += " " + fmted
        
        return txt

    def process_instance_l(self, lws, line, eol, annot, in_sec, in_sub):
        """
        Process L instance (inductor). 

        lname p n <value> [<p1=value1> <p2=value2> ...]
        lname p n <model> [<p1=value1> <p2=value2> ...]
        lname p n <value> <model> [<p1=value1> <p2=value2> ...]

        Removes ic parameter. 
        """
        name = annot["name"]
        parts = annot["words"]
        mod_index = annot["mod_index"]
        model = annot["mod_name"]
        
        terminals = self.process_terminals(parts[:2])

        if model is None:
            # No model specified
            model = self.cfg["default_model_prefix"]+"l"
            self.data["default_models_needed"].add("l")

            # Part 3 is a parameter assignment
            params = parts[2:]

            # No parameter assignments yet
            psplit = []
        else:
            # Have model
            if mod_index==2:
                # Third entry, immediately after terminals, no value
                psplit = []
                model = annot["output_mod_name"]
                params = parts[3:]
            elif mod_index==3:
                # Model is 4th entry, 3rd entry must be a value
                psplit = [("l", self.format_value(parts[2]))]
                model = annot["output_mod_name"]
                params = parts[4:]
            else:
                # Don't know how to handle
                raise ConverterError("Cannot handle model at position "+str(mod_index+1)+".")

        # Process parameters
        psplit += self.process_instance_params(params, "l", handle_m=True, in_sub=in_sub)
                
        txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+model+" "

        if len(psplit)>0:
            fmted, need_split, split = self.format_params(psplit, len(txt))
            if need_split or split:
                fmted = self.indent(fmted, len(lws)+2)
                txt += "(" + eol + "\n" + fmted
                txt += "\n" + lws + ")"
            else:
                txt += " " + fmted
        
        return txt
