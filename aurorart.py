#!/usr/bin/env python3
"""
AuroraRT 构建工具
类似 colcon/catkin_make/Ament 的编译工具
"""

import os
import sys
import argparse
import subprocess
import json
import multiprocessing
from pathlib import Path

# 添加 aurorart_build 模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'aurorart_build'))

from aurorart_build.project_discovery import discover_projects
from aurorart_build.build_system import build_project
from aurorart_build.dependency_resolver import resolve_dependencies
from aurorart_build.installer import install_project
from aurorart_build.env_setup import generate_env_setup

def main():
    parser = argparse.ArgumentParser(description='AuroraRT 构建工具')
    parser.add_argument('command', choices=['build', 'install', 'test', 'clean', 'env', 'list'],
                       help='执行的命令')
    parser.add_argument('--workspace', '-w', default=os.getcwd(),
                       help='工作空间目录')
    parser.add_argument('--build-dir', '-b', default='build',
                       help='构建目录')
    parser.add_argument('--install-dir', '-i', default='install',
                       help='安装目录')
    parser.add_argument('--parallel', '-p', type=int, default=multiprocessing.cpu_count(),
                       help='并行构建数量')
    parser.add_argument('--packages-select', '-s', nargs='+',
                       help='选择要构建的包')
    parser.add_argument('--xmake-args', nargs='*',
                       help='传递给 xmake 的参数')
    
    args = parser.parse_args()
    
    workspace = Path(args.workspace)
    build_dir = workspace / args.build_dir
    install_dir = workspace / args.install_dir
    
    if args.command == 'build':
        # 发现项目
        projects = discover_projects(workspace)
        
        # 解析依赖
        resolved_projects = resolve_dependencies(projects)
        
        # 构建项目
        for project in resolved_projects:
            print(f"构建项目: {project['name']}")
            build_project(project, build_dir, install_dir, args.parallel, args.xmake_args)
            
    elif args.command == 'install':
        # 发现项目
        projects = discover_projects(workspace)
        
        # 解析依赖
        resolved_projects = resolve_dependencies(projects)
        
        # 安装项目
        for project in resolved_projects:
            print(f"安装项目: {project['name']}")
            install_project(project, build_dir, install_dir)
            
    elif args.command == 'test':
        # 运行测试
        # 发现项目
        projects = discover_projects(workspace)
        
        # 解析依赖
        resolved_projects = resolve_dependencies(projects)
        
        # 运行测试
        for project in resolved_projects:
            print(f"运行项目测试: {project['name']}")
            if project['type'] == 'xmake':
                subprocess.run(['xmake', 'test'], cwd=project['path'], check=True)
            else:
                print(f"不支持的项目类型: {project['type']}")
            
    elif args.command == 'clean':
        # 清理构建目录
        if build_dir.exists():
            import shutil
            shutil.rmtree(build_dir)
            print(f"清理构建目录: {build_dir}")
        if install_dir.exists():
            shutil.rmtree(install_dir)
            print(f"清理安装目录: {install_dir}")
            
    elif args.command == 'env':
        # 生成环境设置脚本
        generate_env_setup(install_dir, workspace)
        print(f"生成环境设置脚本到: {install_dir}")
        
    elif args.command == 'list':
        # 列出发现的项目
        projects = discover_projects(workspace)
        print("发现的项目:")
        for project in projects:
            print(f"- {project['name']} (类型: {project['type']})")
            if 'dependencies' in project:
                print(f"  依赖: {', '.join(project['dependencies'])}")

if __name__ == '__main__':
    main()
