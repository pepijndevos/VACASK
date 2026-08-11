#include "parseroutput.h"
#include "simulator.h"
#include "status.h"
#include "common.h"


namespace NAMESPACE {

std::ostream& operator<<(std::ostream& os, const PTParsedIdentifier& obj) {
    os << std::string(obj.id_);
    return os;
}


std::ostream& operator<<(std::ostream& os, const PTIdentifierList& obj) {
    for(auto it=obj.cbegin(); it!=obj.cend(); ++it) {
        if (it!=obj.cbegin()) 
            os << " ";
        if (it->name()!=Id::none) {
            os << it->name();
        } else {
            os << "<null>";
        }
    }
    return os;
}


void PTParameterValue::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx << std::string(id_) << "=" << val_;
}


void PTParameterExpression::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx << std::string(id_) << "=" << rpn_.str();
}

void PTParameterMap::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    for(auto& it : map_) {
        os << pfx << std::string(it.first) << "=";
        if (std::holds_alternative<const PTParameterValue*>(it.second)) {
            std::get<const PTParameterValue*>(it.second)->dump(0, os);
        } else if (std::holds_alternative<std::unique_ptr<const PTParameterValue>>(it.second)) {
            std::get<std::unique_ptr<const PTParameterValue>>(it.second)->dump(0, os);
        } else if (std::holds_alternative<const PTParameterExpression*>(it.second)) {
            std::get<const PTParameterExpression*>(it.second)->dump(0, os);
        } else {
            std::get<std::unique_ptr<const PTParameterExpression>>(it.second)->dump(0, os);
        }
        os << "\n";
    }
}

std::ostream& operator<<(std::ostream& os, const PTParameters& obj) {
    for(auto it=obj.values_.cbegin(); it!=obj.values_.cend(); ++it) {
        os << (it->name()) << "=" << it->val() << " " ;
    }
    for(auto it=obj.expressions_.begin(); it!=obj.expressions_.end(); ++it) {
        os << (it->name()) << "=" << it->rpn() << " " ;
    }
    
    return os;
}

void PTModel::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx;
    os << "model " << (modelName_) << " " << (deviceName_) << " ";
    os << parameters_ << "\n";
}


void PTInstance::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx << (instanceName_);
    if (behavioralData_.get()) {
        os << " (" << connections_[0].name() << " " << connections_[1].name();
        for(auto& n : behavioralData_->node) {
            auto& [nId, nVaName, ndx] = n;
            os << " " << nId;
        }
        for(auto& f : behavioralData_->flow) {
            auto& [fId, fVaName, ndx] = f;
            os << " " << fId << ":flow(br)";
        }
        os << ") ";
    } else {
        os << " (" << connections_ << ") ";
    }
    os << masterName_ << " " << parameters_ << "\n";
}

void PTBehavioral::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx << "// " << (instanceName_) << " (" << connections_ << ")";
    os << (currentSource_ ? " flow=" : " potential=" );
    os << expr_.str() << " discipline=[";
    os << "\"" << discipline_ << "\", ";
    os << "\"" << potentialAccessor_ << "\", ";
    os << "\"" << flowAccessor_ << "\"";
    os << "]\n";
}


void PTBlockSequence::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    bool first = true;
    for(auto& entry : entries_) {
        auto& [loc, rpn, block] = entry;
        if (rpn.size()>0) {
            if (first) {
                os << pfx << "@if " << rpn << "\n";
            } else {
                os << pfx << "@elseif " << rpn << "\n";
            }
        } else {
            os << pfx << "@else\n";
        }
        block.dump(indent+2, os);
        first = false;
    }
    os << pfx << "@end\n";
}


void PTBlock::dump(int indent, std::ostream& os) const {
    for(auto& mod : models_) {
        mod.dump(indent, os);
    }
    for(auto& inst : instances_) {
        inst.dump(indent, os);
    }
    for(auto& behav : behaviorals_) {
        behav.dump(indent, os);
    }
    if (hasBlockSequences()) {
        for(auto& seq: *blockSequences_) {
            seq.dump(indent, os);
        }
    }
}

