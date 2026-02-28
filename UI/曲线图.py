import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import serial
import serial.tools.list_ports
import threading
import re
import csv
import time
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.ticker import MultipleLocator
import sys
import os

# ================= 全局变量 =================
ser = None
running = False
start_time = None
connection_lost_time = None
reconnect_attempts = 0
MAX_RECONNECT_ATTEMPTS = 5
RECONNECT_DELAY = 2  # 秒

# 新增：数据保留时间
DATA_RETENTION_SECONDS = 3600  # 1*60*60

# 自动保存配置
AUTO_SAVE_ENABLED = True
AUTO_SAVE_INTERVAL = 30
last_auto_save_time = None
auto_save_dir = "serial_monitor_data"
auto_save_filepath = None
auto_save_thread = None

# 新增：线程锁（解决数据访问冲突）
data_lock = threading.Lock()

# 数据存储列表（保留最近3小时数据）
oxygen_time = []
oxygen_data = []
ph_time = []
ph_data = []
ph_setpoint_data = []
temp_time = []
temp_data = []
temp_setpoint_data = []
co2_time = []
co2_data = []
gas_o2_time = []
gas_o2_data = []
ch4_do_time = []
ch4_do_data = []
od_time = []
od_raw_data = []
od_filtered_data = []
current_od_baseline = None
last_od_raw_time = None

# 串口参数
current_port = None
current_baudrate = None

# 各参数纵坐标范围扩展值
RANGE_EXTENSIONS = {
    "oxygen": 1,
    "ph": 0.3,
    "temperature": 3,
    "co2": 300,
    "gas_o2": 1,
    "ch4_do": 1.5,
    "od": 0.2
}

plt.rcParams['font.sans-serif']=['SimHei']    # 用来正常显示中文标签
plt.rcParams['axes.unicode_minus'] = False    # 用来显示负号


# ================= 工具函数 =================
def rel_time_to_system_time(rel_time):
    """将相对时间（秒）转换为系统时间字符串"""
    if start_time is None:
        return ""
    # 计算绝对时间戳（开始时间 + 相对时间）
    absolute_timestamp = start_time + rel_time
    # 格式化为 "YYYY-MM-DD-HH-MM-SS"
    return time.strftime("%Y-%m-%d-%H-%M-%S", time.localtime(absolute_timestamp))


def create_directory(directory):
    if not os.path.exists(directory):
        try:
            os.makedirs(directory)
            print(f"📂 自动保存目录创建成功: {directory}")
            return True
        except Exception as e:
            print(f"❌ 自动保存目录创建失败: {e}")
            return False
    return True


def init_auto_save_single_file():
    global auto_save_filepath
    start_timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"serial_monitor_auto_save_{start_timestamp}.csv"
    auto_save_filepath = os.path.join(auto_save_dir, filename)

    if not os.path.exists(auto_save_filepath):
        try:
            with open(auto_save_filepath, "w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow([
                    "Time (s)", "Oxygen (%)", "pH (Measured)",
                    "pH (Setpoint)", "Temp (°C, PV)", "Temp (°C, SP)",
                    "CO₂ (ppm)", "Gas O₂ (%)", "CH4 DO (mg/L)",
                    "OD Raw Value", "OD Filtered Value"
                ])
            print(f"📄 自动保存文件初始化完成: {auto_save_filepath}")
            return True
        except Exception as e:
            print(f"❌ 自动保存文件初始化失败: {e}")
            auto_save_filepath = None
            return False
    return True


