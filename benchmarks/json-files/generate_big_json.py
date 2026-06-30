import os
import json

input_path = "twitter.json"
output_path = "gigantic_test.json"
target_size = 1 * 1024 * 1024 * 1024  # 1 GB in bytes

if not os.path.exists(input_path):
    print(f"Error: {input_path} not found! Run this inside your json-files folder.")
    exit(1)

print("Reading and cleaning source payload chunk...")
with open(input_path, "r", encoding="utf-8") as f:
    raw_content = f.read().strip()

# Safely check if it contains multiple objects/lines and wrap them if necessary
try:
    # Try parsing the raw content as a single unified chunk
    json.loads(raw_content)
    clean_chunk = raw_content
except json.JSONDecodeError:
    # If it fails with extra data, assume it is line-delimited/concatenated JSON
    # We will clean it up and wrap it into a valid unified block element
    lines = [line.strip() for line in raw_content.splitlines() if line.strip()]
    cleaned_elements = []
    for line in lines:
        try:
            json.loads(line)
            cleaned_elements.append(line)
        except json.JSONDecodeError:
            continue # Skip any half-broken text frames
    
    clean_chunk = ",".join(cleaned_elements)

# Calculate iterations required based on our clean text block size
single_block_size = len(clean_chunk)
if single_block_size == 0:
    print("Error: Source file contains no valid JSON components.")
    exit(1)

repetitions = target_size // (single_block_size + 1)
print(f"Generating 1GB layout ({output_path}) by repeating payload {repetitions} times...")

# Stream the text chunks incrementally directly to disk
with open(output_path, "w", encoding="utf-8") as f:
    f.write("[")
    for i in range(repetitions):
        f.write(clean_chunk)
        if i < repetitions - 1:
            f.write(",")
    f.write("]")

final_size = os.path.getsize(output_path)
print(f"Success! Generated {output_path} ({final_size / (1024**3):.2f} GB)")