#!/usr/bin/env python3

"""
AuroraRT 消息定义解析器

解析 .aurora.msg、.aurora.srv 和 .aurora.action 文件，生成消息定义的抽象语法树
"""

import re
import os

class MsgField:
    """消息字段定义"""
    def __init__(self, field_type, field_name, array_size=None):
        self.field_type = field_type
        self.field_name = field_name
        self.array_size = array_size
        self.is_array = array_size is not None
    
    def __str__(self):
        if self.is_array:
            if self.array_size:
                return f"{self.field_type}[{self.array_size}] {self.field_name}"
            else:
                return f"{self.field_type}[] {self.field_name}"
        else:
            return f"{self.field_type} {self.field_name}"

class MsgDefinition:
    """消息定义"""
    def __init__(self, name):
        self.name = name
        self.fields = []
        self.imports = []
        self.serialization_formats = ['cdr']  # 默认使用CDR格式
    
    def add_field(self, field):
        self.fields.append(field)
    
    def add_import(self, import_path):
        self.imports.append(import_path)
    
    def add_serialization_format(self, fmt):
        if fmt not in self.serialization_formats:
            self.serialization_formats.append(fmt)
    
    def __str__(self):
        lines = []
        for imp in self.imports:
            lines.append(f"import {imp}")
        for fmt in self.serialization_formats:
            lines.append(f"# serialization: {fmt}")
        for field in self.fields:
            lines.append(str(field))
        return "\n".join(lines)

class SrvDefinition:
    """服务定义"""
    def __init__(self, name):
        self.name = name
        self.request = MsgDefinition(f"{name}_Request")
        self.response = MsgDefinition(f"{name}_Response")
    
    def __str__(self):
        lines = []
        lines.append("# Request")
        lines.append(str(self.request))
        lines.append("---")
        lines.append("# Response")
        lines.append(str(self.response))
        return "\n".join(lines)

class ActionDefinition:
    """动作定义"""
    def __init__(self, name):
        self.name = name
        self.goal = MsgDefinition(f"{name}_Goal")
        self.result = MsgDefinition(f"{name}_Result")
        self.feedback = MsgDefinition(f"{name}_Feedback")
    
    def __str__(self):
        lines = []
        lines.append("# Goal")
        lines.append(str(self.goal))
        lines.append("---")
        lines.append("# Result")
        lines.append(str(self.result))
        lines.append("---")
        lines.append("# Feedback")
        lines.append(str(self.feedback))
        return "\n".join(lines)

def parse_field(line):
    """解析字段定义
    
    Args:
        line: 字段定义行
        
    Returns:
        MsgField: 字段对象
    """
    # 匹配类型、名称和可选的数组大小
    # 支持 int32[] values 和 int32[5] values 格式
    field_pattern = r'^([a-zA-Z0-9_]+)(\[(\d*)\])?\s+([a-zA-Z0-9_]+)$'
    line_stripped = line.strip()
    match = re.match(field_pattern, line_stripped)
    if match:
        field_type = match.group(1)
        field_name = match.group(4)
        array_size = match.group(3) if match.group(2) else None
        if array_size:
            array_size = int(array_size) if array_size else None
        
        return MsgField(field_type, field_name, array_size)
    else:
        raise ValueError(f"Invalid field definition: {line}")

def parse_msg_file(file_path):
    """解析消息文件
    
    支持 .aurora.msg 和 ROS2 的 .msg 文件格式
    
    Args:
        file_path: 消息文件路径
        
    Returns:
        MsgDefinition: 消息定义对象
    """
    # 尝试使用utf-8编码读取，如果失败则使用gbk
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        with open(file_path, 'r', encoding='gbk') as f:
            content = f.read()
    
    # 处理 .aurora.msg 文件格式，提取正确的消息名称
    basename = os.path.basename(file_path)
    if basename.endswith('.aurora.msg'):
        msg_name = basename[:-11]  # 移除 .aurora.msg 后缀
    elif basename.endswith('.msg'):
        msg_name = os.path.splitext(basename)[0]
    else:
        msg_name = os.path.splitext(basename)[0]
    msg_def = MsgDefinition(msg_name)
    
    # 解析每一行
    for line in content.split('\n'):
        # 去除空白
        line_stripped = line.strip()
        
        # 解析注释中的序列化格式指令
        if line_stripped.startswith('# serialization:'):
            fmt_str = line_stripped.split(':', 1)[1].strip()
            # 解析逗号分隔的序列化格式列表
            for fmt in fmt_str.split(','):
                fmt = fmt.strip()
                if fmt:
                    msg_def.add_serialization_format(fmt)
            continue
        
        # 跳过其他注释
        if not line_stripped or line_stripped.startswith('#'):
            continue
        
        # 解析导入语句
        if line_stripped.startswith('import '):
            import_path = line_stripped[7:].strip()
            msg_def.add_import(import_path)
            continue
        
        # 解析字段定义
        field = parse_field(line_stripped)
        msg_def.add_field(field)
    
    return msg_def

