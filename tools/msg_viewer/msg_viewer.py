#!/usr/bin/env python3

"""
AuroraRT 消息可视化工具

用于查看消息文件内容和实时消息流
"""

import os
import sys
import argparse
import json
import struct
from datetime import datetime

class MsgViewer:
    """消息查看器"""
    def __init__(self):
        pass
    
    def view_file(self, file_path):
        """查看消息文件
        
        Args:
            file_path: 消息文件路径
        """
        if not os.path.exists(file_path):
            print(f"Error: File not found: {file_path}")
            return False
        
        try:
            # 尝试以不同格式读取
            if file_path.endswith('.msg'):
                self._view_msg_file(file_path)
            elif file_path.endswith('.cdr'):
                self._view_cdr_file(file_path)
            elif file_path.endswith('.json'):
                self._view_json_file(file_path)
            else:
                # 尝试自动检测格式
                self._view_auto_file(file_path)
            return True
        except Exception as e:
            print(f"Error viewing file: {e}")
            return False
    
    def _view_msg_file(self, file_path):
        """查看 .msg 文件"""
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
        
        print(f"Message Type: {msg_type}")
        print(f"Timestamp: {timestamp}")
        print(f"Data Length: {data_len} bytes")
        print("Data:")
        
        # 尝试解析为JSON
        try:
            json_data = json.loads(msg_data.decode('utf-8'))
            print(json.dumps(json_data, indent=2, ensure_ascii=False))
        except json.JSONDecodeError:
            # 显示原始字节
            print(f"Raw bytes: {msg_data.hex()}")
    
    def _view_cdr_file(self, file_path):
        """查看 .cdr 文件"""
        with open(file_path, 'rb') as f:
            data = f.read()
        
        print(f"CDR File: {file_path}")
        print(f"File Size: {len(data)} bytes")
        print("Raw bytes:")
        print(data.hex())
    
    def _view_json_file(self, file_path):
        """查看 .json 文件"""
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        print(f"JSON File: {file_path}")
        print("Content:")
        print(json.dumps(data, indent=2, ensure_ascii=False))
    
    def _view_auto_file(self, file_path):
        """自动检测并查看文件"""
        with open(file_path, 'rb') as f:
            data = f.read()
        
        # 尝试解析为JSON
        try:
            json_data = json.loads(data.decode('utf-8'))
            print(f"File: {file_path}")
            print("Detected as JSON:")
            print(json.dumps(json_data, indent=2, ensure_ascii=False))
            return
        except json.JSONDecodeError:
            pass
        
        # 尝试解析为.msg格式
        try:
            if len(data) >= 24:
                type_len = struct.unpack('Q', data[:8])[0]
                if type_len > 0 and len(data) >= 8 + type_len + 16:
                    msg_type = data[8:8+type_len].decode('utf-8')
                    timestamp_ns = struct.unpack('Q', data[8+type_len:8+type_len+8])[0]
                    timestamp = datetime.fromtimestamp(timestamp_ns / 1e9)
                    data_len = struct.unpack('Q', data[8+type_len+8:8+type_len+16])[0]
                    
                    print(f"File: {file_path}")
                    print(f"Detected as AuroraRT message:")
                    print(f"Message Type: {msg_type}")
                    print(f"Timestamp: {timestamp}")
                    print(f"Data Length: {data_len} bytes")
                    print("Data:")
                    msg_data = data[8+type_len+16:]
                    print(msg_data.hex())
                    return
        except Exception:
            pass
        
        # 显示原始字节
        print(f"File: {file_path}")
        print("Detected as raw data:")
        print(f"File Size: {len(data)} bytes")
        print("Raw bytes:")
        print(data.hex())
    
    def view_directory(self, directory, msg_type=None):
        """查看目录中的消息文件
        
        Args:
            directory: 目录路径
            msg_type: 消息类型过滤
        """
        if not os.path.exists(directory):
            print(f"Error: Directory not found: {directory}")
            return False
        
        files = []
        for root, _, filenames in os.walk(directory):
            for filename in filenames:
                if filename.endswith(('.msg', '.cdr', '.json')):
                    files.append(os.path.join(root, filename))
        
        if not files:
            print(f"No message files found in {directory}")
            return False
        
        print(f"Found {len(files)} message files in {directory}:")
        for file_path in files:
            print(f"\n=== {file_path} ===")
            self.view_file(file_path)
        
        return True
    
    def view_topic(self, topic, timeout=60):
        """查看实时消息流
        
        Args:
            topic: 话题名称
            timeout: 超时时间（秒）
        """
        print(f"Viewing real-time messages on topic: {topic}")
        print(f"Press Ctrl+C to exit")
        
        # 尝试与AuroraRT运行时集成
        try:
            # 尝试导入AuroraRT的发布订阅系统
            try:
                from aurorart import Node, Subscription
                
                # 创建一个临时节点
                node = Node(f"msg_viewer_{topic}")
                
                # 定义消息回调函数
                def callback(msg):
                    print(f"[{datetime.now()}] Received message on topic {topic}")
                    try:
                        # 尝试将消息转换为JSON
                        if hasattr(msg, 'to_json'):
                            json_data = msg.to_json()
                            print(json.dumps(json_data, indent=2, ensure_ascii=False))
                        else:
                            # 尝试直接打印消息
                            print(f"  Message: {msg}")
                    except Exception as e:
                        print(f"  Error parsing message: {e}")
                
                # 创建订阅
                subscription = node.create_subscription(topic, callback)
                
                # 运行指定时间
                import time
                start_time = time.time()
                while time.time() - start_time < timeout:
                    time.sleep(0.1)
                
                # 清理资源
                node.shutdown()
                
            except ImportError:
                # 如果无法导入AuroraRT，使用模拟实现
                print("AuroraRT runtime not found, using simulated messages")
                import time
                start_time = time.time()
                while time.time() - start_time < timeout:
                    # 模拟接收消息
                    print(f"[{datetime.now()}] Received message on topic {topic}")
                    print("  Message: {\"id\": 1, \"name\": \"test\", \"value\": 3.14}")
                    time.sleep(1)
        except KeyboardInterrupt:
            print("\nStopped viewing topic")
        except Exception as e:
            print(f"Error viewing topic: {e}")

def main():
    parser = argparse.ArgumentParser(description='AuroraRT 消息可视化工具')
    subparsers = parser.add_subparsers(dest='command', help='子命令')
    
    # 查看文件命令
    file_parser = subparsers.add_parser('file', help='查看消息文件')
    file_parser.add_argument('file_path', help='消息文件路径')
    
    # 查看目录命令
    dir_parser = subparsers.add_parser('dir', help='查看目录中的消息文件')
    dir_parser.add_argument('directory', help='目录路径')
    dir_parser.add_argument('--type', help='消息类型过滤')
    
    # 查看话题命令
    topic_parser = subparsers.add_parser('topic', help='查看实时消息流')
    topic_parser.add_argument('topic', help='话题名称')
    topic_parser.add_argument('--timeout', type=int, default=60, help='超时时间（秒）')
    
    args = parser.parse_args()
    
    viewer = MsgViewer()
    
    if args.command == 'file':
        viewer.view_file(args.file_path)
    elif args.command == 'dir':
        viewer.view_directory(args.directory, args.type)
    elif args.command == 'topic':
        viewer.view_topic(args.topic, args.timeout)
    else:
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()
