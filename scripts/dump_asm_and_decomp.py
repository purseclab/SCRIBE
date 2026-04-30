from argparse import ArgumentParser
parser = ArgumentParser()
parser.add_argument("binary", help="Path to binary")
parser.add_argument("function", help="Function to decompile")
parser.add_argument("base_addr", help="Base address of binary")
args = parser.parse_args()

from scribe.config import DECOMPILER_CONFIG_IDA_PATH
from headless_ida import HeadlessIda
headlessida = HeadlessIda(f"{DECOMPILER_CONFIG_IDA_PATH}/idat64", args.binary)


import idautils, ida_funcs, ida_hexrays, idc, ida_lines, ida_nalt, ida_auto

def get_function_by_name(name):
    for ea in idautils.Functions():
        if ida_funcs.get_func_name(ea) == name:
            return ea
    return None

def decompile_function(ea):
    cfunc = ida_hexrays.decompile(ea)
    if cfunc is None:
        return ""
    return str(cfunc)

def disassemble_function(ea):
    func = ida_funcs.get_func(ea)
    start, end = func.start_ea, func.end_ea
    asm = "/*\n"
    for instr in func.head_items():
        asm += f"{hex(instr)}:\t{ida_lines.tag_remove(idc.GetDisasm(instr))}\n"
    asm += "*/\n"
    return asm


ida_auto.auto_wait()

input_base_addr = int(args.base_addr, 0)
ida_base_addr = ida_nalt.get_imagebase()


if args.function.isnumeric():
    function = int(args.function, 0) - input_base_addr + ida_base_addr
else:
    function = get_function_by_name(args.function)

if idc.GetDisasm(function).startswith("jmp"):
    function = get_function_by_name(idc.GetDisasm(function).split()[1])

with open(f"{args.binary}.c", "w") as f:
    f.write(disassemble_function(function))
    f.write(decompile_function(function))
