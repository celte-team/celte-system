import re
import sys
from collections import defaultdict
from pprint import pprint


def parse_cpp_file(file_path):
    functions_by_region = defaultdict(list)
    current_region = None
    current_function = None
    collecting_function = False
    function_lines = []
    is_specific = 0

    # Regex patterns
    region_start_pattern = re.compile(r'#pragma region (.+)')
    region_end_pattern = re.compile(r'#pragma endregion')
    function_start_pattern = re.compile(
        r'EXPORT\s+(?:\[\[nodiscard\]\]\s*)?(\w[\w\s:&*]*)\s+(\w+)\s*\(')
    function_end_pattern = re.compile(r'.*\)\s*{.*')
    function_end_pattern2 = re.compile(r'.*\)(?!.)')
    doc_line_pattern = re.compile(r'///\s?(.+)')

    with open(file_path, 'r') as file:
        lines = file.readlines()

    for line in lines:
        region_start_match = region_start_pattern.match(line)
        region_end_match = region_end_pattern.match(line)
        doc_line_match = doc_line_pattern.match(line)

        if region_start_match:
            current_region = region_start_match.group(1)
        elif region_end_match:
            current_region = None

        elif doc_line_match and current_function is None:
            current_function = {'docs': [doc_line_match.group(1)]}

        elif doc_line_match and current_function:
            current_function['docs'].append(doc_line_match.group(1))

        elif function_start_pattern.search(line):
            collecting_function = True
            # print("the function start: " + line.strip())
        elif "CELTE_SERVER_MODE_ENABLED" in line.strip():
            is_specific = 1
        elif "CELTE_CLIENT_MODE_ENABLED" in line.strip():
            is_specific = -1
        elif line.strip().startswith("#else"):
            is_specific *= -1
        elif line.strip().startswith("#endif"):
            is_specific = 0

        if collecting_function:
            function_lines.append(line.strip())
            pattern = function_end_pattern2.search(function_lines[-1])
            pattern2 = function_end_pattern.search(function_lines[-1])

            # print(pattern)
            # print(pattern2)

            if function_end_pattern.search(line) or pattern:
                # print("end of function")
                full_function = " ".join(function_lines)
                function_match = re.search(
                    r'(?:EXPORT\s+)(?:\[\[nodiscard\]\]\s*)?(\w[\w\s:&*]*)\s+(\w+)\s*\((.*?)\)\s*(?:\{|$)', full_function, re.DOTALL)
                if current_function is None:
                    current_function = {"docs": []}

                if function_match:
                    return_type, name, args = function_match.groups()
                    current_function.update({
                        'name': name.strip(),
                        'return_type': return_type.strip(),
                        'args': args.strip(),
                        'scope': is_specific
                    })
                    functions_by_region[current_region].append(
                        current_function)
                    current_function = None
                    collecting_function = False
                    function_lines = []

    return functions_by_region


def inject_function_into_struct(file_path, struct_name, functions_by_region):
    # Read the original file
    with open(file_path, 'r') as file:
        lines = file.readlines()

    # Patterns to find the struct and where to insert the function

    struct_start_pattern = re.compile(rf'struct\s+{struct_name}\s*\{{')
    region_comment_pattern = re.compile(r'\s*/\*\s+[-]+\s+(.+)\s+[-]+\s+\*/')
    struct_end_pattern = re.compile(r'^\s*};')

    inside_struct = False
    current_region = None
    new_lines = []
    for line in lines:
        if struct_start_pattern.search(line):
            inside_struct = True

        if inside_struct:
            region_comment_match = region_comment_pattern.match(line)
            if region_comment_match:
                current_region = region_comment_match.group(1).strip()
            elif struct_end_pattern.match(line):
                inside_struct = False
                current_region = None

        new_lines.append(line)

        if inside_struct and current_region in functions_by_region:
            if len(functions_by_region[current_region]) > 0:
                new_lines.append("\n")
            for func in functions_by_region[current_region]:
                f = f"{func['return_type']} (*{func['name']})({func['args']});"
                new_lines.append(f"        {f}\n")
            functions_by_region[current_region] = []

    with open(file_path, 'w') as file:
        file.writelines(new_lines)


