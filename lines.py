from glob import glob

files = glob("./*.h") + glob("./*.c") + glob("./*.pl")

total_lines = 0
for file in files:
    with open(file, "rt", encoding="utf-8") as f:
        lines = f.readlines()
    total_lines += len(lines)

print(f"Total Lines: {total_lines}")