# 新增：清理过期数据（只保留最近3小时）
def clean_expired_data(current_rel_time):
    """清理超过保留时间的数据，确保内存占用稳定"""
    cutoff_time = current_rel_time - DATA_RETENTION_SECONDS
    if cutoff_time < 0:
        cutoff_time = 0

    data_pairs = [
        (oxygen_time, oxygen_data),
        (ph_time, ph_data),
        (ph_time, ph_setpoint_data),
        (temp_time, temp_data),
        (temp_time, temp_setpoint_data),
        (co2_time, co2_data),
        (gas_o2_time, gas_o2_data),
        (ch4_do_time, ch4_do_data),
        (od_time, od_raw_data),
        (od_time, od_filtered_data)
    ]

    with data_lock:  # 确保清理操作线程安全
        for time_list, data_list in data_pairs:
            # 找到第一个超过截止时间的索引
            cutoff_idx = 0
            while cutoff_idx < len(time_list) and time_list[cutoff_idx] < cutoff_time:
                cutoff_idx += 1
            # 删除过期数据
            if cutoff_idx > 0:
                del time_list[:cutoff_idx]
                del data_list[:cutoff_idx]


# ================= 串口操作模块 =================
def get_available_ports():
    return [port.device for port in serial.tools.list_ports.comports()]


def start_serial_monitor(port, baudrate):
    global ser, running, start_time, current_port, current_baudrate
    global reconnect_attempts, last_auto_save_time, auto_save_thread

    current_port = port
    current_baudrate = baudrate
    reconnect_attempts = 0

    try:
        if ser and ser.is_open:
            ser.close()
            ser = None

        ser = serial.Serial(port, baudrate, timeout=1)
        running = True

        if start_time is None:
            start_time = time.time()
            last_auto_save_time = start_time
            if AUTO_SAVE_ENABLED:
                create_directory(auto_save_dir)
                init_auto_save_single_file()

        if AUTO_SAVE_ENABLED and auto_save_filepath and (auto_save_thread is None or not auto_save_thread.is_alive()):
            auto_save_thread = threading.Thread(target=auto_save_loop, daemon=True)
            auto_save_thread.start()
            print(f"⏰ 自动保存线程启动（间隔：{AUTO_SAVE_INTERVAL}秒）")

        threading.Thread(target=read_serial, daemon=True).start()
        print(f"✅ 串口连接成功: {port}（波特率：{baudrate}）")
        return True

    except Exception as e:
        print(f"❌ 串口连接失败: {e}")
        return False


def stop_serial_monitor():
    global running, ser, auto_save_thread
    running = False

    if auto_save_thread and auto_save_thread.is_alive():
        auto_save_thread.join(timeout=3.0)
        auto_save_thread = None

    if ser:
        try:
            if ser.is_open:
                ser.close()
                print("🔌 串口已关闭")
            ser = None
        except Exception as e:
            print(f"⚠️ 关闭串口出错: {e}")

    if AUTO_SAVE_ENABLED and auto_save_filepath:
        save_data_to_csv(auto_save_filepath, is_auto_save=True, is_append=True)
        print(f"💡 最后数据已保存至: {auto_save_filepath}")


