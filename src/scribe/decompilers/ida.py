import os
import traceback
from pathlib import Path

import angr
from angrmanagement.utils.graph import to_supergraph
from headless_ida import HeadlessIda

from ..config import DECOMPILER_CONFIG_IDA_DEF_H_FILE_PATH, DECOMPILER_CONFIG_IDA_PATH


class IdaDecompiler:
    def __init__(self, binary_path):
        self.binary_path = binary_path
        self.headlessida = HeadlessIda(
            os.path.join(DECOMPILER_CONFIG_IDA_PATH, "idat64"), binary_path
        )
        ida_libs = [
            "idc",
            "idautils",
            "idaapi",
            "ida_funcs",
            "ida_xref",
            "ida_nalt",
            "ida_auto",
            "ida_hexrays",
            "ida_name",
            "ida_expr",
            "ida_struct",
            "ida_typeinf",
            "ida_loader",
            "ida_lines",
            "ida_segment",
            "ida_frame",
            "ida_idaapi",
            "ida_bytes",
        ]
        for lib in ida_libs:
            setattr(self, lib, self.headlessida.import_module(lib))

        self.decomps = {}
        self.callees = {}
        self.callers = {}

        self.extra_struct_defs = ""

        # angr
        self._angr_p = None
        self._angr_cfg = None

    @property
    def angr_p(self):
        if self._angr_p is None:
            self._angr_p = angr.Project(self.binary_path, main_opts={"base_addr": self.get_base_address()})
        return self._angr_p

    @property
    def angr_cfg(self):
        if self._angr_cfg is None:
            self._angr_cfg = self.angr_p.analyses.CFGFast()
        return self._angr_cfg

    def get_callee_list(self, addr):
        text_seg = self.ida_segment.get_segm_by_name(".text")
        if addr not in self.callees:
            self.callees[addr] = []
            for ea in self.idautils.FuncItems(addr):
                for xref in self.idautils.XrefsFrom(ea):
                    tif = self.ida_typeinf.tinfo_t()
                    self.ida_hexrays.get_type(
                        xref.to, tif, self.ida_hexrays.GUESSED_DATA
                    )
                    if tif.is_func() and text_seg.start_ea <= xref.to < text_seg.end_ea:
                        self.callees[addr].append(xref.to)
        return list(set(self.callees[addr]))

    def get_caller_list(self, addr):
        if addr not in self.callers:
            self.callers[addr] = []
            for xref in self.idautils.XrefsTo(addr):
                func = self.ida_funcs.get_func(xref.frm)
                if func:
                    self.callers[addr].append(func.start_ea)
        return list(set(self.callers[addr]))

    def _possible_second_return_register(self, function):
        if self.angr_p.arch.name == "AMD64":
            second_return_register = "rdx"
            second_return_register_offset = self.angr_p.arch.registers[second_return_register][0]
        else:
            return False

        # Retrieve function from CFG
        try:
            func = self.angr_cfg.functions[function]
        except KeyError:
            return False

        rda = self.angr_p.analyses.ReachingDefinitions(subject=func)

        # Find all definitions of the second return register
        all_second_return_register_definitions = [
            x for x in rda.all_definitions 
            if isinstance(x.atom, angr.knowledge_plugins.key_definitions.atoms.Register) 
            and x.atom.reg_offset == second_return_register_offset
            and x.codeloc.ins_addr
        ]

        all_second_return_register_definitions.sort(key=lambda x: x.codeloc.ins_addr)

        if not all_second_return_register_definitions:
            return False

        def_addr = all_second_return_register_definitions[-1].codeloc.ins_addr
        if not def_addr:
            return False
        
        if not (func.addr <= def_addr < func.addr + func.size):
            return False

        # If the second return register is used before ret, return False
        if rda.all_uses.get_uses(all_second_return_register_definitions[-1]):
            return False

        # Find all call sites of the function
        callsites = [] # [xref.ins_addr for xref in self.angr_p.kb.xrefs.get_xrefs_by_dst(function) if xref.is_call]
        node = self.angr_cfg.model.get_any_node(function)
        if node:
            for pred in self.angr_cfg.model.get_predecessors(node):
                if pred.instruction_addrs:
                    if self.angr_p.arch.branch_delay_slot and len(pred.instruction_addrs) > 1:
                        ins_addr = pred.instruction_addrs[-2]
                    else:
                        ins_addr = pred.instruction_addrs[-1]
                else:
                    ins_addr = pred.addr
                callsites.append(ins_addr)

        # Check each call site
        for callsite in callsites:
            # Find the supergraph node containing the call site
            callsite_func = self.angr_cfg.functions.function(self.angr_cfg.get_any_node(callsite, anyaddr=True).function_address)
            if not callsite_func:
                continue
            callsite_rda = self.angr_p.analyses.ReachingDefinitions(subject=callsite_func)
            callsite_supernodes = [node for node in to_supergraph(callsite_func.transition_graph).nodes  if node.addr <= callsite < node.addr + node.size]

            if not callsite_supernodes:
                continue
            callsite_supernode = callsite_supernodes[0]

            bbs_after_callsite = [callsite_func.get_block(node.addr) for node in callsite_supernode.cfg_nodes if node.addr > callsite]
            if not bbs_after_callsite:
                continue
            first_bb_after_callsite = bbs_after_callsite[0]

            if first_bb_after_callsite is None:
                continue
            for instr_addr in first_bb_after_callsite.instruction_addrs:
                if instr_addr <= callsite:
                    continue
                # Find all uses of the second return register at the instruction address
                all_callsite_uses = [
                    x for x in callsite_rda.all_uses.get_uses_by_insaddr(instr_addr) 
                    if isinstance(x.atom, angr.knowledge_plugins.key_definitions.atoms.Register) 
                    and x.atom.reg_offset == second_return_register_offset
                ]

                if not all_callsite_uses:
                    continue

                # If the second return register is not defined after the call site and before the next instruction, return True
                if not any(
                    x.codeloc.ins_addr and
                    callsite < x.codeloc.ins_addr < instr_addr
                    for x in all_callsite_uses
                ):
                    # if callsite == 0x19c70:
                    #     import ipdb; ipdb.set_trace()
                    return True

        # If none of the conditions are met, return False
        return False

    def _update_function_with_additional_ret_reg(self, addr):
        # get original function decl
        func_decl = self.ida_typeinf.tinfo_t()
        # intentionally not using decompilation cache to prevent infinite recursion
        self.ida_typeinf.parse_decl(func_decl, None, self.ida_lines.tag_remove(self.ida_hexrays.decompile(addr).print_dcl()) + ";", self.ida_typeinf.PT_TYP)

        # add struct definition
        my_struct = self.ida_struct.get_struc(self.ida_struct.add_struc(self.idc.BADADDR, f"__recompile_xtra_ret_{addr:x}"))
        self.ida_struct.add_struc_member(my_struct, "a", self.idc.BADADDR, self.ida_bytes.get_flags_by_size(func_decl.get_rettype().get_size()), None, func_decl.get_rettype().get_size())
        self.ida_struct.add_struc_member(my_struct, "b", self.idc.BADADDR, self.ida_bytes.get_flags_by_size(func_decl.get_rettype().get_size()), None, func_decl.get_rettype().get_size())
        self.extra_struct_defs += f"struct __recompile_xtra_ret_{addr:x} {{\n    {func_decl.get_rettype().dstr()} a;\n    {func_decl.get_rettype().dstr()} b;\n}};\n\n"

        # update function decl
        func_args_str = ", ".join([f"{func_decl.get_nth_arg(i).dstr()} arg_{i}" for i in range(func_decl.get_nargs())])
        new_func_decl = f"struct __recompile_xtra_ret_{addr:x} {self.ida_name.get_ea_name(addr)}({func_args_str});"

        assert self.ida_typeinf.apply_cdecl(None, addr, new_func_decl, self.ida_typeinf.TINFO_DEFINITE), f"Failed to apply cdecl {new_func_decl} to {addr:x}"


    def decompile(self, addr, approach="bottom-up"):
        if approach == "direct":
            self._decompile(addr)
        else:
            self._decompile_with_deps(addr, approach)
        return self.decomps[addr]

    def _decompile(self, addr):
        if addr not in self.decomps:
            # try:
            #     # in case function uses second return register (cp - get_stat_atime), update function decl
            #     if self._possible_second_return_register(addr):
            #         self._update_function_with_additional_ret_reg(addr)
            # except Exception as e:
            #     print(f"Warning: Failed to check/update second return register for {hex(addr)}:\n{traceback.format_exc()}")
            self.decomps[addr] = self.ida_hexrays.decompile(addr)
        if self.decomps[addr] is None:
            raise Exception(f"Failed to decompile function at {hex(addr)}")
        return self.decomps[addr]

    def _decompile_with_deps(self, addr, approach, visited=None, stack=None):
        if visited is None:
            visited = set()
        if stack is None:
            stack = set()

        # Detect circular dependencies to prevent infinite recursion
        if addr in stack:
            return
        stack.add(addr)

        if approach == "bottom-up":
            deps = self.get_callee_list(addr)
        elif approach == "top-down":
            deps = self.get_caller_list(addr)
        else:
            raise Exception(f"Unknown approach {approach}")

        for dep_addr in deps:
            if dep_addr not in visited:
                self._decompile_with_deps(dep_addr, approach, visited, stack)

        stack.remove(addr)
        visited.add(addr)

        try:
            self._decompile(addr)
        except Exception:
            print(f"Failed to decompile {hex(addr)}: {traceback.format_exc()}")

    def get_decompile_text(self, addr):
        return str(self.decompile(addr))

    def get_disassembly(self, addr):
        func = self.ida_funcs.get_func(addr)
        asm = ""
        for instr in func.head_items():
            asm += f"{self.ida_lines.tag_remove(self.idc.GetDisasm(instr))}\n"
        return asm

    def get_base_address(self):
        return self.ida_nalt.get_imagebase()

    def get_architecture(self):
        info = self.idaapi.get_inf_structure()
        if info.is_64bit():
            if info.procname.lower() == "metapc":
                return "amd64"
            elif info.procname.lower() == "arm":
                return "aarch64"
        if info.is_32bit():
            if info.procname.lower() == "metapc":
                return "x86"
            elif info.procname.lower() == "arm":
                return "arm"
        raise Exception("Unknown architecture")

    def get_function_by_name(self, name):
        addr = self.ida_name.get_name_ea(0, name)
        if addr == self.idc.BADADDR:
            raise Exception(f"Function {name} not found")
        return self.ida_name.get_name_ea(0, name)

    def get_function_list(self, skip_dot_functions=True):
        text_seg = self.ida_segment.get_segm_by_name(".text")
        funcs = [
            x
            for x in self.idautils.Functions()
            if text_seg.start_ea <= x < text_seg.end_ea
        ]
        if skip_dot_functions:
            funcs = [x for x in funcs if not self.get_function_name(x).startswith(".")]
        return funcs

    def is_function_bp_based(self, addr):
        func = self.ida_funcs.get_func(addr)
        if self.get_architecture() == "amd64":
            rbp_count, rsp_count = 0, 0
            for instr in func.head_items():
                disasm = self.ida_lines.tag_remove(self.idc.GetDisasm(instr))
                if "[rsp" in disasm:
                    rsp_count += 1
                if "[rbp" in disasm:
                    rbp_count += 1

            return not (rsp_count != 0 and rbp_count == 0)
        elif self.get_architecture() == "arm":
            return False
        return False

    def get_function_stack_offsets(self, addr):
        stack_offsets = []
        func = self.ida_funcs.get_func(addr)
        is_bp_based = self.is_function_bp_based(addr)
        decomp_func = self.decompile(addr)
        for lvar in decomp_func.get_lvars():
            if lvar.is_stk_var():
                if is_bp_based:
                    stack_offsets.append(
                        {
                            "name": lvar.name,
                            "StkLoc": self.ida_frame.soff_to_fpoff(
                                func, lvar.get_stkoff() - decomp_func.get_stkoff_delta()
                            ),
                        }
                    )
                else:
                    stack_offsets.append(
                        {
                            "name": lvar.name,
                            "StkLoc": lvar.get_stkoff()
                            - decomp_func.get_stkoff_delta(),
                        }
                    )
            elif lvar.is_reg_var():
                stack_offsets.append(
                    {
                        "name": lvar.name,
                        "StkLoc": self.ida_hexrays.get_mreg_name(
                            lvar.get_reg1(), lvar.width
                        ),
                    }
                )
        return {
            "functions": [
                {
                    "name": self.get_function_name(addr),
                    "is_bp_based": is_bp_based,
                    "variables": stack_offsets,
                }
            ]
        }

    def get_function_size(self, addr):
        return self.ida_funcs.get_func(addr).size()

    def get_function_name(self, addr):
        return self.ida_name.get_ea_name(addr)

    def _add_underscore_symbol(self, symbols, key, val, skip_if_exists=False):
        # remove leading underscores, one at a time and add to symbols
        symbols[key] = val
        while key.startswith("_"):
            key = key[1:]
            if skip_if_exists and key in symbols:
                continue
            symbols[key] = val
        return symbols

    def _normalize_symbol(self, symbol):
        # if @ is present, remove everything after it
        if "@" in symbol:
            symbol = symbol.split("@")[0]
        # replace everything that's not [a-zA-Z0-9_] with _
        return "".join([c if c.isalnum() or c == "_" else "_" for c in symbol])

    def get_symbols(self):
        symbols = {}
        for ea, name in self.idautils.Names():
            if self.idc.get_segm_attr(ea, self.idc.SEGATTR_TYPE) == self.idc.SEG_XTRN:
                continue
            elif (
                self.ida_funcs.get_func(ea)
                and self.ida_funcs.get_func(ea).flags
                & (self.ida_funcs.FUNC_LIB | self.ida_funcs.FUNC_THUNK)
                and name[1].startswith(".")
            ):
                symbols = self._add_underscore_symbol(
                    symbols, self._normalize_symbol(name), ea, skip_if_exists=True
                )
            else:
                symbols = self._add_underscore_symbol(
                    symbols, self._normalize_symbol(name), ea
                )

        # Dummy Names (e.g. dword_1234, unk_1234) are not included in idautils.Names()
        for seg_ea in self.idautils.Segments():
            seg = self.ida_segment.getseg(seg_ea)
            for ea in range(seg.start_ea, seg.end_ea):
                name = self.ida_name.get_ea_name(ea)
                if self.idc.get_segm_attr(ea, self.idc.SEGATTR_TYPE) == self.idc.SEG_XTRN:
                    continue
                if name and not name.startswith("loc_"):
                    symbols = self._add_underscore_symbol(
                        symbols, self._normalize_symbol(name), ea, skip_if_exists=True
                    )
        return symbols

    def get_referenced_function_prototypes(self, addr):
        return self.get_referenced_function_prototypes_and_externs(addr)

    def get_referenced_function_prototypes_and_externs(self, addr):
        self.decompile(addr) # ensure decompilation cache is populated
        result = ""
        result += self.extra_struct_defs
        externs = {} # dedup
        xrefs = {}
        for ea in self.idautils.FuncItems(addr):
            for xref in self.idautils.XrefsFrom(ea):
                xrefs[xref.to] = xref  # dedup
        for _, xref in xrefs.items():
            tif = self.ida_typeinf.tinfo_t()
            self.ida_hexrays.get_type(xref.to, tif, self.ida_hexrays.GUESSED_DATA)
            if tif.is_func():
                if (
                    xref.to < addr
                    or xref.to >= addr + self.ida_funcs.get_func(addr).size()
                ):
                    try:
                        result += f"{self.ida_lines.tag_remove(self.decompile(xref.to).print_dcl()).replace('__noreturn','')};\n"
                    except Exception:
                        pass
            elif tif.is_funcptr() or not xref.iscode:
                if self.ida_name.get_ea_name(xref.to):  # name might be empty
                    name = self.ida_name.get_ea_name(xref.to)
                    name = self._normalize_symbol(name)
                    externs = self._generate_extern(tif, name, externs)
        for decl in externs.values():
            result += decl
        return result

    def _generate_extern(self, tif, name, externs=None):
        externs = externs or {}
        if tif.is_funcptr():
            if name not in externs:
                externs[name] = f"{str(self.ida_typeinf.print_tinfo('', 0, 0, self.ida_typeinf.PRTYPE_1LINE, tif, name, ''))};\n"
        elif tif.is_unknown():
            if name not in externs:
                externs[name] = f"extern _UNKNOWN {name};\n"
        elif tif.is_array():
            if name not in externs:
                externs[name] = f"extern {str(tif.get_array_element()).replace('__ptr32', '')} {name}[{tif.get_array_nelems()}];\n"
        else:
            # if name == "optarg":
            #     result += f"extern char *{name};\n" # it will be treat as wrong type anyway, causing incompatible pointer to integer conversion assigning to 'xxx' from 'xxx'
            # else:
            if name not in externs:
                externs[name] = f"extern {str(tif).replace('__ptr32', '')} {name};\n"
        if name.startswith("_"):
            externs = self._generate_extern(tif, name[1:], externs)

        return externs

    def get_general_compile_header(self):
        header = "#include <stddef.h>\n#include <bits/types/FILE.h>\n#include <stdint.h>\n#include <string.h>\n#if defined(__x86_64__)\n#include <emmintrin.h>\n#endif\n#include <errno.h>\n"
        header += "#include <time.h>\n#include <dirent.h>\n#include <signal.h>\n#include <wchar.h>\n"
        header += "#include <sys/stat.h>\n#include <nl_types.h>\n#include <byteswap.h>\n"
        header += "#include <stdarg.h>\n"
        header += Path(DECOMPILER_CONFIG_IDA_DEF_H_FILE_PATH).read_text() + "\n"
        return header

    def get_decompiler_extra_header(self):
        return (
            Path(os.path.realpath(os.path.dirname(__file__)), "ida_extra.h").read_text()
            + "\n"
        )

    def is_cf_protection_branch_enabled(self, addr):
        return self.ida_ua.print_insn_mnem(addr).startswith("endbr")
