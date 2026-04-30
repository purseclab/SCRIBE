from openai import OpenAI
from argparse import ArgumentParser
import logging
import re

# Set up logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

parser = ArgumentParser()
parser.add_argument("dir", type=str, help="CVE directory")
args = parser.parse_args()

import os

client = OpenAI(
    api_key=os.environ["OPENAI_API_KEY"],
    base_url=os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1"),
)

MODEL_NAME = "claude-sonnet-4-5-20250929"

SYSTEM_PROMPT = """
You are an expert in applying source code patches.
Given SEARCH/REPLACE blocks and a code file, apply the patches precisely.
All code and blocks are wrapped in explicit XML-like tags (e.g., <CODE>, <DIFF>, <SEARCH_REPLACE_BLOCKS>).
Only return the fully patched file inside <CODE>...</CODE> tags and nothing else.
If you cannot apply a patch exactly, do the closest reasonable edit preserving code structure.
If your output exceeds 8192 tokens, end with [CONTINUE] and wait for further instruction.
"""

SEARCH_REPLACE_PROMPT = """Apply the following SEARCH/REPLACE blocks to the code file below.

<SEARCH_REPLACE_BLOCKS>
{blocks}
</SEARCH_REPLACE_BLOCKS>

<CODE>
{original}
</CODE>

Return ONLY the patched code, wrapped in <CODE>...</CODE> tags, and NOTHING else.
If some blocks cannot be precisely located, edit the code as reasonably as possible to achieve the intended result.
"""

def parse_unified_diff(diff_text):
    """Convert a unified diff to a list of (search, replace) blocks."""
    blocks = []
    diff_lines = diff_text.splitlines()
    i = 0
    while i < len(diff_lines):
        line = diff_lines[i]
        if line.startswith('@@'):
            old_block = []
            new_block = []
            i += 1
            while i < len(diff_lines):
                l = diff_lines[i]
                if l.startswith('@@') or l.startswith('---') or l.startswith('+++'):
                    break
                if l.startswith('-'):
                    old_block.append(l[1:])
                elif l.startswith('+'):
                    new_block.append(l[1:])
                elif l.startswith(' '):
                    old_block.append(l[1:])
                    new_block.append(l[1:])
                i += 1
            search = '\n'.join(old_block).strip()
            replace = '\n'.join(new_block).strip()
            if search != replace:  # only add if there is a change
                blocks.append((search, replace))
        else:
            i += 1
    return blocks

def build_search_replace_blocks(blocks):
    """Format SEARCH/REPLACE blocks as a single tagged string."""
    out = []
    for search, replace in blocks:
        out.append(
            "<<<<<<< SEARCH\n" +
            search +
            "\n=======\n" +
            replace +
            "\n>>>>>>> REPLACE"
        )
    return '\n\n'.join(out)

def parse_search_replace_blocks(text):
    pattern = r"<<<<<<< SEARCH\n(.*?)=======\n(.*?)>>>>>>> REPLACE"
    blocks = re.findall(pattern, text, flags=re.DOTALL)
    return [(search.strip(), replace.strip()) for (search, replace) in blocks]

def apply_search_replace(original, blocks):
    code = original
    for search_block, replace_block in blocks:
        if search_block in code:
            code = code.replace(search_block, replace_block)
        else:
            cleaned_code = re.sub(r'\s+', '', code)
            cleaned_search = re.sub(r'\s+', '', search_block)
            if cleaned_search in cleaned_code:
                logging.warning('Applying patch with whitespace-insensitive match.')
                code = code.replace(search_block, replace_block)
            else:
                logging.error('Block not found, falling back to LLM.')
                return None
    return code

def extract_code_from_tags(text):
    # Extract content between <CODE>...</CODE> tags
    match = re.search(r"<CODE>(.*?)</CODE>", text, flags=re.DOTALL | re.IGNORECASE)
    if match:
        return match.group(1).strip()
    return text.strip()

def apply_patch_llm(original, search_replace_blocks):
    full_response = ""
    continuation = True
    continuation_count = 0
    user_prompt = SEARCH_REPLACE_PROMPT.format(
        original=original,
        blocks=search_replace_blocks
    )
    logging.debug('Delegating patch application to LLM.')
    while continuation:
        completion = client.chat.completions.create(
            model=MODEL_NAME,
            messages=[
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_prompt},
                {"role": "assistant", "content": full_response}
            ],
            max_tokens=8192,
        )
        chunk = completion.choices[0].message.content.strip()
        logging.debug(f'Received response chunk of length: {len(chunk)}')
        if chunk.endswith('[CONTINUE]'):
            full_response += chunk.removesuffix('[CONTINUE]')
            user_prompt = "Continue the previous output. Just continue <CODE> content."
            continuation_count += 1
            if continuation_count > 10:
                logging.warning('Exceeded maximum continuations. Stopping process.')
                break
        else:
            full_response += chunk
            continuation = False
    return extract_code_from_tags(full_response)

# ---- Main program ----
with open(f"{args.dir}/from_decompiler/decompiled.c") as f:
    original = f.read()
with open(f"{args.dir}/src_patch.diff") as f:
    diff = f.read()

# Apply input tags for clarity
original_tagged = original.strip()
diff_tagged = f"<DIFF>\n{diff.strip()}\n</DIFF>"

# Convert diff to SEARCH/REPLACE blocks
blocks = parse_unified_diff(diff)
search_replace_blocks = build_search_replace_blocks(blocks)
blocks_tagged = f"<SEARCH_REPLACE_BLOCKS>\n{search_replace_blocks}\n</SEARCH_REPLACE_BLOCKS>"

logging.info('Applying patch using SEARCH/REPLACE blocks.')
blocks_parsed = parse_search_replace_blocks(search_replace_blocks)
result = apply_search_replace(original_tagged, blocks_parsed)
if result is None:
    logging.info('Local application failed; using LLM for patch application.')
    result = apply_patch_llm(original_tagged, search_replace_blocks)
else:
    logging.info('Patch applied locally.')

output_path = f"{args.dir}/llm_patched.c"
with open(output_path, "w") as f:
    f.write(result)
logging.info(f'Patched code written to {output_path}')
