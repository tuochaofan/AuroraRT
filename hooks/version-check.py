#!/usr/bin/env python3

"""
pre-commit-hook version-check
    ensures version information is consistent across different files
"""

import re
import sys
import json


cmake_version_regex = re.compile(r"project\s*\(\s*AuroraRT.*VERSION\s+([0-9]+\.[0-9]+\.[0-9]).*\)", re.IGNORECASE)


def main():
    # 检查CMakeLists.txt中的版本
    with open('CMakeLists.txt') as f:
        m = cmake_version_regex.search(f.read())
        if not m:
            print("Could not locate version information in CMakeLists.txt.", file=sys.stderr)
            sys.exit(1)
        cmake_version = m.group(1)

    # 检查vcpkg.json中的版本
    try:
        with open('vcpkg.json') as f:
            vcpkg_data = json.load(f)
            vcpkg_version = vcpkg_data.get('version', '')
            if vcpkg_version and vcpkg_version != cmake_version:
                print(f"vcpkg.json version:    {vcpkg_version}", file=sys.stderr)
                print(f"CMakeLists.txt version: {cmake_version}", file=sys.stderr)
                sys.exit(1)
    except FileNotFoundError:
        # vcpkg.json 不存在，跳过检查
        pass

    # 检查README.md中的版本
    try:
        with open('README.md') as f:
            readme_content = f.read()
            # 查找版本信息
            version_match = re.search(r"version\s*[:=]\s*([0-9]+\.[0-9]+\.[0-9])", readme_content, re.IGNORECASE)
            if version_match:
                readme_version = version_match.group(1)
                if readme_version != cmake_version:
                    print(f"README.md version:    {readme_version}", file=sys.stderr)
                    print(f"CMakeLists.txt version: {cmake_version}", file=sys.stderr)
                    sys.exit(1)
    except FileNotFoundError:
        # README.md 不存在，跳过检查
        pass

    print(f"Version check passed: {cmake_version}")


if __name__ == "__main__":
    main()