void PTSubcircuitDefinition::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');

    bool isToplevel = !modelName_;

    if (!isToplevel) {
        os << pfx << "subckt " << (modelName_) << " (" << terminals_ << ")\n";
    }
    
    if (parameters_.count()>0) {
        os << pfx << (isToplevel ? "" : "  ") << "parameters " << parameters_ << "\n";
    }
    if (subDefs_.size()>0) {
        os << "\n";
    }
    for(auto it=subDefs_.begin(); it!=subDefs_.end(); ++it) {
        it->get()->dump(isToplevel ? indent : indent+2, os);
    }
    if (root_.models().size()>0) {
        os << "\n";
    }
    root_.dump(isToplevel ? indent : indent+2, os);
    
    if (!isToplevel) {
        os << pfx << "ends\n";
    }
    
    os << "\n";
}


void PTLoad::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    os << pfx << "load \"" << file_ << "\"";
    os << " " << (parameters_);
    os << "\n";
}


std::ostream& operator<<(std::ostream& os, const PTSave& s) {
    os << std::string(s.typeName()) << "(";
    if (s.id[0]) {
        os << "\"" << std::string(s.id[0]) << "\"";
    }
    if (s.id[1]) {
        os << "," << "\"" << std::string(s.id[1]) << "\"";
    }
    os << ")";
    
    return os;
}


std::ostream& operator<<(std::ostream& os, const PTSaves& s) {
    if (s.saves_.size()>0) {
        for(auto it=s.saves_.cbegin(); it!=s.saves_.cend(); ++it) {
            os << *it << " ";
        }
    }
    
    return os;
}


std::ostream& operator<<(std::ostream& os, const PTSweep& s) {
    os << "sweep " << std::string(s.name_) << " " << s.parameters_;
    
    return os;
}


void PTAnalysis::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    if (sweeps_.size()>0) {
        for(auto it=sweeps_.cbegin(); it!=sweeps_.cend(); ++it) {
            os << pfx << *it << "\n";
        }
    }
    os << pfx << (sweeps_.size()>0 ? "  " : "");
    os << "analysis " << std::string(name_) << " " << std::string(typeName_) << " ";
    os << parameters_ << "\n";
}


std::ostream& operator<<(std::ostream& os, const PTEmbed& e) {
    os << "embed \"" << e.filename_ << "\" <<<FILE\n" << e.contents_ << ">>>FILE";
    return os;
}


std::ostream& operator<<(std::ostream& os, const PTCommand& c) {
    os << c.name_;
    if (c.keywords_.size()>0) {
        os << " " << c.keywords_;
    }
    if (c.saves_.saves().size()==0) {
        if (c.expressions_.size()>0) {
            os << " (";
            bool first = true;
            for(auto& it : c.expressions_) {
                if (!first) {
                    os << ", ";
                }
                os << it;
                first = false;
            }
            os << ")";
        }
        os << " " << c.args_;
    } else {
        os << " " << c.saves_;
    }
    return os;
}


bool PTParameters::verify(int level, Status& s) const {
    std::unordered_map<Id,Loc> puniq;
    for(auto& it : values_) {
        auto [itPrev, inserted] = puniq.insert({it.name(), it.location()});
        if (!inserted) {
            s.set(Status::Redefinition, "Parameter '"+std::string(it.name())+"' redefinition.");
            s.extend(it.location());
            if (itPrev->second) {
                s.extend("Parameter was first defined here");
                s.extend(itPrev->second);
            }
            return false;
        }
    }
    for(auto& it : expressions_) {
        auto [itPrev, inserted] = puniq.insert({it.name(), it.location()});
        if (!inserted) {
            s.set(Status::Redefinition, "Parameter '"+std::string(it.name())+"' redefinition.");
            s.extend(it.location());
            if (itPrev->second) {
                s.extend("Parameter was first defined here");
                s.extend(itPrev->second);
            }
            return false;
        }
    }
    return true;
}

