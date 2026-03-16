#!/usr/bin/env python3

"""
AuroraRT 消息查看器安装脚本
"""

from setuptools import setup, find_packages

setup(
    name='aurora_msg_viewer',
    version='1.0.0',
    description='AuroraRT message visualization tool',
    author='AuroraRT Team',
    author_email='aurorart@example.com',
    packages=find_packages(),
    scripts=['aurora_msg_viewer'],
    install_requires=[
        'argparse',
        'json',
    ],
    classifiers=[
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.6',
)
