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

def analyze_frequency_domain_with_lowpass_filter_timeseries_corrected(csv_file, acc_columns, sampling_frequency, cutoff_frequency=10.0, use_window=True):
    """
    分析 CSV 加速度数据的频域信息，应用低通滤波器 (修正后的滤波器设计)，并同时显示原始和滤波后的时域信号 (完整数据)。
    ... (参数说明与之前相同)
    """

    try:
        df = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"错误: 文件 '{csv_file}' 未找到.")
        return
    except Exception as e:
        print(f"读取 CSV 文件时发生错误: {e}")
        return

    if not all(col in df.columns for col in acc_columns):
        print(f"错误: CSV 文件中缺少列名: {acc_columns}")
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

        # --- 低通滤波实现 (修正后的滤波器设计) ---
        # 6. 设计理想低通滤波器
        filter_frequency_response = np.ones(N, dtype=complex) # 初始化滤波器频响为全 1
        for i in range(N):
            if np.abs(freq[i]) > cutoff_frequency: # 使用频率轴 freq 判断频率是否超过截止频率
                filter_frequency_response[i] = 0  # 如果频率超过截止频率，设置为 0

        # 7. 应用滤波器 (频域乘法)
        filtered_fft_result = fft_result * filter_frequency_response

        magnitude_spectrum_filtered = np.abs(filtered_fft_result) / N
        magnitude_spectrum_positive_filtered = magnitude_spectrum_filtered[positive_freq_indices]

        filtered_acc_data = np.fft.ifft(filtered_fft_result).real

        # --- 绘图 --- (绘图部分代码与之前相同，无需修改)
        plt.figure(figsize=(18, 12))

        plt.subplot(4, 2, 1)
        plt.plot(freq_positive, magnitude_spectrum_positive_original)
        plt.title(f'{acc_col} 轴 - 原始加速度频谱 (截止频率: {cutoff_frequency} Hz, 加窗: {use_window})')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值')
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2)
        plt.ylim(bottom=0)

        plt.subplot(4, 2, 2)
        plt.plot(freq_positive, magnitude_spectrum_positive_filtered)
        plt.title(f'{acc_col} 轴 - 滤波后加速度频谱 (低通, 截止频率: {cutoff_frequency} Hz)')
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

        time_filtered = np.arange(0, len(filtered_acc_data)) / sampling_frequency
        plt.subplot(4, 2, (5, 6))
        plt.plot(time_filtered, filtered_acc_data)
        plt.title(f'{acc_col} 轴 - 滤波后的时域加速度信号 (完整数据)')
        plt.xlabel('时间 (秒)')
        plt.ylabel('加速度值')
        plt.grid(True)

        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    csv_file_path = sys.argv[1] #'accelerometer_data.csv' # 替换为你的 CSV 文件路径
    accelerometer_columns = ['accel_x', 'accel_y', 'accel_z'] # 替换为你的加速度数据列名

    sampling_rate = 15.0
    hanning_window_enabled = True
    lowpass_cutoff_freq = 10.0 # 确保截止频率设置为 10Hz

    analyze_frequency_domain_with_lowpass_filter_timeseries_corrected(
        csv_file_path,
        accelerometer_columns,
        sampling_rate,
        cutoff_frequency=lowpass_cutoff_freq,
        use_window=hanning_window_enabled
    )