#!/bin/env python3

# -*- coding: utf-8 -*-


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

# 设置中文字体 (可选)
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei']   # 设置全局字体为 SimHei
plt.rcParams['axes.unicode_minus'] = False  # 解决负号显示为方块的问题 (可选)


def analyze_frequency_domain_with_lowpass_filter(csv_file, acc_columns, sampling_frequency, cutoff_frequency=10.0, use_window=True):
    """
    分析 CSV 加速度数据的频域信息，并应用低通滤波器。

    参数:
    csv_file (str): CSV 文件路径。
    acc_columns (list of str): 包含加速度数据的列名列表，例如 ['acc_x', 'acc_y', 'acc_z']。
    sampling_frequency (float): 采样频率，单位 Hz。
    cutoff_frequency (float): 低通滤波器的截止频率，单位 Hz，默认为 10.0 Hz。
    use_window (bool): 是否使用汉宁窗，默认为 True。

    返回:
    None (显示原始频谱、滤波后频谱和滤波后的时域信号)
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
        acc_data = df[acc_col].dropna().values  # 获取加速度数据，去除 NaN 值并转换为 NumPy 数组

        if not acc_data.size:
            print(f"警告: 列 '{acc_col}' 数据为空，跳过频域分析和滤波。")
            continue

        # 1. 数据预处理 - 去除均值 (DC 成分)
        acc_data_centered = acc_data - np.mean(acc_data)

        # 2. 数据预处理 - 窗口函数 (汉宁窗) - 可选
        if use_window:
            window = np.hanning(len(acc_data_centered))
            acc_data_windowed = acc_data_centered * window
        else:
            acc_data_windowed = acc_data_centered

        # 3. 执行 FFT
        N = len(acc_data_windowed)
        fft_result = np.fft.fft(acc_data_windowed)

        # 4. 计算频谱幅值 (Magnitude Spectrum) - 原始频谱
        magnitude_spectrum_original = np.abs(fft_result) / N

        # 5. 频率轴计算
        freq = np.fft.fftfreq(N, 1/sampling_frequency)

        # 只取正频率部分 (用于绘图原始频谱)
        positive_freq_indices = np.where(freq >= 0)
        freq_positive = freq[positive_freq_indices]
        magnitude_spectrum_positive_original = magnitude_spectrum_original[positive_freq_indices]

        # --- 低通滤波实现 ---
        # 6. 设计理想低通滤波器
        filter_frequency_response = np.ones(N, dtype=complex) # 初始化滤波器频响为全 1 (允许所有频率通过)
        cutoff_index = int(cutoff_frequency * N / sampling_frequency) # 计算截止频率对应的索引
        filter_frequency_response[cutoff_index+1:N-cutoff_index] = 0 # 将截止频率以上的频响设置为 0 (阻止高频通过)
        # 注意：由于FFT结果的对称性，需要将正负频率部分都设置为0. 对于实信号，频谱是对称的，正频率和负频率部分包含相同的信息。

        # 7. 应用滤波器 (频域乘法)
        filtered_fft_result = fft_result * filter_frequency_response

        # 8. 计算滤波后频谱幅值
        magnitude_spectrum_filtered = np.abs(filtered_fft_result) / N

        magnitude_spectrum_positive_filtered = magnitude_spectrum_filtered[positive_freq_indices] # 正频率部分

        # 9. 执行 IFFT，将滤波后的频域信号转换回时域
        filtered_acc_data = np.fft.ifft(filtered_fft_result).real # 取实部，因为原始信号是实信号

        # --- 绘图 ---
        plt.figure(figsize=(15, 10))

        # 原始频谱
        plt.subplot(3, 1, 1)
        plt.plot(freq_positive, magnitude_spectrum_positive_original)
        plt.title(f'{acc_col} 轴 - 原始加速度频谱 (截止频率: {cutoff_frequency} Hz, 加窗: {use_window})')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值')
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2)
        plt.ylim(bottom=0) # y轴下限为0

        # 滤波后频谱
        plt.subplot(3, 1, 2)
        plt.plot(freq_positive, magnitude_spectrum_positive_filtered)
        plt.title(f'{acc_col} 轴 - 滤波后加速度频谱 (低通, 截止频率: {cutoff_frequency} Hz)')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值')
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2)
        plt.ylim(bottom=0) # y轴下限为0

        # 滤波后的时域信号 (只显示一部分，例如前 1 秒)
        time = np.arange(0, len(filtered_acc_data)) / sampling_frequency
        display_time_seconds = 1 # 显示前 1 秒
        display_samples = int(display_time_seconds * sampling_frequency)

        plt.subplot(3, 1, 3)
        plt.plot(time[:display_samples], filtered_acc_data[:display_samples])
        plt.title(f'{acc_col} 轴 - 滤波后的时域加速度信号 (前 {display_time_seconds} 秒)')
        plt.xlabel('时间 (秒)')
        plt.ylabel('加速度值')
        plt.grid(True)
        plt.xlim(0, display_time_seconds)

        plt.tight_layout() # 调整子图布局，避免重叠
        plt.show()


if __name__ == "__main__":
    csv_file_path = sys.argv[1] #'accelerometer_data.csv' # 替换为你的 CSV 文件路径
    accelerometer_columns = ['accel_x', 'accel_y', 'accel_z','ag'] # 替换为你的加速度数据列名
    sampling_rate = 15.0 # 替换为你的采样频率 (Hz)
    hanning_window_enabled = True # 是否使用汉宁窗
    lowpass_cutoff_freq = 10.0 # 低通滤波器的截止频率，设置为 10Hz

    analyze_frequency_domain_with_lowpass_filter(
        csv_file_path,
        accelerometer_columns,
        sampling_rate,
        cutoff_frequency=lowpass_cutoff_freq,
        use_window=hanning_window_enabled
    )
