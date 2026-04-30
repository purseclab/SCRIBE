from scribe.decompilers.ida import IdaDecompiler

import argparse, json

parser = argparse.ArgumentParser(description="Decompile a function")
parser.add_argument("binary", type=str, help="Binary path")
parser.add_argument("function_addr", type=str, help="Function addr")

args = parser.parse_args()


decompiler = IdaDecompiler(args.binary)

print(f"Decompiling function at {args.function_addr} in {args.binary}")
if args.function_addr.startswith("0x") or args.function_addr.isnumeric():
    function_addr = int(args.function_addr, 0)
else:
    function_addr = decompiler.get_function_by_name(args.function_addr)

with open("/output/shared_header.h", "w") as f:
    f.write(decompiler.get_general_compile_header())

with open("/output/decompiler_header.h", "w") as f:
    f.write(decompiler.get_decompiler_extra_header())

with open("/output/symbols.json", "w") as f:
    f.write(json.dumps(decompiler.get_symbols()))

with open("/output/decompiled.c", "w") as f:
    f.write(decompiler.get_referenced_function_prototypes(function_addr))
    f.write(decompiler.get_decompile_text(function_addr))

with open("/output/localvars.json", "w") as f:
    f.write(json.dumps(decompiler.get_function_stack_offsets(function_addr)))

with open("/output/disassembled.asm", "w") as f:
    f.write(decompiler.get_disassembly(function_addr))
