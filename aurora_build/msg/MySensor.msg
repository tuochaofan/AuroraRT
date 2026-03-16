# MySensor.msg
# 传感器数据消息

float32 temperature
float32 humidity
int32 pressure
bool is_active
string sensor_id

# 容器类型
float32[] readings
string[] tags
