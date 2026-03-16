#!/usr/bin/env python3

"""
AuroraRT 消息落盘和回灌工具

用于保存消息到文件和从文件回灌消息
"""

import os
import sys
import time
import argparse
import json
import struct
from datetime import datetime

class MsgRecorder:
    """消息记录器"""
    def __init__(self, output_dir='./records'):
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
    
    def record_message(self, msg_type, msg_data, timestamp=None):
        """记录消息到文件
        
        Args:
            msg_type: 消息类型名称
            msg_data: 消息数据（字节数组）
            timestamp: 时间戳（可选）
            
        Returns:
            str: 保存的文件路径
        """
        if timestamp is None:
            timestamp = datetime.now()
        
        # 按照日期创建目录
        date_dir = timestamp.strftime('%Y-%m-%d')
        full_dir = os.path.join(self.output_dir, date_dir)
        os.makedirs(full_dir, exist_ok=True)
        
        # 生成文件名
        filename = f"{msg_type}_{timestamp.strftime('%H-%M-%S-%f')}.msg"
        file_path = os.path.join(full_dir, filename)
        
        # 写入消息头和数据
        with open(file_path, 'wb') as f:
            # 写入消息类型
            msg_type_bytes = msg_type.encode('utf-8')
            f.write(struct.pack('Q', len(msg_type_bytes)))
            f.write(msg_type_bytes)
            
            # 写入时间戳
            timestamp_ns = int(timestamp.timestamp() * 1e9)
            f.write(struct.pack('Q', timestamp_ns))
            
            # 写入消息长度
            f.write(struct.pack('Q', len(msg_data)))
            
            # 写入消息数据
            f.write(msg_data)
        
        return file_path
    
    def record_message_from_json(self, msg_type, json_data, timestamp=None):
        """从JSON记录消息
        
        Args:
            msg_type: 消息类型名称
            json_data: JSON格式的消息数据
            timestamp: 时间戳（可选）
            
        Returns:
            str: 保存的文件路径
        """
        msg_data = json.dumps(json_data).encode('utf-8')
        return self.record_message(msg_type, msg_data, timestamp)
    
    def load_message(self, file_path):
        """从文件加载消息
        
        Args:
            file_path: 消息文件路径
            
        Returns:
            tuple: (msg_type, timestamp, msg_data)
        """
        with open(file_path, 'rb') as f:
            # 读取消息类型
            type_len = struct.unpack('Q', f.read(8))[0]
            msg_type = f.read(type_len).decode('utf-8')
            
            # 读取时间戳
            timestamp_ns = struct.unpack('Q', f.read(8))[0]
            timestamp = datetime.fromtimestamp(timestamp_ns / 1e9)
            
            # 读取消息长度
            data_len = struct.unpack('Q', f.read(8))[0]
            
            # 读取消息数据
            msg_data = f.read(data_len)
        
        return msg_type, timestamp, msg_data
    
    def load_message_as_json(self, file_path):
        """从文件加载消息并解析为JSON
        
        Args:
            file_path: 消息文件路径
            
        Returns:
            tuple: (msg_type, timestamp, json_data)
        """
        msg_type, timestamp, msg_data = self.load_message(file_path)
        try:
            json_data = json.loads(msg_data.decode('utf-8'))
            return msg_type, timestamp, json_data
        except json.JSONDecodeError:
            return msg_type, timestamp, msg_data
    
    def list_records(self, msg_type=None, start_time=None, end_time=None):
        """列出记录的消息
        
        Args:
            msg_type: 消息类型（可选）
            start_time: 开始时间（可选）
            end_time: 结束时间（可选）
            
        Returns:
            list: 消息文件路径列表
        """
        records = []
        
        for root, _, files in os.walk(self.output_dir):
            for file in files:
                if file.endswith('.msg'):
                    file_path = os.path.join(root, file)
                    
                    # 解析文件名获取消息类型和时间
                    try:
                        parts = file.split('_')
                        if len(parts) < 2:
                            continue
                        
                        msg_type_file = parts[0]
                        time_str = '_'.join(parts[1:]).replace('.msg', '')
                        timestamp = datetime.strptime(time_str, '%H-%M-%S-%f')
                        
                        # 检查消息类型
                        if msg_type and msg_type != msg_type_file:
                            continue
                        
                        # 检查时间范围
                        if start_time and timestamp < start_time:
                            continue
                        if end_time and timestamp > end_time:
                            continue
                        
                        records.append(file_path)
                    except Exception:
                        continue
        
        return records

