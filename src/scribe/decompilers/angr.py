import angr
from collections import defaultdict
from angr.angrdb import AngrDB
from pathlib import Path
import os
import pickle


class AngrDecompiler:
    def __init__(self, binary_path, use_adb=False, use_pickle=False):
        self.binary_path = binary_path
        if use_adb and use_pickle:
            raise Exception("Cannot use both adb and pickle")
        if use_pickle and os.path.exists(
            f"{binary_path}.pickle"
        ):  # use pickle when available
            with open(f"{binary_path}.pickle", "rb") as f:
                self.p, self.cfg = pickle.load(f)
                self.cfg_model = self.cfg.model
        elif use_adb and os.path.exists(f"{binary_path}.adb"):  # use adb when available
            self.p = AngrDB().load(f"{binary_path}.adb")
            self.cfg_model = self.p.kb.cfgs.cfgs["CFGFast"]
        else:
            self.p = angr.Project(
                binary_path, auto_load_libs=False, load_debug_info=False
            )
            self.cfg = self.p.analyses.CFG(data_references=True, normalize=True)
            self.cfg_model = self.cfg.model
            self.p.analyses[angr.analyses.CompleteCallingConventionsAnalysis].prep()(
                recover_variables=True
            )
            # self.cfg.do_full_xrefs() # TODO: remove this

            self.dump_adb(binary_path + ".adb")
            self.dump_pickle(binary_path + ".pickle")

        self.backlinks = {}
        self.matched_info = defaultdict(dict)
        self.matched = defaultdict(list)
        self.decomps = {}

    def dump_adb(self, filename):
        db = AngrDB(self.p)
        db.dump(filename)

    def dump_pickle(self, filename):
        with open(filename, "wb") as f:
            pickle.dump((self.p, self.cfg), f)

    def print_all_string(self):
        for dst, xrefs in self.p.kb.xrefs.xrefs_by_dst.items():
            if xrefs:
                if (
                    list(xrefs)[0].memory_data.sort
                    == angr.knowledge_plugins.cfg.MemoryDataSort.String
                ):
                    print(hex(dst))
                    print(list(xrefs)[0].memory_data.content)
                    for xref in list(xrefs):
                        print(hex(xref.ins_addr))

    def get_function_by_name(self, function_name):
        if function_name and function_name in self.p.kb.functions:
            return self.p.kb.functions[function_name].addr
        else:
            return None

    def get_function_list(self, skip_dot_functions=True):
        return [
            func
            for func in self.p.kb.functions
            if not self.p.kb.functions[func].is_plt
            and not self.p.kb.functions[func].is_simprocedure
            and not self.p.kb.functions[func].alignment
            and not self.p.kb.functions[func].name.startswith("sub_")
        ]

    def is_function_bp_based(self, addr):
        func = self.p.kb.functions[addr]
        if self.get_architecture() == "amd64":
            rbp_count, rsp_count = 0, 0
            for block in func.blocks:
                for ins in block.capstone.insns:
                    if "[rsp" in ins.op_str:
                        rsp_count += 1
                    if "[rbp" in ins.op_str:
                        rbp_count += 1
            return not (rsp_count != 0 and rbp_count == 0)
        elif self.get_architecture() == "arm":
            return False
        return False

    def get_function_stack_offsets(self, addr):
        stack_offsets = []
        if addr not in self.decomps:
            self.decompile(addr)
        for i in self.decomps[addr].codegen.cfunc.variables_in_use:
            var = self.decomps[addr].codegen.cfunc.variable_manager.unified_variable(i)
            if (
                hasattr(var, "offset")
                and {"name": var.name, "StkLoc": var.offset} not in stack_offsets
            ):
                stack_offsets.append({"name": var.name, "StkLoc": var.offset})
            elif (
                hasattr(var, "reg")
                and {"name": var.name, "StkLoc": self.p.arch.register_names[var.reg]}
                not in stack_offsets
            ):
                stack_offsets.append(
                    {"name": var.name, "StkLoc": self.p.arch.register_names[var.reg]}
                )
        return {
            "functions": [
                {
                    "name": self.get_function_name(addr),
                    "is_bp_based": self.is_function_bp_based(addr),
                    "variables": stack_offsets,
                }
            ]
        }

    def decompile(self, addr):
        decomp = self.p.analyses.Decompiler(
            self.p.kb.functions[addr],
            flavor="pseudocode",
            cfg=self.cfg_model,
            options=[
                (
                    angr.analyses.decompiler.decompilation_options.get_structurer_option(),
                    "Phoenix",
                )
            ],
            optimization_passes=angr.analyses.decompiler.optimization_passes.get_default_optimization_passes(
                self.p.arch, self.p.simos.name
            ),
        )
        self.decomps[addr] = decomp
        return decomp

    def get_decompile_text(self, addr):
        if addr not in self.decomps:
            self.decompile(addr)
        result = self.decomps[addr].codegen.text
        for var in self.decomps[addr].codegen.cexterns:
            if "@" in var.variable.name:
                result = result.replace(
                    var.variable.name, var.variable.name.split("@")[0]
                )
            if "." in var.variable.name:
                result = result.replace(
                    var.variable.name, var.variable.name.replace(".", "_")
                )
        return result

    def get_base_address(self):
        return self.p.loader.main_object.min_addr

    def get_architecture(self):
        if self.p.arch.name == "AMD64":
            return "amd64"
        elif self.p.arch.name == "ARMEL":
            return "arm"
        elif self.p.arch.name == "AARCH64":
            return "aarch64"
        elif self.p.arch.name == "X86":
            return "x86"
        else:
            raise Exception("Unknown architecture")

    def get_function_symbols(self, addr):
        symbols = {}
        if addr not in self.decomps:
            self.decompile(addr)
        for node in self.p.kb.functions[addr].transition_graph.nodes():
            if isinstance(node, angr.knowledge_plugins.functions.Function):
                symbols[node.name] = node.addr
        for var in self.decomps[addr].codegen.cexterns:
            symbols[var.variable.name] = var.variable.addr
        return symbols

    def get_symbols(self):
        symbols = {}
        for symbol in self.p.loader.main_object.symbols:
            if not symbol.name:
                continue
            symbols[symbol.name] = symbol.rebased_addr
        for func in self.p.kb.functions.values():
            if func.is_simprocedure or func.is_alignment:
                continue
            symbols[func.name] = func.addr
        return symbols

    def get_referenced_function_prototypes(self, addr):
        return self.get_referenced_function_prototypes_and_externs(addr)

    def get_referenced_function_prototypes_and_externs(self, addr):
        result = ""
        for node in self.p.kb.functions[addr].transition_graph.nodes():
            if isinstance(node, angr.knowledge_plugins.functions.Function):
                if node.prototype:
                    result += f"{node.prototype.c_repr(name=node.name, full=True)};\n"
        # ignore externs because they are already in decompiled text
        return result

    def get_decompiler_extra_header(self):
        return (
            Path(
                os.path.realpath(os.path.dirname(__file__)), "angr_extra.h"
            ).read_text()
            + "\n"
        )

    def get_general_compile_header(self):
        header = ""
        header += "#define True 1\n#define true 1\n"
        header += "#define False 0\n#define false 0\n"
        header += "#include <bits/types/FILE.h>\n"
        header += "#define NULL 0\n"
        header += "typedef FILE FILE_t;\n"
        return header

    def get_function_ast(self, addr):
        if addr not in self.decomps:
            self.decompile(addr)
        return self.decomps[addr].codegen.cfunc

    def get_function_size(self, addr):
        return self.p.kb.functions[addr].size

    def get_function_name(self, addr):
        return self.p.kb.functions[addr].name