bool PTModel::verify(int level, Status& s) const {
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in model '"+std::string(modelName_)+"'");
        }
        return false;
    }
    return true;
}

bool PTInstance::verify(int level, Status& s) const {
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in instance '"+std::string(instanceName_)+"'");
        }
        return false;
    }
    return true;
}

bool PTBehavioral::verify(int level, Status& s) const {
    return true;
}

bool PTBlock::verify(int level, Status& s) const {
    // Check models
    for(auto& mod : models_) {
        if (!mod.verify(level, s)) {
            return false;
        }
    }

    // Check instances
    for(auto& inst : instances_) {
        if (!inst.verify(level, s)) {
            return false;
        }
    }
    
    // Check behaviorals
    for(auto& behav : behaviorals_) {
        if (!behav.verify(level, s)) {
            return false;
        }
    }

    // Recurse into all blocks in all block sequences
    if (blockSequences_) {
        int cnt=1;
        for(auto& blkSeq : *blockSequences_) {
            if (!blkSeq.verify(level, s)) {
                if (!loc) {
                    s.extend("  in block sequence '"+std::to_string(cnt)+"'");
                }
                return false;
            }
            cnt++;
        }
    }
    return true;
}

bool PTBlockSequence::verify(int level, Status& s) {
    // Check all blocks in all block sequences
    int cnt = 1;
    for(auto& blkSeqEntry : entries_) {
        auto& [loc, cond, blk] = blkSeqEntry;
        if (!blk.verify(level, s)) {
            if (!loc) {
                s.extend("  in block '"+std::to_string(cnt)+"'");
            }
            return false;
        }
        cnt++;
    }
    return true;
}

bool PTSubcircuitDefinition::verify(int level, Status& s) const {
    // Subcircuit terminal can have the same name as a global node. 
    // In that case it is simply a local node name. It does not 
    // behave as a global node. 
    // So do not check against global nodes. 

    // Check for duplicate terminal names
    std::unordered_set<Id> tset;
    for(auto& term : terminals_) {
        auto [exIt, inserted] = tset.insert(term.name());
        if (!inserted) {
            s.set(Status::Conflicting, "Terminal '"+std::to_string(term.name())+"' is not unique.");
            if (loc) {
                s.extend(loc);
            } else {
                s.extend("  in subcircuit definition '"+std::string(name())+"'");
            }
            
            return false;
        }
    }

    // Do not check name conflicts between instances, models, and subcircuits
    // Elaboration will handle that

    // Check parameters
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in subcircuit definition '"+std::string(name())+"'");
        }
        return false;
    }
    
    // Check root block
    if (!root_.verify(level, s)) {
        if (!loc) {
            s.extend("  in subcircuit definition '"+std::string(name())+"'");
        }
        return false;
    }

    // Check all subcircuit definitions within this definition
    for(auto& subDef : subDefs_) {
        if (!subDef->verify(level, s)) {
            if (!loc) {
                s.extend("  in subcircuit definition '"+std::string(name())+"'");
            }
            return false;
        }
    }
    return true; 
}

bool PTLoad::verify(int level, Status& s) const {
    // Check for parameters whose values are defined with expressions
    if (parameters_.expressionCount()>0) {
        s.set(Status::BadArguments, "Load directive parameters must be constants.");
        if (loc) {
            s.extend(loc);
        } else {
            s.extend("  in load directive for '"+file_+"'.");
        }
        return false;
    }

    // Check for parameter duplicates
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in load directive for '"+file_+"'.");
        }
        return false;
    }

    return true;
}

bool PTSweep::verify(int level, Status& s) const {
    // Verify parameters
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in sweep '"+std::string(name_)+"'.");
        }
        return false;
    }
    return true;
}

bool PTAnalysis::verify(int level, Status& s) const {
    // Verify sweeps
    for(auto& sw : sweeps_) {
        if (!sw.verify(level, s)) {
            if (!loc) {
                s.extend("  in analysis '"+std::string(name_)+"'.");
            }
            return false;
        }
    }

    // Verify parameters
    if (!parameters_.verify(level, s)) {
        if (!loc) {
            s.extend("  in analyis '"+std::string(name_)+"'.");
        }
        return false;
    }

    return true;
}

