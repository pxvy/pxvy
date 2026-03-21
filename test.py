import subprocess
import re


def count_macro():
    with open("guid.h", "rt", encoding="utf-8") as f:
        lines = f.readlines()
    count = 0
    pattern = re.compile(r'^\s*#define\s+(REG_[A-Z0-9_-]+)\s+"([^"]*)"\s*$')
    for line in lines:
        if line.startswith("#define") and "REG_" in line:
            match = pattern.match(line)
            if match:
                count += 1
    return count


def get_guid():
    result = subprocess.run(
        ["powershell", "-Command", "[guid]::NewGuid().Guid"],
        capture_output=True,
        text=True,
        check=True,
        encoding='utf-8'
    )

    guid_str = result.stdout.strip()
    return guid_str


def apply_random_guid_header(guids: list[str]):
    with open("guid.h", "rt", encoding="utf-8") as f:
        lines = f.readlines()
    new_lines = []
    guid_index = 0
    pattern = re.compile(r'^\s*#define\s+(REG_[A-Z0-9_-]+)\s+"([^"]*)"\s*$')
    for line in lines:
        if line.startswith("#define") and "REG_" in line:
            match = pattern.match(line)
            if match:
                macro_name = match.group(1)
                guid_value = guids[guid_index]
                guid_index += 1
                new_line = f'#define\t{macro_name}\t\t"{guid_value}"\n'
                new_lines.append(new_line)
        else:
            new_lines.append(line)
    print(''.join(new_lines))
    with open("guid.h", "wt", encoding="utf-8", newline="") as f:
        f.writelines(new_lines)


if __name__ == '__main__':
    macro_count = count_macro()
    print(macro_count)
    guids = []
    for i in range(macro_count):
        guids.append(get_guid())
    apply_random_guid_header(guids)
