#!/bin/env python3

# -*- coding: utf-8 -*-


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

def analyze_frequency_domain(csv_file, acc_columns, sampling_frequency, use_window=True):
    """
    分析 CSV 加速度数据的频域信息。

    参数:
    csv_file (str): CSV 文件路径。
    acc_columns (list of str): 包含加速度数据的列名列表，例如 ['acc_x', 'acc_y', 'acc_z']。
    sampling_frequency (float): 采样频率，单位 Hz。

    返回:
    None (显示频谱图)
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
            print(f"警告: 列 '{acc_col}' 数据为空，跳过频域分析。")
            continue

        # 1. 数据预处理 (可选) - 去除均值 (DC 成分)
        acc_data_centered = acc_data - np.mean(acc_data)

        # 2. 数据预处理 - 窗口函数 (汉宁窗) - 可选
        if use_window:
            window = np.hanning(len(acc_data_centered))
            acc_data_windowed = acc_data_centered * window
        else:
            acc_data_windowed = acc_data_centered # 不加窗函数

        # 3. 执行 FFT
        N = len(acc_data_windowed)
        fft_result = np.fft.fft(acc_data_windowed)

        # 4. 计算频谱幅值 (Magnitude Spectrum)
        magnitude_spectrum = np.abs(fft_result) / N  # 除以 N 进行归一化，得到幅值谱

        # 5. 频率轴计算
        freq = np.fft.fftfreq(N, 1/sampling_frequency)

        # 只取正频率部分 (因为实信号的频谱是对称的)
        positive_freq_indices = np.where(freq >= 0)
        freq_positive = freq[positive_freq_indices]
        magnitude_spectrum_positive = magnitude_spectrum[positive_freq_indices]

        # 6. 绘制频谱图
        plt.figure(figsize=(10, 6))
        plt.plot(freq_positive, magnitude_spectrum_positive)
        plt.title(f'{acc_col} 轴加速度频谱')
        plt.xlabel('频率 (Hz)')
        plt.ylabel('幅值') # 可以考虑单位，例如加速度单位
        plt.grid(True)
        plt.xlim(0, sampling_frequency / 2) # 显示到奈奎斯特频率 (采样频率的一半)
        plt.show()

if __name__ == "__main__":
    csv_file_path = sys.argv[1] #'accelerometer_data.csv' # 替换为你的 CSV 文件路径
    accelerometer_columns = ['accel_x', 'accel_y', 'accel_z','ag'] # 替换为你的加速度数据列名
    sampling_rate = 15.0 # 替换为你的采样频率 (Hz)
    use_hanning_window = True # 设置为 True 使用汉宁窗，设置为 False 不使用


    analyze_frequency_domain(csv_file_path, accelerometer_columns, sampling_rate,use_hanning_window)