/*
// Extract canonical name of the file that contains the behavioral source instance
    std::string canonicalFileName;
    Loc instanceLoc = @1.loc();
    if (instanceLoc) {
      auto [fileStack, fileIndex, line, col] = instanceLoc.data();
      if (fileStack) {
        canonicalFileName = fileStack->canonicalName(fileIndex);
      }
    }
*/

bool ParserTables::verifyWorker(int level, Status& s) const {
    // Check for duplicate analyses (all levels)
    std::unordered_map<Id,const PTAnalysis*> anmap;
    for(auto& it : control_) {
        if (!std::holds_alternative<PTAnalysis>(it)) {
            continue;
        }
        auto& an = std::get<PTAnalysis>(it);
        auto [itEx, inserted] = anmap.insert({an.name(), &an});
        if (!inserted) {
            s.set(Status::Redefinition, "Analysis '"+std::string(an.name())+"' redefinition.");
            s.extend(an.location());
            if (itEx->second->location()) {
                s.extend("Analysis was first defined here");
                s.extend(itEx->second->location());
            }
            return false;
        }
    }

    // Verify load directives
    for(auto& load : loads_) {
        if (!load.verify(level, s)) {
            return false;
        }
    }

    // Verify default subcircuit definition
    if (level>0) {
        if (!defaultSubDef_.verify(level, s)) {
            return false;
        }
    }

    return true;
}

// Determine whether dumpedFile needs to be (re)written, given the set of
// canonical paths of the files it depends on. The decision is based on a
// side-car "<dumpedFile>.origin" file that records the dependency set used
// to produce the current dumpedFile (one path per line): a dump is needed
// if there are no known dependencies, dumpedFile or its origin file is
// missing, the recorded dependency set differs from the current one, or
// any dependency is newer than dumpedFile.
static bool needsDump(const std::string& dumpedFile, const std::unordered_set<std::string>& dependencies) {
    if (dependencies.empty()) {
        // No known dependency, always dump
        return true;
    }

    std::string originFilePath = dumpedFile + ".origin";
    if (!std::filesystem::exists(originFilePath) || !std::filesystem::exists(dumpedFile)) {
        return true;
    }

    // Read the recorded dependency set
    std::ifstream originFile(originFilePath);
    if (!originFile) {
        return true;
    }
    std::unordered_set<std::string> recordedDependencies;
    std::string line;
    while (std::getline(originFile, line)) {
        recordedDependencies.insert(line);
    }
    originFile.close();

    if (recordedDependencies!=dependencies) {
        return true;
    }

    // Any dependency newer than the dumped file?
    auto dumpedModificationTime = std::filesystem::last_write_time(dumpedFile);
    for(auto& dep : dependencies) {
        if (std::filesystem::last_write_time(dep)>dumpedModificationTime) {
            return true;
        }
    }

    return false;
}

// Record the dependency set used to produce dumpedFile, for future
// needsDump() calls. Failure to write is silently ignored, same as the
// previous single-file origin-tracking behavior.
static void writeDependencyFile(const std::string& dumpedFile, const std::unordered_set<std::string>& dependencies) {
    std::ofstream originFile(dumpedFile + ".origin", std::ios::out);
    if (!originFile) {
        return;
    }
    for(auto& dep : dependencies) {
        originFile << dep << "\n";
    }
    originFile.close();
}

