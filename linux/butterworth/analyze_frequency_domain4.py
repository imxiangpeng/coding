#!/bin/env python3

# -*- coding: utf-8 -*-


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

# 设置中文字体 (可选)
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei']   # 设置全局字体为 SimHei
plt.rcParams['axes.unicode_minus'] = False  # 解决负号显示为方块的问题 (可选)
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import butter, lfilter

def butter_lowpass_filter(data, cutoff_freq, sampling_rate, order=4):
    """
    设计并应用巴特沃斯低通滤波器。

    参数:
    data (array-like):  要滤波的数据。
    cutoff_freq (float):  截止频率 (Hz)。
    sampling_rate (float): 采样频率 (Hz)。
    order (int): 滤波器阶数，默认为 4。

    返回:
    array-like: 滤波后的数据。
    """
    nyquist_freq = 0.5 * sampling_rate  # 奈奎斯特频率
    normalized_cutoff = cutoff_freq / nyquist_freq  # 归一化截止频率
    b, a = butter(order, normalized_cutoff, btype='low', analog=False)  # 设计巴特沃斯滤波器
    y = lfilter(b, a, data)  # 应用滤波器
    return y

def analyze_frequency_domain_with_butterworth_lowpass(csv_file, acc_columns, time_column_name='time', cutoff_frequency=10.0, use_window=True, butterworth_order=4):
    """
    分析 CSV 加速度数据的频域信息，应用巴特沃斯低通滤波器，并同时显示原始和滤波后的结果。
    自动从 'time' 列计算采样频率。

    参数:
    csv_file (str): CSV 文件路径。
    acc_columns (list of str): 包含加速度数据的列名列表，例如 ['acc_x', 'acc_y', 'acc_z']。
    time_column_name (str): CSV 文件中时间戳列的列名，默认为 'time'。
    cutoff_frequency (float): 低通滤波器的截止频率，单位 Hz，默认为 10.0 Hz。
    use_window (bool): 是否使用汉宁窗，默认为 True。
    butterworth_order (int): 巴特沃斯滤波器阶数，默认为 4。

    返回:
    None (显示图形)
    """
    df = None  # 初始化 df 为 None

    try:
        df = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"错误: 文件 '{csv_file}' 未找到.")
        return
    except Exception as e:
        print(f"读取 CSV 文件时发生错误: {e}")
        return

    # 添加检查: 如果 df 为 None，说明读取 CSV 失败，直接返回
    if df is None:
        return

    if not all(col in df.columns for col in acc_columns):
        print(f"错误: CSV 文件中缺少加速度数据列名: {acc_columns}")
        return
    if time_column_name not in df.columns:
        print(f"错误: CSV 文件中缺少时间戳列 '{time_column_name}'. 无法自动计算采样频率，请确保CSV文件包含时间戳列。")
        return

    time_data = df[time_column_name]
    time_diffs = time_data.diff().dropna()
    sampling_frequency = None  # 初始化 sampling_frequency
    if not time_diffs.empty:
        average_time_interval = time_diffs.mean()
        if average_time_interval > 0:
            sampling_frequency = 1 / average_time_interval
            print(f"自动计算得到的采样频率: {sampling_frequency:.2f} Hz")
        else:
            print("时间间隔为零或负数，无法计算采样频率。请检查时间戳数据。")
            return
    else:
        print("时间戳数据不足以计算时间差。无法自动计算采样频率。")
        return

    if sampling_frequency is None: # 如果采样频率计算失败，则不继续执行
        return

    for acc_col in acc_columns:
        acc_data = df[acc_col].dropna().values

        if not acc_data.size:
            print(f"警告: 列 '{acc_col}' 数据为空，跳过频域分析和滤波。")
            continue

        acc_data_centered = acc_data - np.mean(acc_data)
        acc_data_original_timeseries = acc_data_centered.copy()

        if use_window:
            window = np.hanning(len(acc_data_centered))
            acc_data_windowed = acc_data_centered * window
        else:
            acc_data_windowed = acc_data_centered

        N = len(acc_data_windowed)
        fft_result = np.fft.fft(acc_data_windowed)
        magnitude_spectrum_original = np.abs(fft_result) / N
        freq = np.fft.fftfreq(N, 1/sampling_frequency)
        positive_freq_indices = np.where(freq >= 0)
        freq_positive = freq[positive_freq_indices]
        magnitude_spectrum_positive_original = magnitude_spectrum_original[positive_freq_indices]

        # --- 巴特沃斯低通滤波 ---
        filtered_acc_data_butterworth = butter_lowpass_filter(acc_data_centered, cutoff_frequency, sampling_frequency, order=butterworth_order)

        filtered_fft_result_butterworth = np.fft.fft(filtered_acc_data_butterworth)
        magnitude_spectrum_filtered_butterworth = np.abs(filtered_fft_result_butterworth) / N
        magnitude_spectrum_positive_filtered_butterworth = magnitude_spectrum_filtered_butterworth[positive_freq_indices]

        # --- 绘图 ---
        plt.figure(figsize=(18, 12))

        plt.subplot(4, 2, 1)
        plt.plot(freq_positive, magnitude_spectrum_positive_original)
        plt.title(f'{acc_col} 轴 - 原始加速度频谱 (截止频率: {cutoff_frequency} Hz, 加窗: {use_window}, 采样率: {sampling_frequency:.2f}Hz)')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值')
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2)
        plt.ylim(bottom=0)

        plt.subplot(4, 2, 2)
        plt.plot(freq_positive, magnitude_spectrum_positive_filtered_butterworth)
        plt.title(f'{acc_col} 轴 - 巴特沃斯滤波后频谱 (低通, 截止频率: {cutoff_frequency} Hz, 阶数: {butterworth_order}, 采样率: {sampling_frequency:.2f}Hz)')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值')
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2)
        plt.ylim(bottom=0)

        time = np.arange(0, len(acc_data_original_timeseries)) / sampling_frequency
        plt.subplot(4, 2, (3, 4))
        plt.plot(time, acc_data_original_timeseries)
        plt.title(f'{acc_col} 轴 - 原始时域加速度信号 (完整数据)')
        plt.xlabel('时间 (秒)')
        plt.ylabel('加速度值')
        plt.grid(True)

        time_filtered_butterworth = np.arange(0, len(filtered_acc_data_butterworth)) / sampling_frequency
        plt.subplot(4, 2, (5, 6))
        plt.plot(time_filtered_butterworth, filtered_acc_data_butterworth)
        plt.title(f'{acc_col} 轴 - 巴特沃斯滤波后时域加速度信号 (完整数据, 阶数: {butterworth_order})')
        plt.xlabel('时间 (秒)')
        plt.ylabel('加速度值')
        plt.grid(True)

        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    csv_file_path = sys.argv[1] #'accelerometer_data.csv'  # 替换为你的 CSV 文件路径
    #accelerometer_columns = ['accel_x', 'accel_y', 'accel_z', 'ag']  # 替换为你的加速度数据列名
    accelerometer_columns = ['accel_z', 'ag']  # 替换为你的加速度数据列名
    time_column = 'time'  # 替换为你的时间戳列名，如果不是 'time'
    hanning_window_enabled = True
    lowpass_cutoff_freq = 2.0 #1.0
    butterworth_filter_order = 2

    analyze_frequency_domain_with_butterworth_lowpass(
        csv_file_path,
        accelerometer_columns,
        time_column_name=time_column,
        cutoff_frequency=lowpass_cutoff_freq,
        use_window=hanning_window_enabled,
        butterworth_order=butterworth_filter_order
    )