class MsgPlayer:
    """消息播放器（回灌工具）"""
    def __init__(self, callback=None):
        self.callback = callback
    
    def play_message(self, file_path, delay=0.0):
        """播放单个消息
        
        Args:
            file_path: 消息文件路径
            delay: 播放前延迟（秒）
        """
        if delay > 0:
            time.sleep(delay)
        
        recorder = MsgRecorder()
        msg_type, timestamp, msg_data = recorder.load_message(file_path)
        
        if self.callback:
            self.callback(msg_type, msg_data, timestamp)
        else:
            print(f"Playing message: {msg_type} at {timestamp}")
            print(f"Message data length: {len(msg_data)}")
    
    def play_messages(self, file_paths, rate=1.0):
        """播放多个消息
        
        Args:
            file_paths: 消息文件路径列表
            rate: 播放速率（1.0为原始速率）
        """
        if not file_paths:
            return
        
        # 按时间戳排序
        recorder = MsgRecorder()
        messages = []
        
        for file_path in file_paths:
            try:
                msg_type, timestamp, msg_data = recorder.load_message(file_path)
                messages.append((timestamp, file_path, msg_type, msg_data))
            except Exception as e:
                print(f"Error loading message {file_path}: {e}")
                continue
        
        if not messages:
            return
        
        # 按时间戳排序
        messages.sort(key=lambda x: x[0])
        
        # 播放消息
        prev_time = None
        for timestamp, file_path, msg_type, msg_data in messages:
            if prev_time:
                # 计算时间差并应用速率
                time_diff = (timestamp - prev_time).total_seconds()
                if time_diff > 0:
                    time.sleep(time_diff / rate)
            
            if self.callback:
                self.callback(msg_type, msg_data, timestamp)
            else:
                print(f"Playing message: {msg_type} at {timestamp}")
                print(f"Message data length: {len(msg_data)}")
            
            prev_time = timestamp
    
    def play_directory(self, directory, msg_type=None, start_time=None, end_time=None, rate=1.0):
        """播放目录中的消息
        
        Args:
            directory: 消息目录
            msg_type: 消息类型（可选）
            start_time: 开始时间（可选）
            end_time: 结束时间（可选）
            rate: 播放速率（1.0为原始速率）
        """
        recorder = MsgRecorder(directory)
        file_paths = recorder.list_records(msg_type, start_time, end_time)
        self.play_messages(file_paths, rate)

def main():
    parser = argparse.ArgumentParser(description='AuroraRT 消息落盘和回灌工具')
    subparsers = parser.add_subparsers(dest='command', help='子命令')
    
    # 记录消息命令
    record_parser = subparsers.add_parser('record', help='记录消息')
    record_parser.add_argument('--type', required=True, help='消息类型')
    record_parser.add_argument('--data', help='消息数据（JSON格式）')
    record_parser.add_argument('--file', help='从文件读取消息数据')
    record_parser.add_argument('--output', default='./records', help='输出目录')
    
    # 播放消息命令
    play_parser = subparsers.add_parser('play', help='播放消息')
    play_parser.add_argument('--file', help='消息文件路径')
    play_parser.add_argument('--directory', help='消息目录')
    play_parser.add_argument('--type', help='消息类型过滤')
    play_parser.add_argument('--rate', type=float, default=1.0, help='播放速率')
    
    # 列出消息命令
    list_parser = subparsers.add_parser('list', help='列出消息')
    list_parser.add_argument('--directory', default='./records', help='消息目录')
    list_parser.add_argument('--type', help='消息类型过滤')
    
    args = parser.parse_args()
    
    if args.command == 'record':
        recorder = MsgRecorder(args.output)
        
        if args.data:
            try:
                json_data = json.loads(args.data)
                file_path = recorder.record_message_from_json(args.type, json_data)
                print(f"Message recorded to: {file_path}")
            except json.JSONDecodeError:
                print("Error: Invalid JSON data")
                sys.exit(1)
        elif args.file:
            if not os.path.exists(args.file):
                print(f"Error: File not found: {args.file}")
                sys.exit(1)
            
            with open(args.file, 'rb') as f:
                msg_data = f.read()
            
            file_path = recorder.record_message(args.type, msg_data)
            print(f"Message recorded to: {file_path}")
        else:
            print("Error: Either --data or --file must be specified")
            sys.exit(1)
    
    elif args.command == 'play':
        player = MsgPlayer()
        
        if args.file:
            if not os.path.exists(args.file):
                print(f"Error: File not found: {args.file}")
                sys.exit(1)
            player.play_message(args.file)
        elif args.directory:
            if not os.path.exists(args.directory):
                print(f"Error: Directory not found: {args.directory}")
                sys.exit(1)
            player.play_directory(args.directory, args.type, rate=args.rate)
        else:
            print("Error: Either --file or --directory must be specified")
            sys.exit(1)
    
    elif args.command == 'list':
        recorder = MsgRecorder(args.directory)
        records = recorder.list_records(args.type)
        
        print(f"Found {len(records)} records:")
        for record in records:
            print(f"  - {record}")
    
    else:
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()