def parse_srv_file(file_path):
    """解析服务文件
    
    支持 .aurora.srv 和 ROS2 的 .srv 文件格式
    
    Args:
        file_path: 服务文件路径
        
    Returns:
        SrvDefinition: 服务定义对象
    """
    # 尝试使用utf-8编码读取，如果失败则使用gbk
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        with open(file_path, 'r', encoding='gbk') as f:
            content = f.read()
    
    srv_name = os.path.splitext(os.path.basename(file_path))[0]
    srv_def = SrvDefinition(srv_name)
    
    # 分割为请求和响应部分
    parts = content.split('---')
    if len(parts) != 2:
        raise ValueError(f"Invalid srv file format: {file_path}")
    
    # 解析请求部分
    for line in parts[0].split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        if line.startswith('import '):
            import_path = line[7:].strip()
            srv_def.request.add_import(import_path)
            continue
        
        field = parse_field(line)
        srv_def.request.add_field(field)
    
    # 解析响应部分
    for line in parts[1].split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        if line.startswith('import '):
            import_path = line[7:].strip()
            srv_def.response.add_import(import_path)
            continue
        
        field = parse_field(line)
        srv_def.response.add_field(field)
    
    return srv_def

def parse_action_file(file_path):
    """解析动作文件
    
    支持 .aurora.action 和 ROS2 的 .action 文件格式
    
    Args:
        file_path: 动作文件路径
        
    Returns:
        ActionDefinition: 动作定义对象
    """
    # 尝试使用utf-8编码读取，如果失败则使用gbk
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        with open(file_path, 'r', encoding='gbk') as f:
            content = f.read()
    
    action_name = os.path.splitext(os.path.basename(file_path))[0]
    action_def = ActionDefinition(action_name)
    
    # 分割为goal、result和feedback部分
    parts = content.split('---')
    if len(parts) != 3:
        raise ValueError(f"Invalid action file format: {file_path}")
    
    # 解析goal部分
    for line in parts[0].split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        if line.startswith('import '):
            import_path = line[7:].strip()
            action_def.goal.add_import(import_path)
            continue
        
        field = parse_field(line)
        action_def.goal.add_field(field)
    
    # 解析result部分
    for line in parts[1].split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        if line.startswith('import '):
            import_path = line[7:].strip()
            action_def.result.add_import(import_path)
            continue
        
        field = parse_field(line)
        action_def.result.add_field(field)
    
    # 解析feedback部分
    for line in parts[2].split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        if line.startswith('import '):
            import_path = line[7:].strip()
            action_def.feedback.add_import(import_path)
            continue
        
        field = parse_field(line)
        action_def.feedback.add_field(field)
    
    return action_def

def parse_directory(directory):
    """解析目录中的消息定义文件
    
    支持 AuroraRT 的 .aurora.msg、.aurora.srv、.aurora.action 文件格式
    也支持 ROS2 的 .msg、.srv、.action 文件格式
    
    Args:
        directory: 包含消息定义文件的目录
        
    Returns:
        dict: 包含消息、服务和动作定义的字典
    """
    definitions = {
        'msg': {},
        'srv': {},
        'action': {}
    }
    
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.aurora.msg') or file.endswith('.msg'):
                file_path = os.path.join(root, file)
                msg_def = parse_msg_file(file_path)
                definitions['msg'][msg_def.name] = msg_def
            elif file.endswith('.aurora.srv') or file.endswith('.srv'):
                file_path = os.path.join(root, file)
                srv_def = parse_srv_file(file_path)
                definitions['srv'][srv_def.name] = srv_def
            elif file.endswith('.aurora.action') or file.endswith('.action'):
                file_path = os.path.join(root, file)
                action_def = parse_action_file(file_path)
                definitions['action'][action_def.name] = action_def
    
    return definitions

def parse_msg_directory(directory):
    """解析目录中的消息文件
    
    支持 AuroraRT 的 .aurora.msg 文件和 ROS2 的 .msg 文件格式
    
    Args:
        directory: 包含消息文件的目录
        
    Returns:
        dict: 消息名称到 MsgDefinition 的映射
    """
    defs = parse_directory(directory)
    return defs['msg']

if __name__ == '__main__':
    # 测试解析器
    import sys
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file or directory>")
        sys.exit(1)
    
    path = sys.argv[1]
    if os.path.isfile(path):
        if path.endswith('.aurora.msg'):
            msg_def = parse_msg_file(path)
            print(f"Message: {msg_def.name}")
            print("Fields:")
            for field in msg_def.fields:
                print(f"  - {field}")
        elif path.endswith('.aurora.srv'):
            srv_def = parse_srv_file(path)
            print(f"Service: {srv_def.name}")
            print("Request Fields:")
            for field in srv_def.request.fields:
                print(f"  - {field}")
            print("Response Fields:")
            for field in srv_def.response.fields:
                print(f"  - {field}")
        elif path.endswith('.aurora.action'):
            action_def = parse_action_file(path)
            print(f"Action: {action_def.name}")
            print("Goal Fields:")
            for field in action_def.goal.fields:
                print(f"  - {field}")
            print("Result Fields:")
            for field in action_def.result.fields:
                print(f"  - {field}")
            print("Feedback Fields:")
            for field in action_def.feedback.fields:
                print(f"  - {field}")
        else:
            print(f"Unsupported file type: {path}")
            sys.exit(1)
    elif os.path.isdir(path):
        defs = parse_directory(path)
        print(f"Found {len(defs['msg'])} messages, {len(defs['srv'])} services, {len(defs['action'])} actions:")
        
        if defs['msg']:
            print("\nMessages:")
            for name, msg_def in defs['msg'].items():
                print(f"  - {name}")
        
        if defs['srv']:
            print("\nServices:")
            for name, srv_def in defs['srv'].items():
                print(f"  - {name}")
        
        if defs['action']:
            print("\nActions:")
            for name, action_def in defs['action'].items():
                print(f"  - {name}")
    else:
        print(f"Path not found: {path}")
        sys.exit(1)