def get_string_to_insert(func, mode):
    if mode == 0:
        return "{ \"" + func["name"] + \
            "\", reinterpret_cast<void**>(&_celteBindings." + \
            func["name"] + ") },"
    if mode == 1:
        return "///" + "\n\t/// ".join(func['docs']) + "\n\t" + \
            f'{func["return_type"]} {func["name"]}({func["args"]});'

    if mode == 2:
        f = 'ClassDB::bind_method(D_METHOD(\"' + func["name"]
        arg_name = [arg.strip().split(" ")[-1]
                    for arg in func["args"].split(",")]
        if len(arg_name) > 0 and arg_name[0] != '':
            pprint(arg_name)
            f += "\", \"" + "\", \"".join(arg_name)
        f += "\"), &CAPI::" + \
            func["name"] + ",\n\t\"" + \
            "\\n\"\n\t\"".join(func["docs"]) + "\");"
        return f
    return ""


def inject_function_into_something(path, functions_by_region, map_name, scope, mode):
    with open(path, 'r') as file:
        lines = file.readlines()

    region_comment_pattern = re.compile(r'\s*/\*\s+[-]+\s+(.+)\s+[-]+\s+\*/')
    struct_end_pattern = re.compile(r'^\s*};')

    inside_struct = False
    current_region = None
    new_lines = []
    for line in lines:
        if map_name in line:
            inside_struct = True

        if inside_struct:
            region_comment_match = region_comment_pattern.match(line)
            if region_comment_match:
                current_region = region_comment_match.group(1).strip()
            elif (mode == 1 and "private:" in line) or \
                 (mode == 2 and "}" in line) or \
                 (mode == 0 and struct_end_pattern.match(line)):
                inside_struct = False
                current_region = None

        new_lines.append(line)

        if inside_struct and current_region in functions_by_region:
            # Insert the functions for the current region
            if len(functions_by_region[current_region]) > 0:
                new_lines.append("\n")
            for func in functions_by_region[current_region]:
                if func["scope"] == scope or mode >= 1:
                    f = get_string_to_insert(func, mode)
                    new_lines.append(f"        {f}\n")
                    new_lines.append("\n")
            # Clear functions to avoid inserting them multiple times
            functions_by_region[current_region] = []

    # Write the modified lines back to the file
    with open(path, 'w') as file:
        file.writelines(new_lines)


def parse_existing_func(path, functions):
    copy = {}
    with open(path, 'r') as file:
        lines = file.readlines()
        pattern = re.compile(r'\w+\s*\*?\s*\(\*\s*(\w+)\)\s*\(.*')

        # Find all matches in the struct definition
        matches = pattern.findall(''.join(lines))
    # Print extracted functions by region
        for region, funcs in functions.items():
            copy[region] = []
            for func in funcs:
                if func["name"] not in matches:
                    copy[region].append(func)
    return copy


def inject_function_at_the_end(path, functions):
    with open(path, 'r') as file:
        lines = file.readlines()

    for region, funcs in functions.items():
        for func in funcs:
            lines.append(
                f'{func["return_type"]} CAPI::{func["name"]}({func["args"]})' + '{\n}\n\n')

    with open(path, 'w') as file:
        file.writelines(lines)


def main(argc, argv):
    print(argv)
    # Example usage
    functions = parse_cpp_file(argv[1])
    copy = parse_existing_func(argv[2], functions)
    copy2 = parse_existing_func(argv[2], functions)
    copy3 = parse_existing_func(argv[2], functions)
    copy4 = parse_existing_func(argv[2], functions)
    copy5 = parse_existing_func(argv[2], functions)
    copy6 = parse_existing_func(argv[2], functions)
    copy7 = parse_existing_func(argv[2], functions)

    map_name1 = "std::map<std::string, void**> bindingsMap = {"
    map_name_client = "std::map<std::string, void**> clientBindingsMap = {"
    map_name_server = "std::map<std::string, void**> serverBindingsMap"

    inject_function_into_struct(argv[2], "CelteBindings", copy)
    inject_function_into_something(argv[2], copy2, map_name1, 0, 0)
    inject_function_into_something(argv[2], copy3, map_name_client, -1, 0)
    inject_function_into_something(argv[2], copy4, map_name_server, 1, 0)
    inject_function_into_something(argv[2], copy5, "public:", 0, 1)

    inject_function_into_something(
        argv[3], copy6, "void CAPI::_bind_methods()", 0, 2)

    inject_function_at_the_end(
        argv[3], copy7)


if __name__ == "__main__":
    main(len(sys.argv), sys.argv)
