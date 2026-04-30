import logging
import os
import tempfile
from pathlib import Path

import pyhidra

logger = logging.getLogger("GhidraDecompiler")


class GhidraDecompiler:
    def __init__(self, binary_path):
        self.binary_path = binary_path

        self.temp_proj_dir_ctx = tempfile.TemporaryDirectory()
        self.temp_proj_dir = self.temp_proj_dir_ctx.__enter__()

        self.pyhidra_ctx = pyhidra.open_program(binary_path, self.temp_proj_dir)
        self.flat_program_api = self.pyhidra_ctx.__enter__()
        self.program = self.flat_program_api.getCurrentProgram()

        import ghidra

        self.ghidra = ghidra

        self.decompiler = self.ghidra.app.decompiler.DecompInterface()
        # self.decompiler.toggleParamMeasures(True)
        self.decompiler.openProgram(self.program)

        self.decomps = {}
        self.highfuncs = {}
        self.extra_struct_defs = ""

    def __del__(self):
        self.pyhidra_ctx.__exit__(None, None, None)
        self.temp_proj_dir_ctx.__exit__(None, None, None)

    def decompile(self, addr, high_function=False):
        if addr not in self.decomps:
            func = self._get_function_by_addr(addr)
            if func is None:
                self.decomps[addr] = None
                self.highfuncs[addr] = None
            else:
                logger.info(f"Decompiling function at {hex(addr)} {func.getName()}")
                d = self.decompiler.decompileFunction(
                    func, 0, self.ghidra.util.task.TaskMonitor.DUMMY
                )
                if not d.decompileCompleted():
                    raise Exception(
                        f"Failed to decompile function at {hex(addr)}: {d.getErrorMessage()}"
                    )
                self.decomps[addr] = d.getDecompiledFunction()
                self.highfuncs[addr] = d.getHighFunction()
        if self.decomps[addr] is None:
            raise Exception(f"Failed to decompile function at {hex(addr)}")
        return self.highfuncs[addr] if high_function else self.decomps[addr]

    def _get_function_by_addr(self, addr):
        return self.flat_program_api.getFunctionContaining(
            self.flat_program_api.toAddr(addr)
        )

    def get_decompile_text(self, addr):
        decompiled_text = self.decompile(addr).getC()

        # replace (in_FS_OFFSET + 0x28) with __readfsqword(0x28)
        decompiled_text = decompiled_text.replace(
            "(in_FS_OFFSET + ", "__readfsqword("
        )

        return decompiled_text

    def get_base_address(self):
        return self.program.getImageBase().getOffset()

    def get_architecture(self):
        language_id = str(self.program.getLanguageID())
        if language_id == "x86:LE:32:default":
            return "x86"
        elif language_id == "x86:LE:64:default":
            return "amd64"
        elif language_id.startswith("ARM:LE:32"):
            return "arm"
        elif language_id.startswith("ARM:LE:64"):
            return "aarch64"
        raise Exception("Unknown architecture")

    def get_function_by_name(self, name):
        funcs = self.program.getListing().getGlobalFunctions(name)
        if funcs is None or len(funcs) == 0:
            raise Exception(f"Function {name} not found")
        if len(funcs) > 1:
            raise Exception(f"Multiple functions with name {name} found")
        return funcs[0]

    def get_function_list(self, skip_dot_functions=True):
        funcs = self.program.getFunctionManager().getFunctionsNoStubs(True)
        func_addrs = []

        for func in funcs:
            if func.isThunk():
                continue
            func_addrs.append(func.getEntryPoint().getOffset())

        return func_addrs

    def is_function_bp_based(self, addr):
        func = self._get_function_by_addr(addr)
        if self.get_architecture() == "amd64":
            rbp_count, rsp_count = 0, 0
            code_units = self.program.getListing().getCodeUnits(func.getBody(), True)
            for code_unit in code_units:
                disasm = code_unit.toString()
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
        is_bp_based = self.is_function_bp_based(addr)
        high_func = self.decompile(addr, high_function=True)
        local_symbols = high_func.getLocalSymbolMap().getSymbols()
        for symbol in local_symbols:
            if symbol.isParameter():
                continue
            storage = symbol.getStorage()
            if storage.isRegisterStorage():
                stack_offsets.append(
                    {
                        "name": symbol.getName(),
                        "StkLoc": symbol.getStorage().getRegister().getName().lower(),
                    }
                )
            elif storage.isStackStorage():
                stack_offsets.append(
                    {
                        "name": symbol.getName(),
                        "StkLoc": symbol.getStorage().getStackOffset(),
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
        func = self._get_function_by_addr(addr)
        return func.getBody().getNumAddresses()

    def get_function_name(self, addr):
        func = self._get_function_by_addr(addr)
        return func.getName()

    def _normalize_symbol(self, symbol):
        # if @ is present, remove everything after it
        if "@" in symbol:
            symbol = symbol.split("@")[0]
        # replace everything that's not [a-zA-Z0-9_] with _
        return "".join([c if c.isalnum() or c == "_" else "_" for c in symbol])

    def get_symbols(self):
        symbols = {}
        symbol_table = self.program.getSymbolTable()

        for symbol in symbol_table.getAllSymbols(True):
            if symbol.isExternal():
                continue
            if (block := self.flat_program_api.getMemoryBlock(symbol.getAddress())) and block.isExternalBlock():
                continue
            if symbol.getAddress().isMemoryAddress():
                symbols[self._normalize_symbol(symbol.getName())] = symbol.getAddress().getOffset()
        return symbols

    def get_referenced_function_prototypes(self, addr):
        return self.get_referenced_function_prototypes_and_externs(addr)

    def get_referenced_function_prototypes_and_externs(self, addr):
        func = self._get_function_by_addr(addr)
        result = ""
        result += self.extra_struct_defs
        xrefs = {}
        called_funcs = {}
        for _addr in func.getBody().getAddresses(True):
            for xref in self.flat_program_api.getReferencesFrom(_addr):
                xrefs[xref.getToAddress().getOffset()] = xref  # dedup
        for xref in xrefs.values():
            if xref.getReferenceType().isCall():
                # continue # use -Wno-error=implicit-function-declaration instead
                called_func = self._get_function_by_addr(
                    xref.getToAddress().getOffset()
                )
                if called_func is None:
                    continue
                called_funcs[called_func.getEntryPoint().getOffset()] = called_func
            else:
                # result += f"// Unknown reference type {xref.getReferenceType()} {hex(xref.getToAddress().getOffset())}\n"
                logger.warning(f"Unknown reference type {xref.getReferenceType()}")
        for called_func_addr, called_func in called_funcs.items():
            result += self.decompile(called_func_addr).getSignature() + "\n"

        high_func = self.decompile(addr, high_function=True)
        if high_func:
            for var in high_func.getGlobalSymbolMap().getSymbols():
                type_str = var.getDataType().getDisplayName()
                if "[" in type_str:
                    before_bracket = type_str[:type_str.index("[")]
                    bracket_str  = type_str[type_str.index("["):]
                    result += f"extern {before_bracket} {var.getName()}{bracket_str};\n"
                else:
                    result += (
                        f"extern {var.getDataType().getDisplayName()} {var.getName()};\n"
                    )
        return result

    def get_decompiler_extra_header(self):
        return (
            Path(os.path.realpath(os.path.dirname(__file__)), "ghidra_extra.h").read_text()
            + "\n"
        )

    def get_general_compile_header(self):
        # https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Framework/SoftwareModeling/src/main/java/ghidra/program/model/data/DataTypeWriter.java

        result = ""
        result += "#include <stdbool.h>\n"
        result += "#include <langinfo.h>\n"
        result += "#include <stddef.h>\n#include <bits/types/FILE.h>\n#include <stdint.h>\n#if defined(__x86_64__)\n#include <emmintrin.h>\n#endif\n#include <errno.h>\n"
        result += "#include <time.h>\n#include <dirent.h>\n#include <signal.h>\n#include <wchar.h>\n"
        result += "#include <sys/stat.h>\n#include <nl_types.h>\n#include <byteswap.h>\n"
        

        from java.io import StringWriter

        writer = StringWriter()
        dtm = self.program.getDataTypeManager()
        local_source_archive = dtm.getLocalSourceArchive()
        builtin_source_archive = dtm.getSourceArchive(dtm.BUILT_IN_ARCHIVE_UNIVERSAL_ID)

        datatypes = dtm.getDataTypes(local_source_archive) + dtm.getDataTypes(builtin_source_archive)
        datatypes = [dt for dt in datatypes if "*" not in dt.getName()]
        datatypes = [dt for dt in datatypes if dt.getName() not in ("pointer")]

        dtw = self.ghidra.program.model.data.DataTypeWriter(dtm, writer)
        dtw.write(datatypes, self.ghidra.util.task.TaskMonitor.DUMMY)
        # dtw.write(dtm, self.ghidra.util.task.TaskMonitor.DUMMY)

        # dtm = self.program.getDataTypeManager()
        # for dt in dtm.getAllDataTypes():
        #     if isinstance(dt, self.ghidra.program.model.data.BuiltInDataType):
        #         if declaration := dt.getCTypeDeclaration(dtm.getDataOrganization()):
        #             result += declaration + "\n"

        result += writer.toString() + "\n"

        return result