bool ParserTables::processBehaviorals(int debug, Status& s) {
    if (behavioralsProcessed_) {
        s.set(Status::InternalError, "ParserTables::processBehaviorals() called more than once.");
        return false;
    }
    behavioralsProcessed_ = true;

    // Verilog-A
    std::string va = "`include \"constants.vams\"\n`include \"disciplines.vams\"\n\n";

    // Set of dependencies
    std::unordered_set<std::string> dependencies;

    // Count behavioral sources
    size_t behavCount = 0;

    // Work list of subcircuit definitions still to process, starting with
    // the root (default) subcircuit definition
    std::vector<std::tuple<PTSubcircuitDefinition*, const std::string>> subDefStack{ {&defaultSubDef_, ""} };
    while (!subDefStack.empty()) {
        // Copy out by value: subsequent push_back() calls in this iteration
        // (below) can reallocate the vector, which would invalidate a
        // reference obtained from back(); pop_back() alone would also
        // destroy such a reference's target.
        auto [ subDef, moduleNameRoot ] = std::move(subDefStack.back());
        subDefStack.pop_back();

        // Queue nested subcircuit definitions
        for(auto& subSubDef : subDef->subDefs()) {
            auto name = moduleNameRoot + "_" + Rpn::sanitizeVariable(std::string(subSubDef->name()));
            subDefStack.push_back( {subSubDef.get(), name} );
        }

        // Work list of blocks still to process within this subcircuit
        // definition, starting with its root block
        std::vector<std::tuple<PTBlock*,const std::string>> blockStack{ {&subDef->root(), ""} };
        while (!blockStack.empty()) {
            // Copy out by value, same reasoning as subDefStack above.
            auto [blk, blockNameRoot] = std::move(blockStack.back());
            blockStack.pop_back();

            for(auto& behav : blk->behaviorals()) {
                // Create module definition
                RPNBehavioralVA behavData = {
                    // Construct module name
                    .moduleName = "__behavioral" + (blockNameRoot.size()>0 ? (moduleNameRoot+"_"+blockNameRoot) : moduleNameRoot) + "_" + std::string(behav.name()), 
                    .currentSource = behav.currentSource(), 
                };
                // Run Rpn::verilogA
                if (!behav.expr().verilogA(behav.discipline(), behav.potentialAccessor(), behav.flowAccessor(), behavData, s)) {
                    if (!behav.location()) {
                        s.extend("  in behavioral source '"+std::string(behav.name())+"'.");
                    }
                    return false;
                }

                // Append to Verilog-A file
                va += behavData.vaCode+"\n\n";
                behavCount++;

                // Create model, same name as module
                PTModel behavModel(Id(behavData.moduleName), Id(behavData.moduleName), behav.location());
                for(auto& [paramId, paramVaName, paramType, paramIdx] : behavData.param) {
                    Rpn passthrough;
                    passthrough.extend(Rpn::Identifier(std::string(paramId)), behav.location());
                    behavModel.add(PTParameterExpression(Id(paramVaName), std::move(passthrough), behav.location()));
                }
                blk->add(std::move(behavModel));

                // Create instance
                // Check number of connections_ (must be 2)
                if (behav.connections().size()!=2) {
                    s.set(Status::BadArguments, "Behavioral source instance '"+std::string(behav.name())+"' requires exactly 2 terminals.");
                    s.extend(behav.location());
                    return false;
                }
                PTIdentifierList behavTerms;
                behavTerms.push_back(behav.connections()[0]);
                behavTerms.push_back(behav.connections()[1]);
                // Then connect RPNBehavioralVA::node nodes
                for(auto& [nodeId, nodeVaName, nodeIdx] : behavData.node) {
                    behavTerms.push_back(PTParsedIdentifier(nodeId, behav.location()));
                }
                // RPNBehavioralVA::flow nodes should not be connected at instance creation. 
                // They should not be connected to internal nodes (handled by OsdiInstance::buildHierarchy()). 
                // They are connected during OsdiInstance::populateStructuresCore(). 
                PTInstance behavInstance(behav.name(), Id(behavData.moduleName), std::move(behavTerms), behav.location());
                behavInstance.addBehavioralData(std::move(behavData));
                blk->add(std::move(behavInstance));

                // Get canonical name of source file where this behavioral is defined
                std::string canonicalFileName;
                if (behav.location()) {
                    auto [fileStack, fileIndex, line, col] = behav.location().data();
                    if (fileStack) {
                        canonicalFileName = fileStack->canonicalName(fileIndex);
                        // Add to set of dependencies
                        dependencies.insert(canonicalFileName);
                    }
                }
            }

            // Queue the blocks of all block sequences within this block
            if (blk->hasBlockSequences()) {
                auto seqNdx = 0;
                for(auto& seq : blk->blockSequences()) {
                    auto blkNdx = 0;
                    for(auto& [loc, cond, innerBlk] : seq.entries()) {
                        std::string blkName = blockNameRoot.size()>0 ? (blockNameRoot + "_") : "";
                        blkName += "s" + std::to_string(seqNdx) + "b" + std::to_string(blkNdx);
                        blockStack.push_back( {&innerBlk, blkName} );
                        blkNdx++;
                    }
                    seqNdx++;
                }
            }
        }
    }

    // No behavioral sources, we are done
    if (behavCount==0) {
        return true;
    }

    // Get the file name of the toplevel netlist
    // Behavioral sources may not come from a file at all (e.g. a circuit
    // built entirely through the API), in which case there is no toplevel
    // netlist file to derive a name from. In that case only the name 
    // extension is used. 
    std::string vaFileName;
    if (fileStack_.isFileEntry(0)) {
        vaFileName = fileStack_.canonicalName(0);
    }
    // Extend with __behavioral.va
    vaFileName += "__behavioral.va";

    // Do we need to dump?
    bool dump = needsDump(vaFileName, dependencies);

    // Dump Verilog-A file
    if (dump) {
        writeDependencyFile(vaFileName, dependencies);

        std::ofstream vaFile(vaFileName, std::ios::out);
        if (!vaFile) {
            s.set(Status::CreationFailed, "Failed to write file '"+vaFileName+"'.");
            return false;
        }
        vaFile << va;
        vaFile.close();
    }

    // Add load directive
    loads_.push_back(PTLoad(vaFileName));

    return true;
}