# ================= 数据保存模块 =================
def save_data_to_csv(filepath, is_auto_save=False, is_append=False, filter_new_data=True):
    with data_lock:  # 读取数据时加锁
        all_times = sorted(set(oxygen_time + ph_time + temp_time + co2_time +
                               gas_o2_time + ch4_do_time + od_time))
    if not all_times:
        if not is_auto_save:
            print("⚠️ 无数据可保存")
        return False

    if filter_new_data and is_auto_save and is_append:
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                reader = csv.reader(f)
                rows = list(reader)
                if len(rows) > 1:
                    last_saved_time = float(rows[-1][0])
                    all_times = [t for t in all_times if t > last_saved_time]
                    if not all_times:
                        return True
        except (FileNotFoundError, ValueError, IndexError):
            pass

    try:
        with open(filepath, "a" if is_append else "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            if not is_append:
                writer.writerow([
                    "Time (s)", "Oxygen (%)", "pH (Measured)",
                    "pH (Setpoint)", "Temp (°C, PV)", "Temp (°C, SP)",
                    "CO₂ (ppm)", "Gas O₂ (%)", "CH4 DO (mg/L)",
                    "OD Raw Value", "OD Filtered Value"
                ])
            for t in all_times:
                row = [rel_time_to_system_time(t)]

                # 读取数据时加锁（单个时间点查询）
                with data_lock:
                    # 氧气数据
                    if t in oxygen_time:
                        idx = oxygen_time.index(t)
                        row.append(round(oxygen_data[idx], 2) if oxygen_data[idx] is not None else "")
                    else:
                        row.append("")

                    # pH数据
                    if t in ph_time:
                        idx = ph_time.index(t)
                        row.append(round(ph_data[idx], 2) if ph_data[idx] is not None else "")
                        row.append(round(ph_setpoint_data[idx], 2) if ph_setpoint_data[idx] is not None else "")
                    else:
                        row.append("")
                        row.append("")

                    # 温度数据
                    if t in temp_time:
                        idx = temp_time.index(t)
                        row.append(round(temp_data[idx], 2) if temp_data[idx] is not None else "")
                        row.append(round(temp_setpoint_data[idx], 2) if temp_setpoint_data[idx] is not None else "")
                    else:
                        row.append("")
                        row.append("")

                    # CO2数据
                    if t in co2_time:
                        idx = co2_time.index(t)
                        row.append(round(co2_data[idx], 2) if co2_data[idx] is not None else "")
                    else:
                        row.append("")

                    # 气体O2数据
                    if t in gas_o2_time:
                        idx = gas_o2_time.index(t)
                        row.append(round(gas_o2_data[idx], 2) if gas_o2_data[idx] is not None else "")
                    else:
                        row.append("")

                    # CH4 DO数据
                    if t in ch4_do_time:
                        idx = ch4_do_time.index(t)
                        row.append(round(ch4_do_data[idx], 2) if ch4_do_data[idx] is not None else "")
                    else:
                        row.append("")

                    # OD数据
                    if t in od_time:
                        idx = od_time.index(t)
                        row.append(round(od_raw_data[idx], 4) if od_raw_data[idx] is not None else "")
                        row.append(round(od_filtered_data[idx], 4) if od_filtered_data[idx] is not None else "")
                    else:
                        row.append("")
                        row.append("")

                writer.writerow(row)

        log_prefix = "🔄 自动保存（追加）" if (is_auto_save and is_append) else "💾 手动保存"
        print(f"{log_prefix}成功: {filepath}（新增{len(all_times)}个时间点）")
        return True
    except Exception as e:
        log_prefix = "❌ 自动保存失败" if is_auto_save else "❌ 手动保存失败"
        print(f"{log_prefix}: {e}")
        return False


def auto_save_loop():
    global last_auto_save_time, running
    while running and auto_save_filepath:
        current_time = time.time()
        if current_time - last_auto_save_time >= AUTO_SAVE_INTERVAL:
            if save_data_to_csv(auto_save_filepath, is_auto_save=True, is_append=True):
                last_auto_save_time = current_time
        time.sleep(1)


# ================= 数据读取线程 =================
def read_serial():
    global start_time, current_od_baseline, last_od_raw_time
    while running and ser:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            current_time = time.time()
            if start_time is None:
                start_time = current_time
            current_rel_time = round(current_time - start_time, 3)

            # 定期清理过期数据（每10条数据清理一次，平衡性能）
            # if len(oxygen_time) % 10 == 0:
            #     clean_expired_data(current_rel_time)

            # OD基线计算阶段
            if "OD Baseline Calculated" in line:
                try:
                    baseline_match = re.search(r"baseline_value:\s*([\d.]+)", line)
                    if baseline_match:
                        with data_lock:
                            current_od_baseline = float(baseline_match.group(1))
                        print(f"📊 OD基线更新: {current_od_baseline:.6f} | 时间: {current_rel_time}s")
                except Exception as e:
                    print(f"⚠️ OD基线解析异常: {e} - 原始数据: {line}")
                continue

            # 原始OD值解析
            if "OD_value:" in line and "baseline_value:" in line and "filtered_OD_value:" not in line:
                try:
                    od_match = re.search(r"OD_value:\s*([\d.]+)", line)
                    baseline_match = re.search(r"baseline_value:\s*([\d.]+)", line)
                    if od_match and baseline_match:
                        od_val = float(od_match.group(1))
                        new_baseline = float(baseline_match.group(1))

                        with data_lock:
                            current_od_baseline = new_baseline
                            # 避免1秒内重复添加
                            if last_od_raw_time is None or current_rel_time - last_od_raw_time > 1.0:
                                od_raw_data.append(od_val)
                                od_filtered_data.append(None)
                                od_time.append(current_rel_time)
                                last_od_raw_time = current_rel_time
                        print(f"✅ OD原始值: {od_val:.6f} | 基线: {new_baseline:.2f} | 时间: {current_rel_time}s")
                except Exception as e:
                    print(f"⚠️ OD原始值解析异常: {e} - 原始数据: {line}")
                continue

            # 过滤后OD值解析
            if "filtered_OD_value:" in line and "baseline_value:" in line:
                try:
                    filtered_od_match = re.search(r"filtered_OD_value:\s*([\d.]+)", line)
                    baseline_match = re.search(r"baseline_value:\s*([\d.]+)", line)
                    if filtered_od_match and baseline_match:
                        filtered_od_val = float(filtered_od_match.group(1))
                        new_baseline = float(baseline_match.group(1))
                        matched = False

                        with data_lock:
                            current_od_baseline = new_baseline
                            # 匹配2秒内的原始值
                            if od_time:
                                for i in reversed(range(len(od_time))):
                                    if abs(od_time[i] - current_rel_time) < 2.0 and od_filtered_data[i] is None:
                                        od_filtered_data[i] = filtered_od_val
                                        matched = True
                                        break

                        if matched:
                            print(
                                f"✅ OD过滤值: {filtered_od_val:.6f} | 基线: {new_baseline:.2f} | 时间: {current_rel_time}s (匹配原始值)")
                        else:
                            with data_lock:
                                od_filtered_data.append(filtered_od_val)
                                od_raw_data.append(None)
                                od_time.append(current_rel_time)
                            print(f"⚠️ OD过滤值（无原始值）: {filtered_od_val:.6f} | 时间: {current_rel_time}s")
                except Exception as e:
                    print(f"⚠️ OD过滤值解析异常: {e} - 原始数据: {line}")
                continue

            # CH4 DO数据解析
            if "CH4:" in line and "DO=" in line:
                try:
                    do_match = re.search(r"DO=\s*([\d.]+)\s*mg/L", line)
                    if do_match:
                        do_val = float(do_match.group(1))
                        with data_lock:
                            ch4_do_data.append(do_val)
                            ch4_do_time.append(current_rel_time)
                        print(f"✅ CH4溶解氧: {do_val} mg/L | 时间: {current_rel_time}s")
                    else:
                        print(f"⚠️ CH4 DO数据解析失败: {line}")
                except Exception as e:
                    print(f"⚠️ CH4 DO数据解析异常: {e} - 原始数据: {line}")
                continue

            # 氧气
            if line.startswith("Oxygen="):
                try:
                    o_val = float(line.split("=")[1])
                    with data_lock:
                        oxygen_data.append(o_val)
                        oxygen_time.append(current_rel_time)
                    print(f"✅ 氧气: {o_val}% | 时间: {current_rel_time}s")
                except:
                    print(f"⚠️ 氧气数据解析失败: {line}")
                continue

            # 气体O2数据解析
            if "gas_O₂" in line:
                try:
                    o2_match = re.search(r"gas_O₂\s*:\s*([\d.]+)\s*%", line)
                    if o2_match:
                        o2_val = float(o2_match.group(1))
                        with data_lock:
                            gas_o2_data.append(o2_val)
                            gas_o2_time.append(current_rel_time)
                        print(f"✅ 气体O₂: {o2_val}% | 时间: {current_rel_time}s")
                    else:
                        print(f"⚠️ 气体O₂数据解析失败: {line}")
                except Exception as e:
                    print(f"⚠️ 气体O₂数据解析异常: {e} - 原始数据: {line}")
                continue

            # pH
            if line.startswith("Get_pH:"):
                try:
                    ph_match = re.search(r"Get_pH:\s*([\d.]+)", line)
                    sp_match = re.search(r"SetPoint:\s*([\d.]+)", line)
                    ph_val = float(ph_match.group(1)) if ph_match else None
                    ph_sp_val = float(sp_match.group(1)) if sp_match else None

                    with data_lock:
                        ph_data.append(ph_val)
                        ph_setpoint_data.append(ph_sp_val)
                        ph_time.append(current_rel_time)
                    print(f"✅ pH: {ph_val} (设定值: {ph_sp_val}) | 时间: {current_rel_time}s")
                except:
                    print(f"⚠️ pH数据解析失败: {line}")
                continue

            # 温度
            if any(prefix in line for prefix in ["[PID]", "[DEBUG]", "[AutoTune]", "Temp=", "温度="]):
                try:
                    temp_val = None
                    temp_sp_val = None

                    if "[PID]" in line or "[DEBUG]" in line:
                        match = re.search(r"SP=([\d.]+),?\s*PV=([\d.]+)", line)
                        if match:
                            temp_sp_val = float(match.group(1))
                            temp_val = float(match.group(2))

                    elif "[AutoTune]" in line:
                        match = re.search(r"Temp=([\d.]+)", line)
                        if match:
                            temp_val = float(match.group(1))
                            temp_sp_val = 37.0

                    elif "Temp=" in line:
                        match = re.search(r"Temp=([\d.]+)", line)
                        if match:
                            temp_val = float(match.group(1))

                    elif "温度=" in line:
                        match = re.search(r"温度=([\d.]+)", line)
                        if match:
                            temp_val = float(match.group(1))

                    if temp_val is not None or temp_sp_val is not None:
                        with data_lock:
                            temp_data.append(temp_val)
                            temp_setpoint_data.append(temp_sp_val)
                            temp_time.append(current_rel_time)
                        print(f"✅ 温度: 测量值={temp_val}℃ (设定值={temp_sp_val}℃) | 时间: {current_rel_time}s")
                except:
                    print(f"⚠️ 温度数据解析失败: {line}")
                continue

            # CO₂
            if line.startswith("CO2=") or "gas_CO₂:" in line:
                try:
                    co2_val = None
                    if line.startswith("CO2="):
                        co2_val = float(line.split("=")[1].strip())
                    else:
                        match = re.search(r"gas_CO₂:\s*(\d+)\s*ppm", line)
                        co2_val = float(match.group(1)) if match else None
                    if co2_val is not None:
                        with data_lock:
                            co2_data.append(co2_val)
                            co2_time.append(current_rel_time)
                        print(f"✅ CO₂: {co2_val} ppm | 时间: {current_rel_time}s")
                except:
                    print(f"⚠️ CO₂数据解析失败: {line}")
                continue

            print(f"📜 MCU日志: {line} | 时间: {current_rel_time}s")

        except Exception as e:
            print(f"⚠️ 串口读取异常: {e}")
            time.sleep(0.1)


# ================= 图表绘制模块 =================
def update_plot(frame, lines, axs, od_baseline_text):
    def safe_set_data(line, t_list, d_list):
        with data_lock:
            min_len = min(len(t_list), len(d_list))
            valid_time = [t_list[i] for i in range(min_len) if d_list[i] is not None]
            valid_data = [d_list[i] for i in range(min_len) if d_list[i] is not None]
        if valid_time:
            line.set_data(valid_time, valid_data)

    def update_ax_limits(ax, param_type, data_lists):
        # 收集有效数据
        all_valid_data = []
        with data_lock:
            for d_list in data_lists:
                all_valid_data.extend([d for d in d_list if d is not None])

        if not all_valid_data:
            defaults = {
                "oxygen": (0, 12.5),
                "ph": (6, 8),
                "temperature": (30, 40),
                "co2": (600, 800),
                "gas_o2": (19, 22),
                "ch4_do": (0, 15),
                "od": (0, 1)
            }
            ax.set_ylim(defaults[param_type])
        else:
            data_min = min(all_valid_data)
            data_max = max(all_valid_data)
            extension = RANGE_EXTENSIONS.get(param_type, 0)
            ax.set_ylim(data_min - extension, data_max + extension)

        # 处理X轴（只显示最近3小时数据，减少刻度数量）
        all_valid_time = []
        with data_lock:
            time_list_mapping = [
                (oxygen_data, oxygen_time),
                (ph_data, ph_time),
                (ph_setpoint_data, ph_time),
                (temp_data, temp_time),
                (temp_setpoint_data, temp_time),
                (co2_data, co2_time),
                (gas_o2_data, gas_o2_time),
                (ch4_do_data, ch4_do_time),
                (od_raw_data, od_time),
                (od_filtered_data, od_time)
            ]

            for d_list, t_list in time_list_mapping:
                if d_list in data_lists:
                    min_len = min(len(t_list), len(d_list))
                    all_valid_time.extend([t_list[i] for i in range(min_len) if d_list[i] is not None])

        if all_valid_time:
            max_time = max(all_valid_time)
            # 计算3小时前的时间点
            min_time = max(0, max_time - DATA_RETENTION_SECONDS)
            ax.set_xlim(min_time, max_time + 30)  # 右侧留30秒缓冲

            # 🔥 关键修改：增大刻度间隔，减少刻度数量
            time_range = max_time - min_time  # 当前显示的时间范围（秒）
            if time_range <= 600:  # 10分钟内 → 每2分钟1个刻度（120秒）
                ax.xaxis.set_major_locator(MultipleLocator(120))
            elif time_range <= 1800:  # 30分钟内 → 每5分钟1个刻度（300秒）
                ax.xaxis.set_major_locator(MultipleLocator(300))
            elif time_range <= 3600:  # 1小时内 → 每10分钟1个刻度（600秒）
                ax.xaxis.set_major_locator(MultipleLocator(600))
            elif time_range <= 7200:  # 2小时内 → 每15分钟1个刻度（900秒）
                ax.xaxis.set_major_locator(MultipleLocator(900))
            else:  # 2-3小时 → 每30分钟1个刻度（1800秒），3小时最多仅6个刻度
                ax.xaxis.set_major_locator(MultipleLocator(1800))
        else:
            ax.set_xlim(0, 30)
            ax.xaxis.set_major_locator(MultipleLocator(10))  # 初始无数据时，保持少量刻度

        ax.relim()
        ax.autoscale_view(True, True, True)

    # 子图配置
    ax_config = [
        (0, "oxygen", [oxygen_data]),
        (1, "ph", [ph_data, ph_setpoint_data]),
        (2, "temperature", [temp_data, temp_setpoint_data]),
        (3, "co2", [co2_data]),
        (4, "gas_o2", [gas_o2_data]),
        (5, "ch4_do", [ch4_do_data]),
        (6, "od", [od_raw_data, od_filtered_data])
    ]

    # 更新所有曲线数据
    for i, line in enumerate(lines):
        line_data = [
            (oxygen_time, oxygen_data),
            (ph_time, ph_data),
            (ph_time, ph_setpoint_data),
            (temp_time, temp_data),
            (temp_time, temp_setpoint_data),
            (co2_time, co2_data),
            (gas_o2_time, gas_o2_data),
            (ch4_do_time, ch4_do_data),
            (od_time, od_raw_data),
            (od_time, od_filtered_data)
        ][i]
        safe_set_data(line, line_data[0], line_data[1])

    # 更新每个子图的坐标轴范围
    for ax_idx, param_type, data_lists in ax_config:
        update_ax_limits(axs[ax_idx], param_type, data_lists)

    # 更新OD基线文本
    with data_lock:
        current_baseline = current_od_baseline
    if current_baseline is not None:
        od_baseline_text.set_text(f"Baseline: {current_baseline:.2f}")
    else:
        od_baseline_text.set_text("Baseline: --")

    return lines + [od_baseline_text]


def create_plots():
    fig, axs = plt.subplots(7, 1, figsize=(12, 21), sharex=False)
    fig.subplots_adjust(hspace=0.4, right=0.85)
    fig.suptitle("发酵数据监控", fontsize=14, fontweight='bold')

    # 子图1：氧气监控
    axs[0].set_title("Oxygen", fontsize=12, fontweight='bold')
    axs[0].set_ylabel("Oxygen(%)", fontsize=10)
    axs[0].grid(True, alpha=0.3)
    oxygen_line, = axs[0].plot([], [], "b-", linewidth=2, label="Oxygen")
    axs[0].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图2：pH监控
    axs[1].set_title("pH", fontsize=12, fontweight='bold')
    axs[1].set_ylabel("pH Value", fontsize=10)
    axs[1].grid(True, alpha=0.3)
    ph_line, = axs[1].plot([], [], "g-", linewidth=2, label="Measured pH")
    ph_sp_line, = axs[1].plot([], [], "g--", linewidth=2, label="pH Setpoint")
    axs[1].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图3：温度监控
    axs[2].set_title("Temperature", fontsize=12, fontweight='bold')
    axs[2].set_ylabel("Temp(°C)", fontsize=10)
    axs[2].grid(True, alpha=0.3)
    temp_line, = axs[2].plot([], [], "r-", linewidth=2, label="Temp(Measured)")
    temp_sp_line, = axs[2].plot([], [], "r--", linewidth=2, label="Temp(Setpoint)")
    axs[2].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图4：CO2监控
    axs[3].set_title("CO₂", fontsize=12, fontweight='bold')
    axs[3].set_ylabel("CO₂(ppm)", fontsize=10)
    axs[3].grid(True, alpha=0.3)
    co2_line, = axs[3].plot([], [], "m-", linewidth=2, label="CO₂")
    axs[3].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图5：气体O2监控
    axs[4].set_title("Endgas-O₂", fontsize=12, fontweight='bold')
    axs[4].set_ylabel("Endgas-O₂(%)", fontsize=10)
    axs[4].grid(True, alpha=0.3)
    gas_o2_line, = axs[4].plot([], [], "y-", linewidth=2, label="Endgas-O₂")
    axs[4].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图6：CH4溶解氧监控
    axs[5].set_title("CH4-DO", fontsize=12, fontweight='bold')
    axs[5].set_ylabel("DO (mg/L)", fontsize=10)
    axs[5].grid(True, alpha=0.3)
    ch4_do_line, = axs[5].plot([], [], "c-", linewidth=2, label="CH4 DO")
    axs[5].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 子图7 - OD监控
    axs[6].set_title("OD ", fontsize=12, fontweight='bold')
    axs[6].set_xlabel("Time (seconds)", fontsize=10)
    axs[6].set_ylabel("OD Value", fontsize=10)
    axs[6].grid(True, alpha=0.3)
    od_raw_line, = axs[6].plot([], [], "b-", linewidth=2, label="OD Value")
    od_filtered_line, = axs[6].plot([], [], "r-", linewidth=2, label="Filtered OD")
    axs[6].legend(loc="center left", bbox_to_anchor=(1.02, 0.5), fontsize=9)

    # 添加右上角基线文本
    od_baseline_text = axs[6].text(
        0.98, 0.95, "Baseline: --",
        transform=axs[6].transAxes,
        ha="right", va="top",
        fontsize=10, fontweight="bold",
        bbox=dict(boxstyle="round", facecolor="white", alpha=0.8)
    )

    # 所有曲线列表
    lines = [
        oxygen_line, ph_line, ph_sp_line, temp_line, temp_sp_line,
        co2_line, gas_o2_line, ch4_do_line, od_raw_line, od_filtered_line
    ]

    ani = FuncAnimation(
        fig, update_plot,
        fargs=(lines, axs, od_baseline_text),
        interval=1000,
        blit=False,
        cache_frame_data=False
    )

    def on_close(event):
        # 修复Tkinter多实例问题
        root = tk.Toplevel()
        root.withdraw()
        save_confirmation = messagebox.askyesnocancel(
            "确认退出",
            "退出前是否保存当前监控数据？\n（选择\"取消\"将不保存并退出）",
            icon="question"
        )

        if save_confirmation is None:
            root.destroy()
            stop_serial_monitor()
            plt.close("all")
            sys.exit(0)
        elif save_confirmation:
            filepath = filedialog.asksaveasfilename(
                defaultextension=".csv",
                filetypes=[("CSV文件", "*.csv"), ("所有文件", "*.*")],
                title="保存监控数据",
                initialfile=f"serial_data_{time.strftime('%Y%m%d_%H%M%S')}.csv"
            )
            if filepath:
                save_data_to_csv(filepath, is_auto_save=False, is_append=False)
        else:
            pass

        stop_serial_monitor()
        root.destroy()
        plt.close("all")
        sys.exit(0)

    fig.canvas.mpl_connect("close_event", on_close)
    plt.show()


# ================= 主界面 =================
def main():
    root = tk.Tk()
    root.title("Serial Monitor - 串口配置")
    root.geometry("420x220")
    root.resizable(False, False)

    # 串口选择
    ttk.Label(root, text="可用串口：", font=("Arial", 10)).grid(row=0, column=0, padx=10, pady=25, sticky=tk.W)
    ports = get_available_ports()
    port_var = tk.StringVar(value=ports[0] if ports else "")
    port_combo = ttk.Combobox(
        root,
        textvariable=port_var,
        values=ports,
        width=18,
        state="readonly"
    )
    port_combo.grid(row=0, column=1, padx=10, pady=25)

    def refresh_ports():
        new_ports = get_available_ports()
        port_combo.config(values=new_ports)
        if new_ports:
            port_var.set(new_ports[0])

    ttk.Button(root, text="刷新串口", command=refresh_ports).grid(row=0, column=2, padx=10, pady=25)

    # 波特率选择
    ttk.Label(root, text="波特率：", font=("Arial", 10)).grid(row=1, column=0, padx=10, pady=5, sticky=tk.W)
    baud_var = tk.StringVar(value="115200")
    baud_combo = ttk.Combobox(
        root,
        textvariable=baud_var,
        values=["9600", "19200", "38400", "57600", "115200", "230400"],
        width=15,
        state="readonly"
    )
    baud_combo.grid(row=1, column=1, padx=10, pady=5)

    # 开始监控按钮
    def start_monitoring():
        selected_port = port_var.get()
        selected_baud = baud_var.get()
        if not selected_port:
            messagebox.showerror("错误", "请选择可用的串口！")
            return
        if start_serial_monitor(selected_port, int(selected_baud)):
            root.destroy()
            create_plots()
        else:
            messagebox.showerror(
                "连接失败",
                f"无法连接到串口 {selected_port}\n请检查：\n1. 设备是否正确连接\n2. 串口是否被其他程序占用\n3. 波特率是否匹配"
            )

    ttk.Button(
        root,
        text="启动实时监控",
        command=start_monitoring,
        style="Accent.TButton"
    ).grid(row=2, column=0, columnspan=3, pady=20, padx=60, sticky=tk.EW)

    # 界面样式
    style = ttk.Style()
    style.configure("Accent.TButton", font=("Arial", 10, "bold"), padding=8, background="#4CAF50")

    root.mainloop()


# ================= 程序入口 =================
if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n🔌 程序被手动中断")
        stop_serial_monitor()
        sys.exit(0)
    except Exception as e:
        print(f"❌ 程序异常退出: {e}")
        stop_serial_monitor()
        sys.exit(1)