#include <stdio.h>
#include <stdlib.h>
#include <fftw3.h>
#include <math.h>

#define SAMPLE_RATE 500.0 // 假设采样率是 100Hz (需要根据你的实际数据调整)
#define CUTOFF_FREQUENCY 40.0 // 假设抖动频率高于 10Hz (需要根据频谱分析调整)

#define MAX_LINE_LENGTH 1000

int main(int argc, char** argv) {
    char line[MAX_LINE_LENGTH];
    // 1. 加载 CSV 数据 (简化示例，假设数据已加载到 input_data 数组)
    int N = 424; // 数据点数量 (需要根据你的实际数据长度调整，最好是 2 的幂)
    double *input_data = (double*) malloc(sizeof(double) * N);
    // ... 这里添加你的 CSV 文件读取代码，将数据填充到 input_data 数组 ...
    //  例如：从文件读取 N 个 double 值到 input_data

    FILE *file = fopen(argv[1], "r");
    if (!file) return -1;

    fgets(line, MAX_LINE_LENGTH, file); // 跳过 CSV 头部

    double default_hight = 0;
    int i = 0;
    while (fgets(line, MAX_LINE_LENGTH, file)) {

        double now, dt, accel, pressure, ag;

#if 0        
        if (sscanf(line, "%lf,%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%*lf,%lf,%*lf,%lf", &now, &dt, &accel, &pressure, &ag) != 5) {
            printf("CSV 解析错误:%s\n", line);
            continue;
        }
#else
        if (sscanf(line, "%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%lf", &now, &accel, &ag) != 3) {
            printf("CSV 解析错误:%s\n", line);
            continue;

        }

        static double prev_time = 0;
        dt = now - prev_time;
        prev_time = now;

         printf("csv now:%.3f dt: %.3f s, accel: %.3f(%.03f), pressure: %.3f Pa\n", 
            now, dt, accel, ag, pressure);

        input_data[i] = ag;
        i++;
        if (i == N) break;

#endif        
    }

    printf("i:%d vs N:%d\n", i, N);
    // 2. 执行 FFT
    fftw_complex *fft_result;
    fftw_plan plan_forward;

    fft_result = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    plan_forward = fftw_plan_dft_r2c_1d(N, input_data, fft_result, FFTW_ESTIMATE);

    fftw_execute(plan_forward);

     for (int i = 0; i < N; i++) {
        char tmp[256] = {0};
        snprintf(tmp, sizeof(tmp), "echo %lf,%lf >> result.fft.2.csv", input_data[i], fft_result[i][0]);
        system(tmp);
    }   
    // 3. 分析频谱 (这里只是打印幅度谱，你需要更详细的分析和可视化)
    printf("Frequency Spectrum (Magnitude):\n");
    for (int i = 0; i < N / 2; i++) { // 只需显示正频率部分
        double freq = (double)i * SAMPLE_RATE / N; // 计算频率
        double magnitude = sqrt(pow(fft_result[i][0], 2) + pow(fft_result[i][1], 2));
        printf("Freq: %.2f Hz, Magnitude: %.4f\n", freq, magnitude);
        char tmp[256] = {0};
        snprintf(tmp, sizeof(tmp), "echo %lf,%lf >> result.fft.csv", freq, magnitude);
        system(tmp);
    }
    //  **关键步骤：分析频谱图，确定抖动频率范围，并调整 CUTOFF_FREQUENCY**

    // 4. 频域滤波 (简单低通滤波器示例)
    for (int i = 0; i < N; i++) {
        double freq = (double)i * SAMPLE_RATE / N;
        if (freq > CUTOFF_FREQUENCY) {
            fft_result[i][0] = 0.0; // 实部置零
            fft_result[i][1] = 0.0; // 虚部置零
        }
    }

    // 5. 执行逆 FFT
    double *filtered_data;
    fftw_plan plan_backward;

    filtered_data = (double*) fftw_malloc(sizeof(double) * N);
    plan_backward = fftw_plan_dft_c2r_1d(N, fft_result, filtered_data, FFTW_ESTIMATE);

    fftw_execute(plan_backward);

    // 6. 保存滤波后的数据 (简化示例，打印到控制台)
    printf("\nFiltered Data:\n");
    for (int i = 0; i < N; i++) {
        // 逆 FFT 的结果需要除以 N 进行归一化
        printf("%.4f ", filtered_data[i] / N);
        //char tmp[256] = {0};
        //snprintf(tmp, sizeof(tmp), "echo %lf,%lf >> result.fft.2.csv", input_data[i], filtered_data[i]);
        //system(tmp);
    }
    printf("\n");
    // ...  你可以添加代码将 filtered_data 保存到 CSV 文件 ...

    // 清理资源
    fftw_destroy_plan(plan_forward);
    fftw_destroy_plan(plan_backward);
    fftw_free(fft_result);
    fftw_free(filtered_data);
    fftw_free(input_data);
    fftw_cleanup();

    return 0;
}