bool ParserTables::writeEmbedded(int debug, Status& s) {
    for(auto& e : embed_) {
        // Embedded files depend on the single file that contains the
        // embed directive, if that location is known
        std::unordered_set<std::string> dependencies;
        if (e.location()) {
            auto [fs, pos, line, offset] = e.location().data();
            dependencies.insert(fs->canonicalName(pos));
        }

        // No need to dump
        if (!needsDump(e.filename(), dependencies)) {
            continue;
        }

        writeDependencyFile(e.filename(), dependencies);

        std::ofstream file(e.filename(), std::ios::out);
        if (!file) {
            // Failure to dump is an error
            s.set(Status::CreationFailed, "Failed to write file '"+e.filename()+"'.");
            s.extend(e.location());
            return false;
        } else {
            // Dump
            file << e.contents();
            file.close();
        }

    }
    return true;
}

void ParserTables::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');

    os << pfx << title_ << "\n\n";

    if (loads_.size()>0) {
        for(auto it=loads_.begin(); it!=loads_.end(); ++it) {
            it->dump(indent, os);
        }
        os << "\n";
    }
    
    if (groundNodes_.size()>0) {
        os << pfx << "ground";
        for(auto it=groundNodes_.begin(); it!=groundNodes_.end(); ++it) {
            os << " " << *it;
        }
        os << "\n\n";
    }
    
    if (globalNodes_.size()>0) {
        os << pfx << "global";
        for(auto it=globalNodes_.begin(); it!=globalNodes_.end(); ++it) {
            os << " " << *it;
        }
        os << "\n\n";
    }

    defaultSubDef_.dump(indent, os);
    os << "\n";

    if (embed_.size()>0) {
        for(auto& it : embed_) {
            os << pfx << it << "\n";
        }
        os << "\n";
    }

    if (control_.size()>0) {
        os << pfx << "control\n";
        for(auto& it : control_) {
            if (std::holds_alternative<PTAnalysis>(it)) {
                std::get<PTAnalysis>(it).dump(indent+2, os); 
            } else {
                os << pfx << "  " << std::get<PTCommand>(it) << "\n";
            }
        }
        os << pfx << "endc\n";
    }
}

}
