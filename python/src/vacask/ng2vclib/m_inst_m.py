from .exc import ConverterError

class InstanceMMixin:
    def process_instance_m(self, lws, line, eol, annot, in_sec, in_sub):
        """
        Process M instance (transistor).
        
        mname d g s b <model> [<p1=value1> <p2=value2> ...]
        
        If binned model:
        
        @if (l>= && l< && w>= && w< )
            mname d g s b <model>__<0> [<p1=value1> <p2=value2> ...]
        @elif (l>= && l< && w>= && w< )
            mname d g s b <model>__<1> [<p1=value1> <p2=value2> ...]
        @else
            
        @endif
        
        """
        
        name = annot["name"]
        parts = annot["words"]
        mod_index = annot["mod_index"]
        model = annot["mod_name"]
        
        if model is None:
            raise ConverterError(line+"\nModel not found.")
        
        terminals = self.process_terminals(parts[:4])
        params = parts[5:]
        
        # Process parameters
        psplit = self.process_instance_params(params, "m", handle_m=True, in_sub=in_sub)
        
        if model in self.data["bins"][(in_sec, None)]:
            # global binned module
            N_bins = len(self.data["bins"][(in_sec, None)][model])
            txt_len = len(lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+annot["output_mod_name"]+" ")
            for i in range(N_bins):
                _,_,_,_,_, bin_params = self.data["bins"][(in_sec, None)][model][i]
                bin_num = "__"+str(i)
                lmin, lmax, wmin, wmax = self.get_bin_boundaries(bin_params)
                boundaries = "(l>="+lmin+" && l<"+lmax+" && w>="+wmin+" && w<"+wmax+")"
                if i == 0:
                    txt = "@if "+boundaries+"\n"
                else:
                    txt += "@elseif "+boundaries+"\n"
                txt += "  "+lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+annot["output_mod_name"]+bin_num+" "
                
                if len(psplit)>0:
                    fmted, need_split, split = self.format_params(psplit, txt_len - len(bin_num))
                    if need_split or split:
                        fmted = self.indent(fmted, len(lws)+4)
                        txt += "(" + eol + "\n" + fmted
                        txt += "\n  " + lws + ")"
                    else:
                        txt += " " + fmted
                txt += "\n"
            txt += "@else\n  "+lws + annot["output_name"] + " (" + (" ".join(terminals))+") bin_not_found \n@end"
        
        else:
            # Simple model
            txt = lws + annot["output_name"] + " (" + (" ".join(terminals))+") "+annot["output_mod_name"]+" "

            if len(psplit)>0:
                fmted, need_split, split = self.format_params(psplit, len(txt))
                if need_split or split:
                    fmted = self.indent(fmted, len(lws)+2)
                    txt += "(" + eol + "\n" + fmted
                    txt += "\n" + lws + ")"
                else:
                    txt += " " + fmted
        
        return txt
